#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdarg.h>
/* For INT_MAX */
#include <limits.h>

/* Debugging facilities */
static bool debug_enabled = false;
static FILE *debug_file = NULL;

void DEBUG_PRINT(const char *format, ...) {
#ifdef DEBUG
    if (debug_enabled) {
        va_list args;
        va_start(args, format);
        vfprintf(debug_file ? debug_file : stderr, format, args);
        va_end(args);
        fflush(debug_file ? debug_file : stderr); /* Flush for immediate output */
    }
#else
    /* Do nothing in non-debug mode */
    (void)format;
#endif
}

/* Structure to represent a shape */
typedef struct {
    char id;
    int rows;
    int cols;
    char **data;
} Shape;

/* Structure to represent the grid */
typedef struct {
    int rows;
    int cols;
    char **data;
} Grid;

/* Structure to represent a placement candidate */
typedef struct {
    int row;
    int col;
    Shape *shape;
    int priority;  /* A* cost score */
} Candidate;

/* Forward declaration for free_grid */
void free_grid(Grid *grid);

/* Forward declaration for is_in_unique_variations */
bool is_in_unique_variations(Shape **unique_variations, int unique_count, Shape *shape);

/* Shape templates (static and const) */
typedef struct {
    char id;
    int rows;
    int cols;
    const char *data[6];
} Template;

static const Template templates[] = {
    { 'A', 5, 1, {"A", "A", "A", "A", "A"}},
    { 'C', 3, 3, {"CC.", ".CC", ".C."}},
    { 'D', 4, 2, {".D", ".D", ".D", "DD"}},
    { 'F', 3, 2, {"FF", "FF", ".F"}},
    { 'I', 4, 2, {"I.", "I.", "II", ".I"}},
    { 'J', 3, 3, {"JJJ", ".J.", ".J."}},
    { 'K', 2, 3, {"K.K", "KKK"}},
    { 'L', 3, 3, {"..L", "..L", "LLL"}},
    { 'M', 3, 3, {"..M", ".MM", "MM."}},
    { 'N', 3, 3, {".N.", "NNN", ".N."}},
    { 'O', 4, 2, {"O.", "OO", "O.", "O."}},
    { 'Q', 3, 3, {".QQ", ".Q.", "QQ."}}
};
int num_shape_templates = sizeof(templates) / sizeof(templates[0]);

/* Function to create a shape from template data */
Shape *create_shape(char id) {
    int i, template_index = -1;
    Template *tmpl;
    Shape *shape;

    for (i = 0; i < num_shape_templates; ++i) {
        if (templates[i].id == id) {
            template_index = i;
            break;
        }
    }
    if (template_index == -1) return NULL;

    tmpl = (Template *)&templates[template_index];
    shape = (Shape *)malloc(sizeof(Shape));
    if (!shape) {
        perror("Failed to allocate memory for shape");
        return NULL;
    }
    shape->id = id;
    shape->rows = tmpl->rows;
    shape->cols = tmpl->cols;
    shape->data = (char **)malloc(shape->rows * sizeof(char *));
    if (!shape->data) {
        perror("Failed to allocate memory for shape data");
        free(shape);
        return NULL;
    }
    for (i = 0; i < shape->rows; i++) {
        shape->data[i] = (char *)malloc(shape->cols * sizeof(char));
        if (!shape->data[i]) {
            perror("Failed to allocate memory for shape data row");
            while (i > 0) {
                free(shape->data[--i]);
            }
            free(shape->data);
            free(shape);
            return NULL;
        }
        strcpy(shape->data[i], tmpl->data[i]);
    }
    return shape;
}

/* Function to free a shape's allocated memory */
void free_shape(Shape *shape) {
    if (shape) {
        int i;
        for (i = 0; i < shape->rows; i++) {
            free(shape->data[i]);
        }
        free(shape->data);
        free(shape);
    }
}

