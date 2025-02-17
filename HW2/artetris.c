#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <limits.h>

/* 
   Structure representing a block rotation.
   The grid is allocated as a contiguous array of size (blockDim * blockDim),
   where blockDim is provided at runtime.
*/
typedef struct {
    int rows;   /* Number of rows in this rotation */
    int cols;   /* Number of columns in this rotation */
    int alloc_dim;  /* Allocated grid dimension (i.e. block_dim passed during allocation) */
    char *grid; /* Pointer to allocated memory (size = blockDim * blockDim) */
} BlockRotation;

/* 
   Structure representing a block with 4 rotated representations.
*/
typedef struct {
    int num_rotations;       /* Should always be 4 */
    BlockRotation rotations[4];
} Block;

/* 
   Structure representing a tetromino template.
   The field "type" is used to map incoming shape letters.
   The baseShape is stored as a contiguous string in row‐major order.
   Cells that are not part of the piece are given as '.'.
*/
typedef struct {
    char type;
    int baseRows;
    int baseCols;
    const char *baseShape;
} BlockTemplate;

/* 
   Structure to hold the solver's best (maximum-placed) solution.
   Also holds the count of complete solutions.
*/
typedef struct {
    char *bestBoard;      /* Board state (size = boardRows * boardCols) */
    int bestCount;        /* How many pieces were placed in the best solution */
    int boardArea;
    int solutions_count;  /* Number of complete solutions found */
} SolverState;

/* New structure for a placement row (for Algorithm X).
   Each placement row corresponds to a possible placement
   of one piece (using one of its rotations) at a specific (top, left)
   offset. It covers its unique "piece constraint" column plus all the board-cell columns.
*/
typedef struct {
    int pieceIndex;
    int rotation;
    int top;
    int left;
    int count;  /* Number of "1"s in this row (always 1 + number of active cells in the piece) */
    int *cols;  /* Array (of size count) of column indices in the exact cover matrix */
} PlacementRow;

/* Function prototypes for canvas functions */
void initialize_canvas(char *grid, int rows, int cols);
void print_canvas(const char *grid, int rows, int cols);
int verify_canvas(const char *grid, int rows, int cols);

/* Function prototypes for block functions */
int allocate_rotation(BlockRotation *rotation, int block_dim);
int rotate_block_90(const BlockRotation *src, BlockRotation *dest, int block_dim, int debug);
int initialize_block(Block *block, const char *init_grid, int init_rows, int init_cols, int block_dim);
void print_block(const Block *block, int block_dim);
void free_block(Block *block);

/* Solver helper function prototypes */
int canPlacePiece(char *board, int boardRows, int boardCols, BlockRotation *piece, int block_dim, int top, int left);
void placePiece(char *board, int boardRows, int boardCols, BlockRotation *piece, int block_dim, int top, int left, char letter);
void removePiece(char *board, int boardRows, int boardCols, BlockRotation *piece, int block_dim, int top, int left);
void solvePuzzle(char *board, int boardRows, int boardCols, Block *blocks, int nblocks, int curIndex, int placedCount, SolverState *state);
void solvePuzzleDLX(int boardRows, int boardCols, Block *blocks, int nblocks, SolverState *state, FILE* log);
PlacementRow *buildPlacementMatrix(int boardRows, int boardCols, Block *blocks, int nblocks, int *outRowsCount, FILE* log);
void freePlacementMatrix(PlacementRow *matrix, int rowCount);
int row_conflicts(PlacementRow *r1, PlacementRow *r2);
int algorithm_x(int total_columns, int total_rows, PlacementRow *matrix, 
                int *active, int *col_covered, int covered_count,
                int *solution, int sol_depth, int nblocks,
                int *solution_count, int *best_solution, int *best_depth,
                clock_t start_time, int time_limit, int boardRows, 
                int boardCols, Block *blocks, FILE* log, int *timed_out);

/* Add blocksToString prototype */
char* blocksToString(Block *blocks, int nblocks);

/* Add print_current_board prototype */
void print_current_board(int boardRows, int boardCols, PlacementRow *matrix, int *solution, int sol_depth, Block *blocks, FILE* log);

