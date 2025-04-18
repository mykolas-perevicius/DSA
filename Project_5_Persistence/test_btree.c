#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <limits.h> /* Needed for INT_MIN, INT_MAX */

/* Required Struct Definitions */
struct Node { int n; int leaf; int *key; int *value; int *c; };
struct BTree { int root; int t; };

/* Required Prototypes from btree.c */
struct BTree BTree_open (const char *name, int t);
void        BTree_close(struct BTree *bt);
void        BTree_put  (const struct BTree *bt, int k, int v);
void        BTree_get  (const struct BTree *bt, int k, int *v);
void        BTree_delete(struct BTree *bt, int k);


/* Required Prototypes from storage.c */
/* ADDED Storage_read prototype */
void          Storage_read (int addr, struct Node *x);
unsigned long Storage_get_read_count(void);
unsigned long Storage_get_write_count(void);
unsigned long Storage_get_alloc_count(void);

/* Test file/config */
#define TEST_DB_FILE "test_btree.db"
#define TEST_T 3
#define NUM_RANDOM_INSERTS 1000
#define NUM_RANDOM_DELETES (NUM_RANDOM_INSERTS / 4) /* Not used yet */
#define NUM_RANDOM_QUERIES (NUM_RANDOM_INSERTS / 2)

/* Sentinel value used for marking deletions in btree.c */
#define DELETION_SENTINEL ((int)0xDEADDEAD)
/* Sentinel value for unused slots (padding) */
#define UNUSED_SENTINEL ((int)0xDEADBEEF)

/* REMOVED unused static prototypes */
/* static struct Node* BTree_disk_read(int t, int addr); */
/* static void BTree_free_node_mem(struct Node *x); */


/* ---- Copied static helpers from btree.c into test_btree.c for checker ---- */
/* This is necessary because they are static in btree.c */

static struct Node* BTree_allocate_node_mem_checker(int t) {
    /* Declarations */
    struct Node *x = NULL;
    int max_keys;
    int max_children;
    int i;
    /* Code */
    assert(t >= 2); max_keys = 2 * t - 1; max_children = 2 * t;
    x = malloc(sizeof(struct Node));
    if (!x) {perror("Checker Memory Error"); exit(EXIT_FAILURE); }
    x->key = malloc(max_keys * sizeof(int));
    x->value = malloc(max_keys * sizeof(int));
    x->c = malloc(max_children * sizeof(int));
    if (!x->key || !x->value || !x->c) {
        perror("Checker Memory Error"); free(x->key); free(x->value); free(x->c); free(x);
        exit(EXIT_FAILURE); }
    for (i = 0; i < max_keys; ++i) { x->key[i] = UNUSED_SENTINEL; x->value[i] = UNUSED_SENTINEL; }
    for (i = 0; i < max_children; ++i) { x->c[i] = UNUSED_SENTINEL; }
    x->n = 0; x->leaf = 1; return x;
}
static void BTree_free_node_mem_checker(struct Node *x) {
    if (x) { free(x->key); free(x->value); free(x->c); free(x); }
}
static struct Node* BTree_disk_read_checker(int t, int addr) {
    struct Node *x = BTree_allocate_node_mem_checker(t);
    Storage_read(addr, x); /* Uses the actual storage read - prototype added above */
    return x;
}
/* --- End of copied helpers --- */


/* --- CLRS Invariant Checks --- */