/* Function to rotate a shape clockwise */
Shape *rotate_shape_clockwise(const Shape *shape) {
    int i, j, new_rows, new_cols;
    Shape *rotated_shape;

    if (!shape) return NULL;

    new_rows = shape->cols;
    new_cols = shape->rows;

    rotated_shape = (Shape *)malloc(sizeof(Shape));
    if (!rotated_shape) {
        perror("Failed to allocate memory for rotated shape");
        return NULL;
    }
    rotated_shape->id = shape->id;
    rotated_shape->rows = new_rows;
    rotated_shape->cols = new_cols;
    rotated_shape->data = (char **)malloc(new_rows * sizeof(char *));
    if(!rotated_shape->data) {
        perror("Failed to allocate rotated shape rows");
        free(rotated_shape);
        return NULL;
    }
    for (i = 0; i < new_rows; i++) {
        rotated_shape->data[i] = (char*)malloc(sizeof(char) * new_cols);
        if(!rotated_shape->data[i]) {
           perror("Failed to allocate memory for rotated shape data");
           while(i > 0) {
              free(rotated_shape->data[--i]);
           }
           free(rotated_shape->data);
           free(rotated_shape);
           return NULL;
        }
    }

    for (i = 0; i < new_rows; i++) {
        for (j = 0; j < new_cols; j++) {
           rotated_shape->data[i][j] = shape->data[new_cols - 1 - j][i];
        }
    }
    return rotated_shape;
}

/* Function to reflect a shape (horizontal mirror) */
Shape *reflect_shape(const Shape *shape) {
    int i, j;
    Shape *reflected_shape;
    if (!shape) return NULL;

    reflected_shape = (Shape *)malloc(sizeof(Shape));
    if(!reflected_shape){
        perror("Failed to allocate memory for reflected shape");
        return NULL;
    }

    reflected_shape->id = shape->id;
    reflected_shape->rows = shape->rows;
    reflected_shape->cols = shape->cols;

    reflected_shape->data = (char**)malloc(sizeof(char*) * reflected_shape->rows);
    if(!reflected_shape->data) {
      perror("Failed to allocate memory for reflected shape data");
      free(reflected_shape);
      return NULL;
    }

    for(i = 0; i < reflected_shape->rows; i++) {
      reflected_shape->data[i] = (char*)malloc(sizeof(char) * reflected_shape->cols);
      if(!reflected_shape->data[i]) {
         perror("Could not allocate memory for row");
         while(i > 0) {
            free(reflected_shape->data[--i]);
         }
         free(reflected_shape->data);
         free(reflected_shape);
         return NULL;
      }
    }

    for (i = 0; i < shape->rows; i++) {
        for (j = 0; j < shape->cols; j++) {
            reflected_shape->data[i][j] = shape->data[i][shape->cols - 1 - j];
        }
    }

    return reflected_shape;
}

/* Function to calculate the active area of a shape */
int calculate_active_area(const Shape *shape) {
    int area = 0, i, j;
    for (i = 0; i < shape->rows; i++) {
        for (j = 0; j < shape->cols; j++) {
            if (shape->data[i][j] != '.') {
                area++;
            }
        }
    }
    return area;
}

/* Function to compare two shapes for equality */
bool are_shapes_equal(const Shape *s1, const Shape *s2) {
    int i, j;
    if (!s1 || !s2) return false; /* Handle NULL shapes */
    if (s1->rows != s2->rows || s1->cols != s2->cols) {
        return false;
    }
    for (i = 0; i < s1->rows; i++) {
        for (j = 0; j < s1->cols; j++) {
            if (s1->data[i][j] != s2->data[i][j]) {
                return false;
            }
        }
    }
    return true;
}

/* Function to get unique variations of a shape */
Shape **get_unique_variations(Shape *variations[], int num_variations, int *unique_count) {
    Shape **unique_variations = (Shape **)malloc(num_variations * sizeof(Shape *));
    int count = 0, i, j;
    if (!unique_variations) {
        perror("Failed to allocate memory for unique variations");
        return NULL;
    }

    if (num_variations > 0 && variations[0] != NULL) {
        unique_variations[count++] = variations[0];
    }

    for (i = 1; i < num_variations; i++) {
        bool is_unique = true;
        if (variations[i] == NULL) continue;
        for (j = 0; j < count; j++) {
            if (are_shapes_equal(variations[i], unique_variations[j])) {
                is_unique = false;
                break;
            }
        }
        if (is_unique) {
            unique_variations[count++] = variations[i];
        }
    }

    *unique_count = count;
    return unique_variations;
}