/* Helper function: returns 1 if the board is fully covered (no '-' empty cells), 0 otherwise */
int is_board_full(const char *board, int boardArea) {
    int i;
    for (i = 0; i < boardArea; i++) {
        if (board[i] == '-')
            return 0;
    }
    return 1;
}

/* Revised print_solution function */
void print_solution(int *solution, int sol_depth, PlacementRow *matrix, 
                   Block *blocks, int boardRows, int boardCols) {
    char *board = calloc(boardRows * boardCols + 1, 1);
    int s, k, b, r, c;  /* Declare all loop variables at start */
    int idx, pos;
    char letter;
    
    if (!board) return;
    
    memset(board, '-', boardRows * boardCols);
    
    for (s = 0; s < sol_depth; s++) {
        idx = solution[s];
        letter = blocks[matrix[idx].pieceIndex].rotations[0].grid[0];
        
        for (k = 0; k < blocks[matrix[idx].pieceIndex].rotations[matrix[idx].rotation].rows; k++) {
            for (b = 0; b < blocks[matrix[idx].pieceIndex].rotations[matrix[idx].rotation].cols; b++) {
                if (blocks[matrix[idx].pieceIndex].rotations[matrix[idx].rotation]
                    .grid[k * blocks[matrix[idx].pieceIndex].rotations[matrix[idx].rotation].alloc_dim + b] != '.') {
                    pos = (matrix[idx].top + k) * boardCols + (matrix[idx].left + b);
                    board[pos] = letter;
                }
            }
        }
    }
    
    for (r = 0; r < boardRows; r++) {
        for (c = 0; c < boardCols; c++) {
            printf("%c", board[r * boardCols + c]);
        }
        printf("\n");
    }
    
    free(board);
}

