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
void solvePuzzleDLX(int boardRows, int boardCols, Block *blocks, int nblocks, char *unused_canvas, SolverState *state, int debug);
PlacementRow *buildPlacementMatrix(int boardRows, int boardCols, Block *blocks, int nblocks, int *outRowsCount, int debug);
void freePlacementMatrix(PlacementRow *matrix, int rowCount);
int row_conflicts(PlacementRow *r1, PlacementRow *r2);
void algorithm_x(int total_columns, int total_rows, PlacementRow *matrix, int *active,
                 int *col_covered, int covered_count, int *solution, int sol_depth, int nblocks,
                 int *solution_count, int *best_solution, int *best_depth,
                 clock_t start_time, int time_limit, int boardRows, int boardCols, Block *blocks, int debug);

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
void print_solution(int *solution, int sol_depth, PlacementRow *matrix, Block *blocks, int boardRows, int boardCols) {
    int boardArea;
    char *solBoard;
    int i, s;

    boardArea = boardRows * boardCols;
    solBoard = malloc(boardArea * sizeof(char));
    if (solBoard == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for printing solution.\n");
        return;
    }
    for (i = 0; i < boardArea; i++) {
        solBoard[i] = '-';
    }
    for (s = 0; s < sol_depth; s++) {
        int idx, top, left, pRows, pCols, alloc_dim, blockDim;
        char letter;
        int k, b, pos;

        idx = solution[s];
        top = matrix[idx].top;
        left = matrix[idx].left;
        pRows = blocks[matrix[idx].pieceIndex].rotations[matrix[idx].rotation].rows;
        pCols = blocks[matrix[idx].pieceIndex].rotations[matrix[idx].rotation].cols;
        alloc_dim = blocks[matrix[idx].pieceIndex].rotations[matrix[idx].rotation].alloc_dim;
        blockDim = alloc_dim;
        letter = blocks[matrix[idx].pieceIndex].rotations[0].grid[0];

        for (k = 0; k < pRows; k++) {
            for (b = 0; b < pCols; b++) {
                if (blocks[matrix[idx].pieceIndex].rotations[matrix[idx].rotation].grid[k * blockDim + b] != '.') {
                    pos = (top + k) * boardCols + (left + b);
                    solBoard[pos] = letter;
                }
            }
        }
    }
    printf("Solution found:\n");
    print_canvas(solBoard, boardRows, boardCols);
    printf("Solution piece types: ");
    for (s = 0; s < sol_depth; s++) {
        int idx;
        char letter;
        idx = solution[s];
        letter = blocks[matrix[idx].pieceIndex].rotations[0].grid[0];
        printf("%c ", letter);
    }
    printf("\n\n");
    free(solBoard);
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
    int debug_case_7; /* Declare BEFORE any executable statements */

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
    
    debug_case_7 = (boardRows == 4 && boardCols == 4 && strcmp(pieceSeq, "JLSO") == 0); /* Now an executable statement */

    /* Define a pool of tetromino templates.
       The patterns are given in minimal bounding boxes in row-major order.
       Cells that are not part of the piece are represented with '.'.
       Areas:
         I: 1x4 ("IIII")       (area = 4)
         O: 2x2 ("OOOO")       (area = 4)
         T: 2x3 ("TTT.T.")     (area = 4) [row0: TTT, row1: .T.]
         S: 2x3 (".SS" "SS.")   (area = 4)
         Z: 2x3 ("ZZ." ".ZZ")   (area = 4)
         J: 2x3 ("J.." "JJJ")   (area = 4)
         L: 2x3 ("..L" "LLL")   (area = 4)
    */
    {
        static const BlockTemplate availableTemplates[] = {
            { 'I', 1, 4, "IIII" },
            { 'O', 2, 2, "OOOO" },
            { 'T', 2, 3, "TTT.T." },
            { 'S', 2, 3, ".SSSS." },  /* row0: ".SS", row1: "SS." */
            { 'Z', 2, 3, "ZZ..ZZ" },  /* row0: "ZZ.", row1: ".ZZ" */
            { 'J', 2, 3, "J..JJJ" },  /* row0: "J..", row1: "JJJ" */
            { 'L', 2, 3, "..LLL" }    /* row0: "..L", row1: "LLL" */
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
                activeArea += 4;
            }
        }
        if (activeArea < canvasArea) {
            printf("The provided pieces cannot fill the canvas.\n");
        } else if (activeArea == canvasArea) {
            SolverState state;
            int debug = 0;  /* Default: no debugging */

            if (debug_case_7) {
                debug = 1; /* Enable debugging for Case 7 */
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
            solvePuzzleDLX(boardRows, boardCols, blocks, nblocks, canvas, &state, debug);
            if (state.solutions_count > 0) {
                printf("Found %d solutions, best solution has %d pieces.\n", state.solutions_count, state.bestCount);
                printf("Best Solution:\n");
                print_canvas(state.bestBoard, boardRows, boardCols);
            } else {
                printf("No solutions found.\n");
            }
            free(state.bestBoard);
        } else {
            printf("Too many pieces for the given canvas size.\n");
        }
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

int rotate_block_90(const BlockRotation *src, BlockRotation *dest, int block_dim, int debug) { /* Add debug flag */
    int i, j;
    if (src == NULL || dest == NULL)
        return -1;
    dest->rows = src->cols;
    dest->cols = src->rows;
    dest->alloc_dim = block_dim;

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

    for (i = 0; i < dest->rows; i++) {
        for (j = 0; j < dest->cols; j++) {
            dest->grid[i * block_dim + j] = src->grid[j * block_dim + (src->rows - 1 - i)];
        }
    }

    if (debug) { /* Conditional debugging */
        printf("Destination (%dx%d):\n", dest->rows, dest->cols);
        for (i = 0; i < dest->rows; i++) {
            for (j = 0; j < dest->cols; j++) {
                printf("%c", dest->grid[i * block_dim + j]);
            }
            printf("\n");
        }
        printf("----\n");
    }

    return 0;
}

int initialize_block(Block *block, const char *init_grid, int init_rows, int init_cols, int block_dim) {
    int i;
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
    {
        int r, c;
        for (r = 0; r < init_rows; r++) {
            for (c = 0; c < init_cols; c++) {
                int pos = r * block_dim + c;
                if (init_grid[r * init_cols + c] != '.')
                    block->rotations[0].grid[pos] = init_grid[r * init_cols + c];
            }
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
int canPlacePiece(char *board, int boardRows, int boardCols, BlockRotation *piece, int block_dim, int top, int left) {
    int i, j;
    (void)boardRows;
    for (i = 0; i < piece->rows; i++) {
        for (j = 0; j < piece->cols; j++) {
            if (piece->grid[i * block_dim + j] != '.') {
                int boardRow = top + i;
                int boardCol = left + j;
                if (boardRow < 0 || boardRow >= boardRows || boardCol < 0 || boardCol >= boardCols)
                    return 0;
                if (board[boardRow * boardCols + boardCol] != '-')
                    return 0;
            }
        }
    }
    return 1;
}

void placePiece(char *board, int boardRows, int boardCols, BlockRotation *piece, int block_dim, int top, int left, char letter) {
    int i, j;
    (void)boardRows;
    for (i = 0; i < piece->rows; i++) {
        for (j = 0; j < piece->cols; j++) {
            if (piece->grid[i * block_dim + j] != '.') {
                board[(top + i) * boardCols + (left + j)] = letter;
            }
        }
    }
}

void removePiece(char *board, int boardRows, int boardCols, BlockRotation *piece, int block_dim, int top, int left) {
    int i, j;
    (void)boardRows;
    for (i = 0; i < piece->rows; i++) {
        for (j = 0; j < piece->cols; j++) {
            if (piece->grid[i * block_dim + j] != '.') {
                board[(top + i) * boardCols + (left + j)] = '-';
            }
        }
    }
}

void solvePuzzle(char *board, int boardRows, int boardCols, Block *blocks, int nblocks, int curIndex, int placedCount, SolverState *state) {
    int r, top, left, curBlockDim;
    int i;
    char letter;

    /* Base case: if we've considered all pieces */
    if (curIndex >= nblocks) {
        if (placedCount == nblocks) {
            state->solutions_count++;
        }
        if (placedCount > state->bestCount) {
            for (i = 0; i < state->boardArea; i++) {
                state->bestBoard[i] = board[i];
            }
            state->bestCount = placedCount;
        }
        return;
    }
    
    /* Update best solution if current placedCount is greater */
    if (placedCount > state->bestCount) {
        for (i = 0; i < state->boardArea; i++) {
            state->bestBoard[i] = board[i];
        }
        state->bestCount = placedCount;
    }
    
    /* Option to skip current piece */
    solvePuzzle(board, boardRows, boardCols, blocks, nblocks, curIndex + 1, placedCount, state);
    
    /* Try to place the current piece in all rotations and positions */
    letter = blocks[curIndex].rotations[0].grid[0]; /* the piece's letter */
    for (r = 0; r < 4; r++) {
        curBlockDim = (blocks[curIndex].rotations[r].rows > blocks[curIndex].rotations[r].cols) ?
                         blocks[curIndex].rotations[r].rows : blocks[curIndex].rotations[r].cols;
        for (top = 0; top <= boardRows - blocks[curIndex].rotations[r].rows; top++) {
            for (left = 0; left <= boardCols - blocks[curIndex].rotations[r].cols; left++) {
                if (canPlacePiece(board, boardRows, boardCols, &blocks[curIndex].rotations[r], curBlockDim, top, left)) {
                    placePiece(board, boardRows, boardCols, &blocks[curIndex].rotations[r], curBlockDim, top, left, letter);
                    solvePuzzle(board, boardRows, boardCols, blocks, nblocks, curIndex + 1, placedCount + 1, state);
                    removePiece(board, boardRows, boardCols, &blocks[curIndex].rotations[r], curBlockDim, top, left);
                }
            }
        }
    }
}

/* Build the sparse matrix of placements.
   For each piece instance (identified by its index in blocks)
   and for each rotation (0-3), for every translation that fits the board
   we add a row. Each row has a "1" in the column for that piece (index = boardArea + pieceIndex)
   and a "1" for each board cell that is covered.
   The function returns an allocated array of PlacementRow (of length *outRowsCount).
*/
PlacementRow *buildPlacementMatrix(int boardRows, int boardCols, Block *blocks, int nblocks, int *outRowsCount, int debug) { /* Add debug flag */
    int boardArea, totalRows, i, r, top, left, k, idx;
    int pRows, pCols, alloc_dim, activeCount, rowCount, colIndex, b;
    PlacementRow *matrix;
    int c; /* Declare loop variable 'c' at the beginning of the function */

    boardArea = boardRows * boardCols;
    totalRows = 0;
    /* First pass: count how many placement rows we will have */
    for (i = 0; i < nblocks; i++) {
        for (r = 0; r < 4; r++) {
            pRows = blocks[i].rotations[r].rows;
            pCols = blocks[i].rotations[r].cols;
            if (pRows > boardRows || pCols > boardCols)
                continue;
            for (top = 0; top <= boardRows - pRows; top++) {
                for (left = 0; left <= boardCols - pCols; left++) {
                    totalRows++;
                }
            }
        }
    }
    matrix = (PlacementRow *)malloc(totalRows * sizeof(PlacementRow));
    if (matrix == NULL) {
        fprintf(stderr, "Error: Failed to allocate placement matrix.\n");
        *outRowsCount = 0;
        return NULL;
    }
    idx = 0;
    for (i = 0; i < nblocks; i++) {
        if(debug){
            printf("Building placements for piece %d (letter %c):\n", i, blocks[i].rotations[0].grid[0]);
        }
        for (r = 0; r < 4; r++) {
            pRows = blocks[i].rotations[r].rows;
            pCols = blocks[i].rotations[r].cols;
            if (pRows > boardRows || pCols > boardCols) {
                if(debug){
                    printf("  Skipping rotation %d (dimensions %dx%d exceed board dimensions %dx%d)\n", r, pRows, pCols, boardRows, boardCols);
                }
                continue;
            }
            activeCount = 0;
            /* Use the allocated dimension stored in the rotation */
            alloc_dim = blocks[i].rotations[r].alloc_dim;
            for (k = 0; k < pRows * pCols; k++) {
                if (blocks[i].rotations[r].grid[k] != '.')
                    activeCount++;
            }
            rowCount = 1 + activeCount;
            if(debug){
                printf("  Rotation %d (%dx%d), active cells: %d, row count: %d\n", r, pRows, pCols, activeCount, rowCount);
            }
            for (top = 0; top <= boardRows - pRows; top++) {
                for (left = 0; left <= boardCols - pCols; left++) {
                    matrix[idx].pieceIndex = i;
                    matrix[idx].rotation = r;
                    matrix[idx].top = top;
                    matrix[idx].left = left;
                    matrix[idx].count = rowCount;
                    matrix[idx].cols = (int *)malloc(rowCount * sizeof(int));
                    if (matrix[idx].cols == NULL) {
                        int k2;
                        fprintf(stderr, "Error: Failed to allocate placement row columns.\n");
                        for (k2 = 0; k2 < idx; k2++) {
                            free(matrix[k2].cols);
                        }
                        free(matrix);
                        *outRowsCount = 0;
                        return NULL;
                    }
                    colIndex = 1;
                    matrix[idx].cols[0] = boardArea + i;  /* Piece constraint column */

                    if(debug){
                        printf("    Placement: top=%d, left=%d, piece constraint column=%d\n", top, left, boardArea + i);
                    }
                    for (k = 0; k < pRows; k++) {
                        for (b = 0; b < pCols; b++) {
                            if (blocks[i].rotations[r].grid[k * alloc_dim + b] != '.') {
                                int boardIndex;
                                boardIndex = (top + k) * boardCols + (left + b);
                                if(debug){
                                    printf("      Covering cell: row=%d, col=%d, board index=%d\n", top + k, left + b, boardIndex);
                                }
                                matrix[idx].cols[colIndex++] = boardIndex;
                            }
                        }
                    }
                    if(debug){
                        printf("    Columns covered:");
                        for (c = 0; c < matrix[idx].count; c++) {
                            printf(" %d", matrix[idx].cols[c]);
                        }
                        printf("\n");
                    }

                    idx++;
                }
            }
        }
    }
    *outRowsCount = totalRows;
    if(debug){
        printf("Total placement rows: %d\n", totalRows);
    }
    return matrix;
}

/* Free the memory allocated for the placement matrix */
void freePlacementMatrix(PlacementRow *matrix, int rowCount) {
    int i;
    for (i = 0; i < rowCount; i++) {
        free(matrix[i].cols);
    }
    free(matrix);
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

/* Revised algorithm_x function */
void algorithm_x(int total_columns, int total_rows, PlacementRow *matrix, int *active,
                 int *col_covered, int covered_count, int *solution, int sol_depth, int nblocks,
                 int *solution_count, int *best_solution, int *best_depth,
                 clock_t start_time, int time_limit, int boardRows, int boardCols, Block *blocks, int debug) { /* Add debug flag */
    int best_c, best_candidate_count;
    int c, r, k, i, j;

    if (debug) { /* Conditional and summarized debugging */
        printf("algorithm_x: depth=%d, covered_count=%d, ", sol_depth, covered_count);
        printf("active_rows=%d, covered_cols=%d\n",
               total_rows - (int) (sol_depth + (total_rows - *best_depth)),
               covered_count); /* Example of summarized info */
    }

    if (sol_depth > *best_depth) {
        *best_depth = sol_depth;
        for (i = 0; i < sol_depth; i++) {
            best_solution[i] = solution[i];
        }
    }

    if (covered_count == total_columns) {
        (*solution_count)++;
        if (debug) { /* Only print solution if debugging */
            print_solution(solution, sol_depth, matrix, blocks, boardRows, boardCols);
        }
        return;
    }

    if (((clock() - start_time) * 1000 / CLOCKS_PER_SEC) >= (time_limit * 1000)) {
        if (debug) {
            printf("Time limit reached.\n");
        }
        return;
    }
    best_c = -1;
    best_candidate_count = INT_MAX;
    for (c = 0; c < total_columns; c++) {
        if (!col_covered[c]) {
            int count = 0;
            for (r = 0; r < total_rows; r++) {
                if (active[r]) {
                    for (k = 0; k < matrix[r].count; k++) {
                        if (matrix[r].cols[k] == c) {
                            count++;
                            break;
                        }
                    }
                }
            }
            if (count < best_candidate_count) {
                best_candidate_count = count;
                best_c = c;
            }
        }
    }
    if (debug) {
        printf("  Best column to cover: %d (candidates: %d)\n", best_c, best_candidate_count);
    }

    if (best_c == -1 || best_candidate_count == 0) {
        if (debug) {
            printf("No uncovered columns or no candidates to cover.\n");
        }
        return;
    }
    for (r = 0; r < total_rows; r++) {
        if (active[r]) {
            int covers = 0;
            for (k = 0; k < matrix[r].count; k++) {
                if (matrix[r].cols[k] == best_c) {
                    covers = 1;
                    break;
                }
            }
            if (!covers)
                continue;

            if (debug) { /* More concise debugging */
                printf("  Trying row %d (piece %c, rot %d, top %d, left %d)\n", r, blocks[matrix[r].pieceIndex].rotations[0].grid[0], matrix[r].rotation, matrix[r].top, matrix[r].left);
            }

            solution[sol_depth] = r;
            {
                int *active_copy;
                int *col_covered_copy;
                int new_covered, new_covered_count;

                active_copy = malloc(total_rows * sizeof(int));
                col_covered_copy = malloc(total_columns * sizeof(int));
                if (!active_copy || !col_covered_copy) { exit(-1); }
                memcpy(active_copy, active, total_rows * sizeof(int));
                memcpy(col_covered_copy, col_covered, total_columns * sizeof(int));
                new_covered = 0;
                for (k = 0; k < matrix[r].count; k++) {
                    if (!col_covered_copy[matrix[r].cols[k]]) {
                        col_covered_copy[matrix[r].cols[k]] = 1;
                        new_covered++;
                        if (debug) {
                            printf("    Covering column %d\n", matrix[r].cols[k]);
                        }
                    }
                }
                new_covered_count = covered_count + new_covered;
                for (j = 0; j < total_rows; j++) {
                    if (active_copy[j] && j != r) {
                        if (row_conflicts(&matrix[r], &matrix[j])) {
                            active_copy[j] = 0;
                            if (debug) {
                                printf("    Deactivating conflicting row %d\n", j);
                            }
                        }
                    }
                }
                active_copy[r] = 0;
                if (debug) {
                    printf("    Row %d deactivated\n", r);
                }

                /* Recursive call with step-by-step execution */
                if (debug) {
                    printf("Press Enter to continue to next recursion...\n");
                    getchar(); /* Pause execution */
                }

                algorithm_x(total_columns, total_rows, matrix, active_copy, col_covered_copy,
                            new_covered_count, solution, sol_depth + 1, nblocks, solution_count,
                            best_solution, best_depth, start_time, time_limit, boardRows, boardCols, blocks, debug); /* Pass debug flag */
                free(active_copy);
                free(col_covered_copy);
            }
        }
    }
}
/* Solve exact cover problem using Dancing Links algorithm (Algorithm X) */
void solvePuzzleDLX(int boardRows, int boardCols, Block *blocks, int nblocks, char *unused_canvas, SolverState *state, int debug) {
    PlacementRow *matrix = NULL;
    int *active = NULL, *col_covered = NULL, *solution = NULL, *best_solution = NULL;
    int rowCount = 0, total_columns = 0, i = 0;
    clock_t start_time = 0;
    int time_limit = 10; /* seconds */

    /* Mark unused parameter to avoid compiler warnings */
    (void)unused_canvas;

    matrix = buildPlacementMatrix(boardRows, boardCols, blocks, nblocks, &rowCount, debug);
    if (matrix == NULL) {
        return;
    }
    total_columns = boardRows * boardCols + nblocks;
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
    algorithm_x(total_columns, rowCount, matrix, active, col_covered, 0, solution, 0, nblocks,
                &state->solutions_count, best_solution, &state->bestCount, start_time, time_limit, boardRows, boardCols, blocks, debug); /* Pass debug flag */

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

    free(active);
    free(col_covered);
    free(solution);
    free(best_solution);
    freePlacementMatrix(matrix, rowCount);
}