/* Function to create a grid from a file */
Grid *create_grid_from_file(const char *filename, char allowed_shapes[]) {
    FILE *fp = fopen(filename, "r");
    Grid *grid = NULL;
    char line[1024];
    int row_count = 0;
    int col_count = 0;
    int i, shape_index = 0, current_col_count;

    if (!fp) {
        perror("Error opening grid file");
        return NULL;
    }

    if (!fgets(line, sizeof(line), fp)) {
        fprintf(stderr, "Error reading allowed shapes from file.\n");
        fclose(fp);
        return NULL;
    }

    for (i = 0; line[i] != '\0' && line[i] != '\n'; i++) {
        Shape *test_shape = create_shape(line[i]);
        if (test_shape) {
            allowed_shapes[shape_index++] = line[i];
            free_shape(test_shape);
        } else {
            fprintf(stderr, "Warning: Invalid shape ID '%c' in input file ignored.\n", line[i]);
        }
    }
    allowed_shapes[shape_index] = '\0';

    if (shape_index == 0) {
        fprintf(stderr, "Error: No valid shapes in the input file.\n");
        fclose(fp);
        return NULL;
    }

    while (fgets(line, sizeof(line), fp)) {
        row_count++;
        current_col_count = strlen(line);
        if (line[current_col_count - 1] == '\n') {
            current_col_count--;
        }

        if (col_count == 0) {
            col_count = current_col_count;
        } else if (col_count != current_col_count) {
            fprintf(stderr, "Error: Inconsistent column count in grid file.\n");
            fclose(fp);
            return NULL;
        }
        if(row_count > 20 || col_count > 20) {
          fprintf(stderr, "Error: Grid dimensions exceed maximum (20x20).\n");
          fclose(fp);
          return NULL;
        }
    }

    if (row_count == 0) {
        fprintf(stderr, "Error: Grid data is empty in the input file.\n");
        fclose(fp);
        return NULL;
    }

    grid = (Grid *)malloc(sizeof(Grid));
    if (!grid) {
        perror("Failed to allocate memory for grid");
        fclose(fp);
        return NULL;
    }
    grid->rows = row_count;
    grid->cols = col_count;
    grid->data = (char **)malloc(row_count * sizeof(char *));
    if (!grid->data) {
        perror("Failed to allocate memory for grid data");
        free(grid);
        fclose(fp);
        return NULL;
    }
    for (i = 0; i < row_count; i++) {
        grid->data[i] = (char *)malloc(col_count * sizeof(char));
        if (!grid->data[i]) {
            perror("Failed to allocate memory for grid data row");
            while (i > 0) free(grid->data[--i]);
            free(grid->data);
            free(grid);
            fclose(fp);
            return NULL;
        }
    }

    rewind(fp);
    fgets(line, sizeof(line), fp);
    for (i = 0; i < row_count; i++) {
        if (fgets(line, sizeof(line), fp)) {
            int j;
            for (j = 0; j < col_count; j++) {
                grid->data[i][j] = line[j];
            }
        } else {
            fprintf(stderr, "Error: Fewer rows than expected in grid data.\n");
            free_grid(grid);
            fclose(fp);
            return NULL;
        }
    }
    fclose(fp);
    return grid;
}

/* Function to free a grid's allocated memory */
void free_grid(Grid *grid) {
    if (grid) {
        int i;
        for (i = 0; i < grid->rows; i++) {
            free(grid->data[i]);
        }
        free(grid->data);
        free(grid);
    }
}

/* Function to print the grid to stdout */
void print_grid(const Grid *grid) {
    int i, j;
    if (!grid) {
        printf("Grid is NULL\n");
        return;
    }
    for (i = 0; i < grid->rows; i++) {
        for (j = 0; j < grid->cols; j++) {
            printf("%c", grid->data[i][j]);
        }
        printf("\n");
    }
}