/* Main function */
int main(int argc, char **argv) {
    int boardRows, boardCols, canvasArea;
    int i, j, ret, nblocks, activeArea;  /* Removed unused variable 'area' */
    char *canvas;
    Block *blocks;
    char *pieceSeq;
    int lenSeq;
    int temp;
    int curBlockDim;
    SolverState state;  /* Moved declaration to start of block */

    /* We now require three command-line arguments:
       1. board_rows
       2. board_cols
       3. a sequence of shape letters (each letter corresponds to a tetromino)
    */
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <board_rows> <board_cols> <piece_sequence>\n", argv[0]);
        return -1;
    }
    boardRows = atoi(argv[1]);
    boardCols = atoi(argv[2]);
    if (boardRows <= 0 || boardCols <= 0) {
        fprintf(stderr, "Invalid board dimensions.\n");
        return -1;
    }
    canvasArea = boardRows * boardCols;
    
    pieceSeq = argv[3];
    lenSeq = (int)strlen(pieceSeq);
    nblocks = lenSeq;
    if (nblocks <= 0) {
        fprintf(stderr, "No pieces provided.\n");
        return -1;
    }
    
    /* Dynamically allocate the canvas (1D array of size boardArea) */
    canvas = (char *)malloc(canvasArea * sizeof(char));
    if (canvas == NULL) {
        fprintf(stderr, "Error: Failed to allocate canvas memory.\n");
        return -1;
    }
    initialize_canvas(canvas, boardRows, boardCols);
    ret = verify_canvas(canvas, boardRows, boardCols);
    if (!ret) {
        fprintf(stderr, "Error: Canvas not initialized correctly.\n");
        free(canvas);
        return -1;
    }
    printf("Initial Canvas:\n");
    print_canvas(canvas, boardRows, boardCols);
    printf("\n");
    
    {
        static const BlockTemplate availableTemplates[] = {
            /* Vertical line of 6 */
            { 'A', 6, 1, 
                "A"    /* Row 1 */
                "A"    /* Row 2 */
                "A"    /* Row 3 */
                "A"    /* Row 4 */
                "A"    /* Row 5 */
                "A" }, /* Row 6 */
            
            /* Staggered shape */
            { 'C', 3, 3,
                "CC."  /* C C - */
                ".CC"  /* - C C */
                ".C." },/* - C - */
            
            /* Modified L-shape */
            { 'D', 4, 2,
                ".D"   /* - D */
                ".D"   /* - D */
                ".D"   /* - D */
                "DD" }, /* D D */
            
            /* Modified Z-shape */
            { 'F', 3, 2,
                "FF"   /* F F */
                "FF"   /* F F */
                ".F" }, /* - F */
            
            /* Modified L-shape */
            { 'I', 4, 2,
                "I."   /* I - */
                "I."   /* I - */
                "II"   /* I I */
                ".I" }, /* - I */
            
            /* T with extended stem */
            { 'J', 3, 3,
                "JJJ"  /* J J J */
                ".J."  /* - J - */
                ".J." },/* - J - */
            
            /* U-shape */
            { 'K', 2, 3,
                "K.K"  /* K - K */
                "KKK" },/* K K K */
            
            /* L-shape */
            { 'L', 3, 3,
                "..L"  /* - - L */
                "..L"  /* - - L */
                "LLL" },/* L L L */
            
            /* Staggered shape */
            { 'M', 3, 3,
                "..M"  /* - - M */
                ".MM"  /* - M M */
                "MM." },/* M M - */
            
            /* Plus shape */
            { 'N', 3, 3,
                ".N."  /* - N - */
                "NNN"  /* N N N */
                ".N." },/* - N - */
            
            /* Modified T-shape */
            { 'O', 4, 2,
                "O."   /* O - */
                "OO"   /* O O */
                "O."   /* O - */
                "O." }, /* O - */
            
            /* Zigzag shape */
            { 'Q', 3, 3,
                ".QQ"  /* - Q Q */
                ".Q."  /* - Q - */
                "QQ." } /* Q Q - */
        };
        const int numTemplates = (int)(sizeof(availableTemplates) / sizeof(availableTemplates[0]));
        
        /* Allocate the blocks array (one block per letter in the sequence) */
        blocks = (Block *)malloc(nblocks * sizeof(Block));
        if (blocks == NULL) {
            fprintf(stderr, "Error: Failed to allocate blocks array.\n");
            free(canvas);
            return -1;
        }
        
        activeArea = 0;
        
        /* For each letter in the input sequence, select the corresponding template */
        for (i = 0; i < nblocks; i++) {
            char curType;
            int found = 0;
            BlockTemplate t;
            const BlockTemplate *ptemplate = NULL;
            int cells;
            int r, c;
            int j; /* Loop variable */
            BlockRotation *rot;
            
            curType = pieceSeq[i];
            for (j = 0; j < numTemplates; j++) {
                if (availableTemplates[j].type == curType) {
                    ptemplate = &availableTemplates[j];
                    found = 1;
                    break;
                }
            }
            if (!found) {
                fprintf(stderr, "Error: Unknown piece type '%c' at position %d.\n", curType, i);
                /* Free previously allocated blocks (their rotations) */
                for (j = 0; j < i; j++) {
                    free_block(&blocks[j]);
                }
                free(blocks);
                free(canvas);
                return -1;
            }
            t = *ptemplate;
            /* Compute grid size = baseRows * baseCols for the template */
            temp = t.baseRows * t.baseCols;
            /* Allocate a temporary buffer to hold the piece's base representation.
                For each cell, if the template cell is not '.', we override it with the
                input letter (to mark this instance), else we keep '.'
            */
            {
                char *buffer;
                buffer = (char *)malloc(temp * sizeof(char));
                if (buffer == NULL) {
                    fprintf(stderr, "Error: Failed to allocate temporary buffer for piece %d.\n", i);
                    for (j = 0; j < i; j++) {
                        free_block(&blocks[j]);
                    }
                    free(blocks);
                    free(canvas);
                    return -1;
                }
                for (j = 0; j < temp; j++) {
                    if (t.baseShape[j] != '.')
                        buffer[j] = curType;
                    else
                        buffer[j] = '.';
                }
                /* The allocated block dimension is the maximum of baseRows and baseCols */
                curBlockDim = (t.baseRows > t.baseCols) ? t.baseRows : t.baseCols;
                ret = initialize_block(&blocks[i], buffer, t.baseRows, t.baseCols, curBlockDim);
                free(buffer);
                if (ret != 0) {
                    fprintf(stderr, "Error: Failed to initialize piece %d.\n", i);
                    for (j = 0; j < i; j++) {
                        free_block(&blocks[j]);
                    }
                    free(blocks);
                    free(canvas);
                    return -1;
                }
                /* Calculate active cells in base rotation */
                cells = 0;
                rot = &blocks[i].rotations[0];
                for (r = 0; r < rot->rows; r++) {
                    for (c = 0; c < rot->cols; c++) {
                        if (rot->grid[r * rot->alloc_dim + c] != '.') {
                            cells++;
                        }
                    }
                }
                activeArea += cells;
            }
        }
        state.boardArea = canvasArea;
        state.bestBoard = (char *)malloc(canvasArea * sizeof(char));
        if (state.bestBoard == NULL) {
            fprintf(stderr, "Error: Failed to allocate memory for bestBoard.\n");
            for (j = 0; j < nblocks; j++) {
                free_block(&blocks[j]);
            }
            free(blocks);
            free(canvas);
            return -1;
        }
        state.bestCount = -1;
        state.solutions_count = 0;
        solvePuzzleDLX(boardRows, boardCols, blocks, nblocks, &state, stderr);
        
        if (state.solutions_count > 0) {
            printf("Found %d solutions, best solution has %d pieces.\n", 
                  state.solutions_count, state.bestCount);
            printf("Best Solution:\n");
            print_canvas(state.bestBoard, boardRows, boardCols);
        } else {
            printf("No valid tiling found for this configuration\n");
        }
        free(state.bestBoard);
    }

    /* Free allocated memory */
    for (i = 0; i < nblocks; i++) {
        free_block(&blocks[i]);
    }
    free(blocks);
    free(canvas);

    return 0;
}