/* Recursive helper for invariant checks */
/* Returns 1 if invariants hold for subtree, 0 otherwise */
static int check_node_recursive(int t, int addr, int is_root, int depth, int* tree_height, int min_bound, int max_bound) {
    /* --- Declarations (ANSI C) --- */
    struct Node *x = NULL;
    int i;
    int result = 1;
    int expected_min_keys;
    /* MOVED declarations up */
    int next_min_bound;
    int next_max_bound;

    /* --- Code --- */
    x = BTree_disk_read_checker(t, addr);
    if (x == NULL) return 0;

    expected_min_keys = is_root ? 1 : t - 1;
    if (x->n == 0 && is_root) {
         if (!x->leaf) {
             fprintf(stderr, "Invariant Fail (Addr %d): Non-leaf root has n=0 keys.\n", addr);
             result = 0; goto cleanup;
         }
         expected_min_keys = 0; /* Allow empty root leaf */
    }

    if (!((x->n >= expected_min_keys) && x->n <= 2 * t - 1)) {
        fprintf(stderr, "Invariant Fail (Addr %d): Key count n=%d out of range [%d, %d]. is_root=%d, is_leaf=%d\n",
                addr, x->n, expected_min_keys, 2 * t - 1, is_root, x->leaf);
        result = 0; goto cleanup;
    }

    for (i = 0; i < x->n; ++i) {
        if (!(x->key[i] >= min_bound && x->key[i] <= max_bound)) {
           /* Allow bounds to be equal if at min/max? CLRS is strict < > */
           /* Let's assume bounds are exclusive for children, inclusive for node's own keys */
             fprintf(stderr, "Invariant Fail (Addr %d): Key[%d]=%d out of bounds [%d, %d].\n",
                     addr, i, x->key[i], min_bound, max_bound);
             result = 0; goto cleanup;
        }
        if (i > 0 && !(x->key[i] > x->key[i-1])) {
            fprintf(stderr, "Invariant Fail (Addr %d): Keys not sorted: key[%d]=%d <= key[%d]=%d.\n",
                    addr, i-1, x->key[i-1], i, x->key[i]);
            result = 0; goto cleanup;
        }
         if (x->key[i] == UNUSED_SENTINEL) {
            fprintf(stderr, "Invariant Fail (Addr %d): Key[%d] has unused sentinel value.\n", addr, i);
            result = 0; goto cleanup;
         }
          if (x->value[i] == UNUSED_SENTINEL) {
            fprintf(stderr, "Invariant Fail (Addr %d): Value[%d] has unused sentinel value.\n", addr, i);
            result = 0; goto cleanup;
          }
    }

    if (x->leaf) {
        if (*tree_height == -1) {
            *tree_height = depth;
        } else if (*tree_height != depth) {
            fprintf(stderr, "Invariant Fail (Addr %d): Leaf node at wrong depth %d (expected %d).\n",
                    addr, depth, *tree_height);
            result = 0; goto cleanup;
        }
    } else {
        if (x->n + 1 <= 0) {
             fprintf(stderr, "Invariant Fail (Addr %d): Invalid child count %d.\n", addr, x->n + 1);
             result = 0; goto cleanup;
        }
        for (i = 0; i <= x->n; ++i) {
             if (x->c[i] < 0 || x->c[i] == UNUSED_SENTINEL) {
                 fprintf(stderr, "Invariant Fail (Addr %d): Child pointer c[%d]=%d is invalid.\n", addr, i, x->c[i]);
                 result = 0; goto cleanup;
             }
             /* Define new bounds for child - use exclusive bounds based on parent keys */
             /* Child keys must be > key[i-1] and < key[i] */
             next_min_bound = (i == 0)    ? min_bound : x->key[i - 1];
             next_max_bound = (i == x->n) ? max_bound : x->key[i];

             /* Check if the calculated bounds make sense (min < max) */
             /* Careful with INT_MIN/INT_MAX */
             if (i > 0 && i < x->n && next_min_bound >= next_max_bound) {
                 fprintf(stderr, "Invariant Logic Error (Addr %d): Child bounds invalid min=%d >= max=%d for child c[%d].\n",
                         addr, next_min_bound, next_max_bound, i);
                 result = 0; goto cleanup;
             }

             if (!check_node_recursive(t, x->c[i], 0, depth + 1, tree_height, next_min_bound, next_max_bound)) {
                 result = 0; goto cleanup;
             }
        }
    }

cleanup:
    BTree_free_node_mem_checker(x);
    return result;
}


/* Top-level invariant check function */
static void check_btree_invariants(const struct BTree *bt) {
    int tree_height = -1;
    int is_valid;

    if (bt == NULL || bt->t < 2 || bt->root != 0) {
         fprintf(stderr, "Invariant Fail: BTree struct invalid (t=%d, root=%d).\n", bt ? bt->t : -1, bt ? bt->root : -1);
         assert(0);
    }

    is_valid = check_node_recursive(bt->t, bt->root, 1, 0, &tree_height, INT_MIN, INT_MAX);

    if (!is_valid) {
        fprintf(stderr, "!!! B-Tree Invariants VIOLATED !!!\n");
        assert(0);
    }
}

/* --- Test Helper Functions --- */

static unsigned long test_rand_state = 1;
int test_rand(void) {
    test_rand_state = test_rand_state * 1103515245 + 12345;
    return (unsigned int)(test_rand_state / 65536) % 32768;
}
void test_srand(unsigned int seed) { test_rand_state = seed; }