/* Function to check if a shape can be placed at a given position */
bool can_place_shape(const Grid *grid, const Shape *shape, int row, int col) {
    int i, j;

    /* Check bounds */
    if (row < 0 || row + shape->rows > grid->rows || col < 0 || col + shape->cols > grid->cols) {
        return false;
    }

    /* Check for overlaps and invalid cells */
    for (i = 0; i < shape->rows; i++) {
        for (j = 0; j < shape->cols; j++) {
            if (shape->data[i][j] != '.' && grid->data[row + i][col + j] != '-') {
                return false;
            }
        }
    }

    return true;
}

/* Function to place a shape on the grid */
void place_shape(Grid *grid, const Shape *shape, int row, int col) {
    int i, j;
    for (i = 0; i < shape->rows; i++) {
        for (j = 0; j < shape->cols; j++) {
            if (shape->data[i][j] != '.') {
                grid->data[row + i][col + j] = shape->data[i][j];
            }
        }
    }
}

/* Function to remove a shape from the grid */
void remove_shape(Grid *grid, const Shape *shape, int row, int col) {
    int i, j;
    for (i = 0; i < shape->rows; i++) {
        for (j = 0; j < shape->cols; j++) {
            if (shape->data[i][j] != '.') {
                grid->data[row + i][col + j] = '-';
            }
        }
    }
}

/* --- Heuristic Functions --- */

/* 1. Constrained Areas First: Calculate how many options a cell has */
int calculate_placement_options(const Grid *grid, int row, int col, char allowed_shapes[]) {
    int options = 0, i, unique_count, variant_index;
    Shape *original, *rot1, *rot2, *rot3, *reflected, *rrot1, *rrot2, *rrot3;
    Shape *variations[8] = {NULL};
    Shape **unique_variations;

    if (grid->data[row][col] != '-') return INT_MAX;  /* Already filled */

    for (i = 0; allowed_shapes[i] != '\0'; i++) {
        original = create_shape(allowed_shapes[i]);
        if (!original) continue;

        variations[0] = original;
        rot1 = rotate_shape_clockwise(original);
        variations[1] = rot1;
        rot2 = rotate_shape_clockwise(rot1);
        variations[2] = rot2;
        rot3 = rotate_shape_clockwise(rot2);
        variations[3] = rot3;
        reflected = reflect_shape(original);
        variations[4] = reflected;
        rrot1 = rotate_shape_clockwise(reflected);
        variations[5] = rrot1;
        rrot2 = rotate_shape_clockwise(rrot1);
        variations[6] = rrot2;
        rrot3 = rotate_shape_clockwise(rrot2);
        variations[7] = rrot3;

        unique_variations = get_unique_variations(variations, 8, &unique_count);

        for (variant_index = 0; variant_index < unique_count; variant_index++) {
            if (can_place_shape(grid, unique_variations[variant_index], row, col)) {
                options++;
            }
        }

        for (variant_index = 0; variant_index < unique_count; variant_index++) {
            if (unique_variations[variant_index]) {
                free_shape(unique_variations[variant_index]);
            }
        }
        free(unique_variations);
    }

    return options;
}

/* 4. Contiguity Preservation: Count empty neighbors */
int count_empty_neighbors(const Grid *grid, int row, int col) {
    int count = 0, i;
    int dr[] = {-1, 1, 0, 0};  /* Up, Down, Left, Right */
    int dc[] = {0, 0, -1, 1};

    for (i = 0; i < 4; i++) {
        int nr = row + dr[i];
        int nc = col + dc[i];
        if (nr >= 0 && nr < grid->rows && nc >= 0 && nc < grid->cols && grid->data[nr][nc] == '-') {
            count++;
        }
    }
    return count;
}

/* 7. Isolation Resolution: Check for lone usable cells */
bool is_isolated(const Grid *grid, int row, int col) {
    if (grid->data[row][col] != '-') return false;  /* Not an empty cell */

    /* Check if all neighbors are blocked or out of bounds */
    return count_empty_neighbors(grid, row, col) == 0;
}