/*---------------------------------------------------------------------------
   Canvas functions operating on a dynamically allocated 1D array.
   Indexing is done via: grid[i * cols + j].
---------------------------------------------------------------------------*/
void initialize_canvas(char *grid, int rows, int cols) {
    int i, j;
    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            grid[i * cols + j] = '-';
        }
    }
}

void print_canvas(const char *grid, int rows, int cols) {
    int i, j;
    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            printf("%c", grid[i * cols + j]);
        }
        printf("\n");
    }
}

int verify_canvas(const char *grid, int rows, int cols) {
    int i, j;
    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            if (grid[i * cols + j] != '-')
                return 0;
        }
    }
    return 1;
}

/*---------------------------------------------------------------------------
   Block functions
---------------------------------------------------------------------------*/
int allocate_rotation(BlockRotation *rotation, int block_dim) {
    int i;
    if (rotation == NULL)
        return -1;
    rotation->grid = (char *)malloc(block_dim * block_dim * sizeof(char));
    if (rotation->grid == NULL)
        return -1;
    for (i = 0; i < block_dim * block_dim; i++) {
        rotation->grid[i] = '.';
    }
    rotation->alloc_dim = block_dim;  /* Store the allocated grid dimension */
    return 0;
}

int rotate_block_90(const BlockRotation *src, BlockRotation *dest, int block_dim, int debug) {
    int i, j;
    
    if (!src || !dest || src->rows <= 0 || src->cols <= 0) {
        return -1;
    }
    
    if (src == NULL || dest == NULL)
        return -1;
    if (src->rows > 6 || src->cols > 6) {
        fprintf(stderr, "[ROTATION] Rejecting %dx%d rotation\n",
               src->rows, src->cols);
        return -1;
    }
    dest->rows = src->cols;
    dest->cols = src->rows;
    
    dest->alloc_dim = block_dim; /* Use the passed-in block_dim */
    
    dest->grid = (char *)malloc(dest->alloc_dim * dest->alloc_dim);
    if (!dest->grid) {
        exit(EXIT_FAILURE); /* No point continuing if allocation fails */
    }

    if (debug) { /* Conditional debugging */
        printf("Rotating block (block_dim = %d):\n", block_dim);
        printf("Source (%dx%d):\n", src->rows, src->cols);
        for (i = 0; i < src->rows; i++) {
            for (j = 0; j < src->cols; j++) {
                printf("%c", src->grid[i * block_dim + j]);
            }
            printf("\n");
        }
    }

    for (i = 0; i < src->rows; i++) {
        for (j = 0; j < src->cols; j++) {
            int original_row = src->rows - 1 - j;
            int original_col = i;
            dest->grid[i * block_dim + j] = src->grid[original_row * block_dim + original_col];
        }
    }

    if (debug) { /* Conditional debugging */
        printf("Destination (%dx%d):\n", dest->rows, dest->cols);
        for (i = 0; i < dest->rows; i++) {
            for (j = 0; j < dest->cols; j++) {
                printf("%c", dest->grid[i * dest->alloc_dim + j]);
            }
            printf("\n");
        }
        printf("----\n");
    }

    return 0;
}