void test_shuffle(int *array, size_t n) {
    size_t i; size_t j; int temp;
    if (n > 1) {
        for (i = 0; i < n - 1; i++) {
            j = i + test_rand() / (32768 / (n - i) + 1);
            temp = array[j]; array[j] = array[i]; array[i] = temp;
        }
    }
}


/* --- Test Cases --- */

void test_basic_crud() {
    struct BTree bt; int val; int not_found_marker = -999;
    printf("--- Test Basic CRUD ---\n");
    remove(TEST_DB_FILE);
    bt = BTree_open(TEST_DB_FILE, TEST_T);
    check_btree_invariants(&bt);
    printf("Inserting key=10, val=100\n");
    BTree_put(&bt, 10, 100); check_btree_invariants(&bt);
    val = not_found_marker; BTree_get(&bt, 10, &val);
    printf("Got val for key=10: %d\n", val); assert(val == 100);
    printf("Inserting key=20, val=200\n");
    BTree_put(&bt, 20, 200); check_btree_invariants(&bt);
    printf("Updating key=10, val=101\n");
    BTree_put(&bt, 10, 101); check_btree_invariants(&bt);
    val = not_found_marker; BTree_get(&bt, 10, &val);
    printf("Got updated val for key=10: %d\n", val); assert(val == 101);
    val = not_found_marker; BTree_get(&bt, 20, &val);
    printf("Got val for key=20: %d\n", val); assert(val == 200);
    val = not_found_marker; BTree_get(&bt, 30, &val);
    printf("Got val for key=30 (not found): %d\n", val); assert(val == not_found_marker);
    BTree_close(&bt); printf("Basic CRUD Passed.\n");
}

void test_node_split() {
    struct BTree bt; int i; int num_keys_to_split; int not_found_marker = -999; int val;
    printf("--- Test Node Split (Requires t=%d) ---\n", TEST_T);
     if (TEST_T < 2) { printf("Skipping split test, t must be >= 2\n"); return; }
    remove(TEST_DB_FILE);
    bt = BTree_open(TEST_DB_FILE, TEST_T);
    num_keys_to_split = 2 * TEST_T - 1;
    printf("Inserting %d keys to force a split...\n", num_keys_to_split);
    for (i = 1; i <= num_keys_to_split; ++i) {
        BTree_put(&bt, i * 10, i * 100); check_btree_invariants(&bt);
    }
    printf("Root node should be full now (n=%d).\n", num_keys_to_split);
    printf("Inserting one more key (%d) to trigger root split...\n", (num_keys_to_split + 1) * 10);
    BTree_put(&bt, (num_keys_to_split + 1) * 10, (num_keys_to_split + 1) * 100);
    check_btree_invariants(&bt);
    printf("Root should have split. Checking keys...\n");
    for (i = 1; i <= num_keys_to_split + 1; ++i) {
        val = not_found_marker; BTree_get(&bt, i * 10, &val);
        if (val != i * 100) { fprintf(stderr, " Split test failed: Key %d not found or wrong value (%d != %d)\n", i*10, val, i*100); }
        assert(val == i * 100);
    }
    BTree_close(&bt); printf("Node Split Test Passed.\n");
}