/* A*-inspired cost scoring function */
int calculate_priority(const Grid *grid, int row, int col, const Shape *shape, char allowed_shapes[]) {
    int alpha = 2;  /* Weight for neighbor density */
    int options = calculate_placement_options(grid, row, col, allowed_shapes);
    int empty_neighbors = count_empty_neighbors(grid, row, col);
    int remaining_coverage = 0;
    int i, j;
    int coverage = 0;

    /* If it's completely isolated, make it highest priority */
    if (is_isolated(grid, row, col)) {
        return -1000;  /* very very high */
    }

    if (options == INT_MAX) return INT_MAX;  /* Already filled */
    if (options == 0) return INT_MAX;         /* no options available */

    remaining_coverage = (grid->rows * grid->cols);
    for (i = 0; i < grid->rows; i++) {
        for (j = 0; j < grid->cols; j++) {
            if (grid->data[i][j] != '-') {
                remaining_coverage--;
            }
        }
    }
    if (shape != NULL) {
        coverage = calculate_active_area(shape);
    }

    return (remaining_coverage - coverage) + alpha * empty_neighbors - options * 3;
}

/* Comparison function for qsort (for the candidate queue) */
int compare_candidates(const void *a, const void *b) {
    return ((Candidate *)a)->priority - ((Candidate *)b)->priority;
}

/* Function to create and populate the candidate queue */
Candidate *create_candidate_queue(Grid *grid, char allowed_shapes[], int *num_candidates) {
    int row, col, i, unique_count, variant_index;
    Candidate *queue = NULL;
    int queue_size = 0;

    /* Determine the maximum possible size of the queue (all empty cells) */
    for (row = 0; row < grid->rows; row++) {
        for (col = 0; col < grid->cols; col++) {
            if (grid->data[row][col] == '-') {
                queue_size++;
            }
        }
    }
    *num_candidates = 0;
    if(queue_size == 0) return NULL; /* No empty cells.  Handle gracefully. */

    queue = (Candidate *)malloc(queue_size * sizeof(Candidate));
        if(!queue) {
            perror("Error on candidate queue malloc");
            exit(EXIT_FAILURE); /* malloc failed.  Exit. */
    }

    /* Iterate over all possible shapes and variations */
    for (i = 0; allowed_shapes[i] != '\0'; i++) {
        Shape *original, *rot1, *rot2, *rot3, *reflected, *rrot1, *rrot2, *rrot3;
        Shape *variations[8] = {NULL};
        Shape **unique_variations;

        original = create_shape(allowed_shapes[i]);
        if (!original) continue;  /* Skip invalid shapes */

        variations[0] = original;
        rot1 = rotate_shape_clockwise(original);
        variations[1] = rot1;
        rot2 = rotate_shape_clockwise(rot1);
        variations[2] = rot2;
        rot3 = rotate_shape_clockwise(rot2);
        variations[3] = rot3;
        reflected = reflect_shape(original);
        variations[4] = reflected;
        rrot1 = rotate_shape_clockwise(reflected);
        variations[5] = rrot1;
        rrot2 = rotate_shape_clockwise(rrot1);
        variations[6] = rrot2;
        rrot3 = rotate_shape_clockwise(rrot2);
        variations[7] = rrot3;

        unique_variations = get_unique_variations(variations, 8, &unique_count);

        /* Iterate only over EMPTY grid cells. */
        for (row = 0; row < grid->rows; row++) {
            for (col = 0; col < grid->cols; col++) {
                if (grid->data[row][col] == '-') {
                    for (variant_index = 0; variant_index < unique_count; variant_index++) {
                        if (can_place_shape(grid, unique_variations[variant_index], row, col)) {
                            queue[*num_candidates].row = row;
                            queue[*num_candidates].col = col;
                            queue[*num_candidates].shape = unique_variations[variant_index];
                            queue[*num_candidates].priority = calculate_priority(grid, row, col, unique_variations[variant_index], allowed_shapes);
                            (*num_candidates)++;
                        }
                    }
                }
            }
        }

        /* Free variations (but NOT the shapes in unique_variations, as they are now in the queue). */
        for (variant_index = 0; variant_index < 8; variant_index++) {
            if (variations[variant_index] && !is_in_unique_variations(unique_variations, unique_count, variations[variant_index])) {
                free_shape(variations[variant_index]);
            }
        }
        free(unique_variations);
    }


    /* Sort the candidate queue by priority (lower priority is better) */
    qsort(queue, *num_candidates, sizeof(Candidate), compare_candidates);

    return queue;
}