int initialize_block(Block *block, const char *init_grid, int init_rows, int init_cols, int block_dim) {
    int i, c;
    if (block == NULL)
        return -1;
    block->num_rotations = 4;
    /* Allocate and initialize rotation 0 */
    if (allocate_rotation(&block->rotations[0], block_dim) != 0)
        return -1;
    block->rotations[0].rows = init_rows;
    block->rotations[0].cols = init_cols;
    for (i = 0; i < block_dim * block_dim; i++) {
        block->rotations[0].grid[i] = '.';
    }
    for (i = 0; i < init_rows; i++) {
        for (c = 0; c < init_cols; c++) {
            /* Fixed index calculation */
            int target_pos = i * block_dim + c;
            int source_pos = i * init_cols + c;
            if (init_grid[source_pos] != '.')
                block->rotations[0].grid[target_pos] = init_grid[source_pos];
        }
    }
    /* Build the other rotations */
    for (i = 1; i < 4; i++) {
        if (allocate_rotation(&block->rotations[i], block_dim) != 0)
            return -1;
        block->rotations[i].rows = block->rotations[i-1].cols;
        block->rotations[i].cols = block->rotations[i-1].rows;
        rotate_block_90(&block->rotations[i-1], &block->rotations[i], block_dim, 0);
    }
    return 0;
}

void print_block(const Block *block, int block_dim) {
    int i, r, c;
    if (block == NULL)
        return;
    printf("Block with %d rotations:\n", block->num_rotations);
    for (i = 0; i < block->num_rotations; i++) {
        printf("Rotation %d (%dx%d):\n", i, block->rotations[i].rows, block->rotations[i].cols);
        for (r = 0; r < block->rotations[i].rows; r++) {
            for (c = 0; c < block->rotations[i].cols; c++) {
                printf("%c", block->rotations[i].grid[r * block_dim + c]);
            }
            printf("\n");
        }
        printf("\n");
    }
}

void free_block(Block *block) {
    int i;
    if (block == NULL)
        return;
    for (i = 0; i < block->num_rotations; i++) {
        if (block->rotations[i].grid != NULL) {
            free(block->rotations[i].grid);
            block->rotations[i].grid = NULL;
        }
    }
}

/*---------------------------------------------------------------------------
   Solver helper functions
---------------------------------------------------------------------------*/

/* Added helper function to get rotation dimensions. */
static void get_rotation_dimensions(Block *blocks, int pieceIndex, int rotation, int *pRows, int *pCols) {
    /* Validate pointers and assign the dimensions from the specified rotation */
    if (!blocks || !pRows || !pCols) return;
    *pRows = blocks[pieceIndex].rotations[rotation].rows;
    *pCols = blocks[pieceIndex].rotations[rotation].cols;
}