void test_random_inserts_and_queries() {
    struct BTree bt; int *keys = NULL; int *values = NULL; int i; int j; int k; int unique;
    int not_found_marker = -9999; time_t start, end; int val;
    printf("--- Test Random Inserts/Queries (N=%d, T=%d) ---\n", NUM_RANDOM_INSERTS, TEST_T);
    remove(TEST_DB_FILE);
    bt = BTree_open(TEST_DB_FILE, TEST_T);
    keys = malloc(NUM_RANDOM_INSERTS * sizeof(int));
    values = malloc(NUM_RANDOM_INSERTS * sizeof(int)); assert(keys && values);
    test_srand((unsigned int)time(NULL));
    printf("Generating %d unique random keys...\n", NUM_RANDOM_INSERTS);
    for (i = 0; i < NUM_RANDOM_INSERTS; ++i) {
        do { unique = 1; k = test_rand() % (NUM_RANDOM_INSERTS * 10);
            for (j = 0; j < i; ++j) { if (keys[j] == k) { unique = 0; break; } }
             if (!unique) continue;
        } while (0);
        keys[i] = k; values[i] = k + 1000000;
    }
    printf("Inserting %d keys...\n", NUM_RANDOM_INSERTS); start = clock();
    for (i = 0; i < NUM_RANDOM_INSERTS; ++i) {
        BTree_put(&bt, keys[i], values[i]);
         if ((i + 1) % (NUM_RANDOM_INSERTS / 10) == 0) {
             printf("  Inserted %d / %d\n", i + 1, NUM_RANDOM_INSERTS);
             check_btree_invariants(&bt);
         }
    }
    end = clock(); check_btree_invariants(&bt);
    printf("Insertion time: %.2f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);
    printf("Querying %d random inserted keys...\n", NUM_RANDOM_QUERIES);
    test_shuffle(keys, NUM_RANDOM_INSERTS); start = clock();
    for (i = 0; i < NUM_RANDOM_QUERIES; ++i) {
        val = not_found_marker; BTree_get(&bt, keys[i], &val);
        if (val != keys[i] + 1000000) { fprintf(stderr, " Random test failed: Key %d not found or wrong value (%d != %d)\n", keys[i], val, keys[i] + 1000000); }
        assert(val == keys[i] + 1000000);
         if ((i + 1) % (NUM_RANDOM_QUERIES / 10) == 0) { printf("  Queried %d / %d\n", i + 1, NUM_RANDOM_QUERIES); }
    }
    end = clock(); printf("Query time: %.2f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);
    val = not_found_marker; BTree_get(&bt, -1, &val); assert(val == not_found_marker);
    printf("Storage Stats: Reads=%lu, Writes=%lu, Allocs=%lu\n",
           Storage_get_read_count(), Storage_get_write_count(), Storage_get_alloc_count());
    BTree_close(&bt); free(keys); free(values);
    printf("Random Inserts/Queries Test Passed.\n");
}

void test_delete_and_update() {
    struct BTree bt; int i;
    int keys_to_use[] = {10, 20, 5, 15, 25, 30, 3, 8, 12, 18, 22, 28};
    int num_keys = sizeof(keys_to_use) / sizeof(keys_to_use[0]);
    int keys_to_delete[] = {15, 3, 30, 99 /* non-existent */};
    int num_to_delete = sizeof(keys_to_delete) / sizeof(keys_to_delete[0]);
    int val; int not_found_marker = -555; int current_key; int is_deleted; int j;
    printf("--- Test Delete and Update (Marking) ---\n");
    remove(TEST_DB_FILE);
    bt = BTree_open(TEST_DB_FILE, TEST_T); check_btree_invariants(&bt);
    printf("Inserting initial keys...\n");
    for (i = 0; i < num_keys; ++i) {
        BTree_put(&bt, keys_to_use[i], keys_to_use[i] * 10); check_btree_invariants(&bt);
    }
    printf("Deleting selected keys...\n");
    for (i = 0; i < num_to_delete; ++i) {
        printf(" Deleting %d\n", keys_to_delete[i]);
        BTree_delete(&bt, keys_to_delete[i]); check_btree_invariants(&bt);
        val = not_found_marker; BTree_get(&bt, keys_to_delete[i], &val); assert(val == not_found_marker);
    }
    printf("Verifying remaining keys and deleted keys...\n");
    for (i = 0; i < num_keys; ++i) {
        current_key = keys_to_use[i]; is_deleted = 0; val = not_found_marker;
        BTree_get(&bt, current_key, &val);
        for (j = 0; j < num_to_delete; ++j) { if (current_key == keys_to_delete[j]) { is_deleted = 1; break; } }
        if (is_deleted) { assert(val == not_found_marker); } else { assert(val == current_key * 10); }
    }
    printf("Attempting to delete already deleted key (should be no-op)...\n");
    BTree_delete(&bt, 15); check_btree_invariants(&bt);
    val = not_found_marker; BTree_get(&bt, 15, &val); assert(val == not_found_marker);
    printf("Re-inserting/Updating a deleted key...\n");
    BTree_put(&bt, 15, 155); check_btree_invariants(&bt);
    val = not_found_marker; BTree_get(&bt, 15, &val);
    printf(" Got val for re-inserted key=15: %d\n", val); assert(val == 155);
    printf("Updating a non-deleted key...\n");
    BTree_put(&bt, 20, 202); check_btree_invariants(&bt);
     val = not_found_marker; BTree_get(&bt, 20, &val); assert(val == 202);
    BTree_close(&bt); printf("Delete and Update Test Passed.\n");
}


int main() {
    printf("Starting B-Tree Test Suite...\n");
    test_basic_crud(); printf("\n");
    test_node_split(); printf("\n");
    test_random_inserts_and_queries(); printf("\n");
    test_delete_and_update(); printf("\n");
    printf("All B-Tree Tests Passed!\n");
    return 0;
}