bool is_in_unique_variations(Shape **unique_variations, int unique_count, Shape *shape){
  int i;
  for(i = 0; i < unique_count; i++){
    if(are_shapes_equal(unique_variations[i], shape))
        return true;
  }
  return false;
}


/*  Backtracking function to solve the puzzle (with heuristics) */
bool solve_grid(Grid *grid, char allowed_shapes[]) {
    int num_candidates = 0, i;
    Candidate *queue = create_candidate_queue(grid, allowed_shapes, &num_candidates);

    if (num_candidates == 0) { /* No empty cells OR create_candidate_queue failed. */
      bool is_solved = true;
      for(i = 0; i < grid->rows; i++) {
        int j;
        for(j = 0; j < grid->cols; j++) {
          if(grid->data[i][j] == '-') is_solved = false;
        }
      }
      free(queue);  /* queue can be NULL here */
      return is_solved;  /* Grid is either full (solved) or has no possible moves. */
    }

    for (i = 0; i < num_candidates; i++) {
        int row = queue[i].row;
        int col = queue[i].col;
        Shape *shape = queue[i].shape;

        if (can_place_shape(grid, shape, row, col)) {  /* Still valid? */
            place_shape(grid, shape, row, col);

            if (solve_grid(grid, allowed_shapes)) {
                free(queue);
                return true;  /* Solution found */
            } else {
                remove_shape(grid, shape, row, col);  /* Backtrack */
            }
        }
    }

    /* Free the shapes that were stored in the queue. */
    for(i = 0; i < num_candidates; i++){
        if(queue[i].shape != NULL) {
          free_shape(queue[i].shape);
        }
    }

    free(queue);
    return false;  /* No solution found */
}


/* Function to create a test grid of specified dimensions filled with '-' */
Grid *create_test_grid(int rows, int cols) {
    Grid *grid = (Grid *)malloc(sizeof(Grid));
    int i, j;
    if (!grid) {
        perror("Failed to allocate memory for test grid");
        return NULL;
    }
    grid->rows = rows;
    grid->cols = cols;
    grid->data = (char **)malloc(rows * sizeof(char *));
    if (!grid->data) {
        perror("Failed to allocate memory for test grid data");
        free(grid);
        return NULL;
    }
    for (i = 0; i < rows; i++) {
        grid->data[i] = (char *)malloc(cols * sizeof(char));
        if (!grid->data[i]) {
            perror("Failed to allocate memory for test grid data row");
            while (i > 0) free(grid->data[--i]);
            free(grid->data);
            free(grid);
            return NULL;
        }
        for (j = 0; j < cols; j++) {
            grid->data[i][j] = '-';
        }
    }
    return grid;
}

int main(int argc, char *argv[]) {
    char *grid_filename = NULL;
    char allowed_shapes[1024] = {0};
    Grid *grid = NULL;
    int arg_index = 1;

    /* Check for debug flag first */
    if (argc > 1 && strcmp(argv[1], "-debug") == 0) {
        debug_enabled = true;
        debug_file = fopen("debug.txt", "w");
        if (!debug_file) {
            perror("Error opening debug.txt");
            debug_enabled = false;  /* Disable debugging if file can't be opened */
        }
        arg_index++;
    }

    /* Process command-line arguments */
     if (argc > arg_index) {
        grid_filename = argv[arg_index];
        grid = create_grid_from_file(grid_filename, allowed_shapes);
        if (!grid) {
            if (debug_file) fclose(debug_file);
            return 1;
        }
    }  else {
       fprintf(stderr, "Usage: %s [-debug] <grid_file>\n", argv[0]);
       if(debug_file) fclose(debug_file);
       return 1;
     }

    DEBUG_PRINT("Initial Grid:\n");
    if (debug_enabled) print_grid(grid);
    DEBUG_PRINT("Allowed shapes: %s\n", allowed_shapes);

    if (solve_grid(grid, allowed_shapes)) {
        printf("\nSolved Grid:\n");
        print_grid(grid);
    } else {
        printf("\nNo solution found.\n");
    }

    free_grid(grid);
    if (debug_file) fclose(debug_file);
    return 0;
}