/* Build the sparse matrix of placements. For each piece instance ... */
PlacementRow *buildPlacementMatrix(int boardRows, int boardCols, Block *blocks, int nblocks, int *outRowsCount, FILE* log) {
    int i, r, top, left, idx, colIndex, j, row, col;
    int pRows, pCols;
    int totalRows = 0;  /* Actual valid count */
    PlacementRow *matrix;

    /* Avoid unused parameter warning */
    (void)log;

    /* Phase 1: Calculate ACTUAL valid placements */
    for (i = 0; i < nblocks; i++) {
        for (r = 0; r < 4; r++) {
            get_rotation_dimensions(blocks, i, r, &pRows, &pCols);
            if (pRows > boardRows || pCols > boardCols)
                continue;
            totalRows += (boardRows - pRows + 1) * (boardCols - pCols + 1);
        }
    }

    matrix = malloc(totalRows * sizeof(PlacementRow));
    if (!matrix) {
        fprintf(stderr, "Error: Failed to allocate placement matrix.\n");
        exit(EXIT_FAILURE);
    }

    idx = 0;
    for (i = 0; i < nblocks; i++) {
        for (r = 0; r < 4; r++) {
            get_rotation_dimensions(blocks, i, r, &pRows, &pCols);
            if (pRows > boardRows || pCols > boardCols)
                continue;
            for (top = 0; top <= boardRows - pRows; top++) {
                for (left = 0; left <= boardCols - pCols; left++) {
                    int activeCells;  /* Declaration moved to beginning of block */
                    activeCells = 0;
                    matrix[idx].pieceIndex = i;
                    matrix[idx].rotation = r;
                    matrix[idx].top = top;
                    matrix[idx].left = left;

                    /* Count the number of active (non-'.') cells in the piece's grid */
                    for (row = 0; row < pRows; row++) {
                        for (col = 0; col < pCols; col++) {
                            if (blocks[i].rotations[r].grid[row * blocks[i].rotations[r].alloc_dim + col] != '.')
                                activeCells++;
                        }
                    }
                    /* Add one extra for the piece constraint column */
                    activeCells++;
                    matrix[idx].count = activeCells;
                    matrix[idx].cols = malloc(activeCells * sizeof(int));
                    if (!matrix[idx].cols) {
                        fprintf(stderr, "Error: Failed to allocate cols for placement row %d\n", idx);
                        for (j = 0; j < idx; j++)
                            free(matrix[j].cols);
                        free(matrix);
                        return NULL;
                    }
                    colIndex = 0;
                    for (row = 0; row < pRows; row++) {
                        for (col = 0; col < pCols; col++) {
                            if (blocks[i].rotations[r].grid[row * blocks[i].rotations[r].alloc_dim + col] == '.')
                                continue;
                            matrix[idx].cols[colIndex++] = (top + row) * boardCols + (left + col);
                        }
                    }
                    /* Append the extra piece constraint column */
                    matrix[idx].cols[colIndex++] = boardRows * boardCols + i;
                    idx++;
                }
            }
        }
    }

    *outRowsCount = idx;  /* Return actual initialized rows */
    return matrix;
}

/* Check if two placement rows conflict (i.e. if they share any column) */
int row_conflicts(PlacementRow *r1, PlacementRow *r2) {
    int i, j;
    for (i = 0; i < r1->count; i++) { 
        for (j = 0; j < r2->count; j++) {
            if (r1->cols[i] == r2->cols[j])
                return 1;
        }
    }
    return 0;
}

/* Free the memory allocated for the placement matrix */
void freePlacementMatrix(PlacementRow *matrix, int rowCount) {
    int i;
    for (i = 0; i < rowCount; i++) {
        free(matrix[i].cols);
    }
    free(matrix);
}

/* Revised algorithm_x function */
int algorithm_x(int total_columns, int total_rows, PlacementRow *matrix, 
                int *active, int *col_covered, int covered_count,
                int *solution, int sol_depth, int nblocks,
                int *solution_count, int *best_solution, int *best_depth,
                clock_t start_time, int time_limit, int boardRows, 
                int boardCols, Block *blocks, FILE* log, int *timed_out) {
    int r, k, j;
    int *active_copy, *col_covered_copy;
    int new_covered;
    int col;  /* Moved declaration to the start of the block */

    if (sol_depth >= nblocks) {
        if (covered_count == total_columns) { /* Check for full coverage only when depth is reached */
            (*solution_count)++;
            if (sol_depth > *best_depth) {
                *best_depth = sol_depth;
                memcpy(best_solution, solution, sol_depth * sizeof(int));
            }
            return 1;
        }
        return 0; /* Stop recursion when depth limit is reached */
    }

    for (r = 0; r < total_rows; r++) {
        if (!active[r]) 
            continue;
        
        if (r >= total_rows) {  /* Row index validation */
            fprintf(stderr, "[CRITICAL] Invalid row index %d/%d\n", r, total_rows);
            continue;
        }
        
        if (matrix[r].cols == NULL) {
            continue; /* Skip invalid rows */
        }
        
        active_copy = malloc(total_rows * sizeof(int));
        col_covered_copy = malloc(total_columns * sizeof(int));
        if (!active_copy || !col_covered_copy) {
            free(active_copy);
            free(col_covered_copy);
            return 0;
        }
        
        memcpy(active_copy, active, total_rows * sizeof(int));
        memcpy(col_covered_copy, col_covered, total_columns * sizeof(int));
        
        new_covered = covered_count;
        for (k = 0; k < matrix[r].count; k++) {
            col = matrix[r].cols[k];
            if (!col_covered_copy[col]) {
                new_covered++;
                col_covered_copy[col] = 1;
            }
        }
        
        for (j = 0; j < total_rows; j++) {
            if (active_copy[j] && j != r) {
                if (row_conflicts(&matrix[r], &matrix[j]))
                    active_copy[j] = 0;
            }
        }
        active_copy[r] = 0;
        solution[sol_depth] = r;

        if ((clock() - start_time) * 1000 / CLOCKS_PER_SEC > time_limit) {
            *timed_out = 1;
            free(active_copy);
            free(col_covered_copy);
            return 0;  /* Abort search */
        }

        algorithm_x(total_columns, total_rows, matrix, active_copy, col_covered_copy,
                    new_covered, solution, sol_depth + 1, nblocks, solution_count,
                    best_solution, best_depth, start_time, time_limit, boardRows, 
                    boardCols, blocks, log, timed_out);

        free(active_copy);
        free(col_covered_copy);
    }

    if (log) {
        if (*solution_count % 100 == 0) {
            int active_count = 0;
            for (j = 0; j < total_rows; j++) {
                if (active[j])
                    active_count++;
            }

            print_current_board(boardRows, boardCols, matrix, solution, sol_depth, blocks, log);
            fprintf(log, "Depth: %d, Candidates: %d\n", sol_depth, active_count);
        }
    }

    return 0;
}

/* In solvePuzzleDLX, we update the call to algorithm_x with the new parameters. */
void solvePuzzleDLX(int boardRows, int boardCols, Block *blocks, int nblocks, SolverState *state, FILE* log) { /* Add debug flag, and remove unused parameter */
    PlacementRow *matrix;
    int *active, *col_covered, *solution, *best_solution;
    int rowCount, total_columns, i;
    clock_t start_time;
    int time_limit = 100; /* seconds */
    int timed_out = 0;  /* New timeout flag */
    double elapsed;  /* Declaration moved to top of function */
    FILE *summary;  /* Declaration moved to top of function */

    if (log) {
        fprintf(log, "Starting DLX solver\n");
    }
    matrix = buildPlacementMatrix(boardRows, boardCols, blocks, nblocks, &rowCount, log); /* Pass debug flag */
    if (matrix == NULL) {
        return;
    }
    total_columns = boardRows * boardCols + nblocks; /* Board cells + piece constraints */
    active = (int *)malloc(rowCount * sizeof(int));
    col_covered = (int *)malloc(total_columns * sizeof(int));
    solution = (int *)malloc(nblocks * sizeof(int));
    best_solution = (int *)malloc(nblocks * sizeof(int));
    if (active == NULL || col_covered == NULL || solution == NULL || best_solution == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory in solvePuzzleDLX.\n");
        freePlacementMatrix(matrix, rowCount);
        return;
    }
    for (i = 0; i < rowCount; i++) {
        active[i] = 1;
    }
    for (i = 0; i < total_columns; i++) {
        col_covered[i] = 0;
    }
    start_time = clock();
    if (log) {
        fprintf(log, "Starting DLX solver (time limit: %ds)\n", time_limit);
        fprintf(log, "Initial matrix: %d rows, %d columns\n", rowCount, total_columns);
    }
    algorithm_x(total_columns, rowCount, matrix, active, col_covered, 0, solution, 0, nblocks,
                &state->solutions_count, best_solution, &state->bestCount, start_time, time_limit, boardRows, boardCols, blocks, log, &timed_out); /* Pass debug flag */

    /* Copy the best solution to the state */
    if (state->bestCount > 0) {
        for (i = 0; i < state->bestCount; i++) {
            int idx, top, left, pRows, pCols, alloc_dim;
            char letter;
            int k, b, pos;

            idx = best_solution[i];
            top = matrix[idx].top;
            left = matrix[idx].left;
            pRows = blocks[matrix[idx].pieceIndex].rotations[matrix[idx].rotation].rows;
            pCols = blocks[matrix[idx].pieceIndex].rotations[matrix[idx].rotation].cols;
            alloc_dim = blocks[matrix[idx].pieceIndex].rotations[matrix[idx].rotation].alloc_dim;
            letter = blocks[matrix[idx].pieceIndex].rotations[0].grid[0];

            for (k = 0; k < pRows; k++) {
                for (b = 0; b < pCols; b++) {
                    if (blocks[matrix[idx].pieceIndex].rotations[matrix[idx].rotation].grid[k * alloc_dim + b] != '.') {
                        pos = (top + k) * boardCols + (left + b);
                        state->bestBoard[pos] = letter;
                    }
                }
            }
        }
    }

    /* Final summary - ALWAYS WRITE */
    elapsed = (clock() - start_time) * 1000.0 / CLOCKS_PER_SEC;
    summary = fopen("solver_summary.txt", "a"); /* Changed from debug_log to always create */
    if (summary) {
        fprintf(summary, "[%dx%d] %s | Solutions: %d | Best: %d | Time: %.2fms%s\n",
                boardRows, boardCols, blocksToString(blocks, nblocks), 
                state->solutions_count, state->bestCount, elapsed,
                timed_out ? " (Timeout)" : "");  /* Add timeout indicator */
        fclose(summary);
    }

    fprintf(stderr, "[DEBUG] Solver completed. Best solution: %d pieces\n", 
           state->bestCount);

    free(active);
    free(col_covered);
    free(solution);
    free(best_solution);
    /* Log the placement matrix details before freeing it */
    if (log) {
        int i, c;
        fprintf(log, "Built placement matrix with %d rows\n", rowCount);
        for (i = 0; i < rowCount; i++) {
            fprintf(log, "Row %d: Piece %c Rot %d @ (%d,%d) covers cols: ",
                    i, 
                    blocks[matrix[i].pieceIndex].rotations[0].grid[0],
                    matrix[i].rotation,
                    matrix[i].top,
                    matrix[i].left);
            for (c = 0; c < matrix[i].count; c++) {
                fprintf(log, "%d ", matrix[i].cols[c]);
            }
            fprintf(log, "\n");
        }
    }
    freePlacementMatrix(matrix, rowCount);
}

char* blocksToString(Block *blocks, int nblocks) {
    static char buf[256];
    int i;
    if (!blocks || nblocks < 1) return "invalid";
    for(i=0; i<nblocks && i<255; i++)
        buf[i] = blocks[i].rotations[0].grid[0];
    buf[nblocks] = '\0';
    return buf;
}

void print_current_board(int boardRows, int boardCols, PlacementRow *matrix, int *solution, int sol_depth, Block *blocks, FILE* log) {
    char *board = malloc(boardRows * boardCols);
    int s, k, b, r, c;  /* Declare all loop variables at start */
    int idx, pos;
    char letter;
    
    memset(board, '-', boardRows * boardCols);
    
    for (s = 0; s < sol_depth; s++) {
        idx = solution[s];
        letter = blocks[matrix[idx].pieceIndex].rotations[0].grid[0];
        
        for (k = 0; k < blocks[matrix[idx].pieceIndex].rotations[matrix[idx].rotation].rows; k++) {
            for (b = 0; b < blocks[matrix[idx].pieceIndex].rotations[matrix[idx].rotation].cols; b++) {
                if (blocks[matrix[idx].pieceIndex].rotations[matrix[idx].rotation].grid[k * blocks[matrix[idx].pieceIndex].rotations[matrix[idx].rotation].alloc_dim + b] != '.') {
                    pos = (matrix[idx].top + k) * boardCols + (matrix[idx].left + b);
                    board[pos] = letter;
                }
            }
        }
    }
    
    if (log) {
        fprintf(log, "Current board state:\n");
        for (r = 0; r < boardRows; r++) {
            for (c = 0; c < boardCols; c++) {
                fprintf(log, "%c", board[r * boardCols + c]);
            }
            fprintf(log, "\n");
        }
    }
    
    free(board);
}