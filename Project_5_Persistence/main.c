#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h> /* For memcpy */
#include <assert.h>

/* Required Struct Definitions */
struct Node { int n; int leaf; int *key; int *value; int *c; };
struct BTree { int root; int t; };

/* Required Prototypes from btree.c */
struct BTree BTree_open (const char *name, int t);
void        BTree_close(struct BTree *bt);
void        BTree_put  (const struct BTree *bt, int k, int v);
void        BTree_get  (const struct BTree *bt, int k, int *v);

/* Required Prototypes from storage.c */
unsigned long Storage_get_read_count(void);
unsigned long Storage_get_write_count(void);
unsigned long Storage_get_alloc_count(void);


#define EXAMPLE_DB_FILE "main_example.db"
#define EXAMPLE_T 170
#define NUM_KEYS_TO_INSERT 100000
#define NUM_KEYS_TO_QUERY 10000

static unsigned long next_rand_main = 1;
int simple_rand_main(void) {
    next_rand_main = next_rand_main * 1103515245 + 12345;
    return (unsigned int)(next_rand_main / 65536) % 32768;
}
void simple_srand_main(unsigned int seed) { next_rand_main = seed; }


int main() {
    /* --- ALL declarations at the top of the block (ANSI C requirement) --- */
    struct BTree bt;
    int *keys_inserted = NULL;
    int *keys_to_query = NULL;
    clock_t start, end;
    double cpu_time_used;
    unsigned long initial_reads, initial_writes, initial_allocs;
    unsigned long final_reads, final_writes, final_allocs;
    int i; /* Declare loop variables here */
    int j; /* Declare loop variable here */
    int k; /* For generated key */
    int duplicate; /* For duplicate check */
    int found_count = 0; /* Moved declaration up */
    int not_found_marker = -7777; /* Moved declaration up */
    int value; /* Moved declaration up */
    size_t shuffle_idx; /* For shuffle index */
    int tmp; /* For shuffle temporary storage */

    /* --- Executable code starts here --- */
    printf("B-Tree Usage Example\n");
    printf("Database file: %s\n", EXAMPLE_DB_FILE);
    printf("Minimum degree t: %d\n", EXAMPLE_T);
    printf("Keys to insert: %d\n", NUM_KEYS_TO_INSERT);
    printf("Keys to query: %d\n", NUM_KEYS_TO_QUERY);

    keys_inserted = malloc(NUM_KEYS_TO_INSERT * sizeof(int));
    keys_to_query = malloc(NUM_KEYS_TO_QUERY * sizeof(int));
    if (!keys_inserted || !keys_to_query) {
        perror("Failed to allocate memory for keys");
        return 1;
    }

    printf("Generating %d unique random keys...\n", NUM_KEYS_TO_INSERT);
    simple_srand_main((unsigned int)time(NULL));
    /* Use loop variable 'i' declared above */
    for (i = 0; i < NUM_KEYS_TO_INSERT; ++i) {
        do {
            k = (simple_rand_main() << 15) | simple_rand_main();
            if (k < 0) k = -k;
            duplicate = 0;
            /* Use loop variable 'j' declared above */
            for (j = 0; j < i; ++j) {
                 if (keys_inserted[j] == k) {
                     duplicate = 1;
                     break;
                 }
            }
            if (duplicate) continue;
        } while (0);
        keys_inserted[i] = k;
    }
    printf("Keys generated.\n");

    /* Select a subset for querying */
    /* Use loop variable 'i' declared above */
     for(i=0; i<NUM_KEYS_TO_INSERT; ++i) {
        shuffle_idx = (size_t)i + simple_rand_main() / (32768 / (NUM_KEYS_TO_INSERT - i) + 1);
        tmp = keys_inserted[shuffle_idx];
        keys_inserted[shuffle_idx] = keys_inserted[i];
        keys_inserted[i] = tmp;
     }
    memcpy(keys_to_query, keys_inserted, NUM_KEYS_TO_QUERY * sizeof(int));

    /* Re-shuffle insertion keys */
    /* Use loop variable 'i' declared above */
     for(i=0; i<NUM_KEYS_TO_INSERT; ++i) {
        shuffle_idx = (size_t)i + simple_rand_main() / (32768 / (NUM_KEYS_TO_INSERT - i) + 1);
        tmp = keys_inserted[shuffle_idx];
        keys_inserted[shuffle_idx] = keys_inserted[i];
        keys_inserted[i] = tmp;
     }

    printf("Opening B-tree file '%s' with t=%d...\n", EXAMPLE_DB_FILE, EXAMPLE_T);
    remove(EXAMPLE_DB_FILE);
    bt = BTree_open(EXAMPLE_DB_FILE, EXAMPLE_T);
    initial_reads = Storage_get_read_count();
    initial_writes = Storage_get_write_count();
    initial_allocs = Storage_get_alloc_count();
    printf("B-tree opened.\n");

    printf("Inserting %d keys...\n", NUM_KEYS_TO_INSERT);
    start = clock();
    /* Use loop variable 'i' declared above */
    for (i = 0; i < NUM_KEYS_TO_INSERT; ++i) {
        BTree_put(&bt, keys_inserted[i], keys_inserted[i] * 2);
        if ((i + 1) % (NUM_KEYS_TO_INSERT / 10) == 0) {
            printf("  Inserted %d / %d\n", i + 1, NUM_KEYS_TO_INSERT);
        }
    }
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Insertion finished in %.2f seconds.\n", cpu_time_used);

    printf("Querying %d keys...\n", NUM_KEYS_TO_QUERY);
    start = clock();
    /* Use loop variable 'i' declared above */
    for (i = 0; i < NUM_KEYS_TO_QUERY; ++i) {
        value = not_found_marker; /* Initialize before get */
        BTree_get(&bt, keys_to_query[i], &value);
        if (value == keys_to_query[i] * 2) {
            found_count++;
        } else {
            fprintf(stderr, "Verification failed: Key %d not found or wrong value %d (expected %d)\n",
                    keys_to_query[i], value, keys_to_query[i] * 2);
        }
    }
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Querying finished in %.2f seconds.\n", cpu_time_used);
    printf("Verified %d out of %d keys.\n", found_count, NUM_KEYS_TO_QUERY);
    assert(found_count == NUM_KEYS_TO_QUERY);

    final_reads = Storage_get_read_count();
    final_writes = Storage_get_write_count();
    final_allocs = Storage_get_alloc_count();

    printf("Closing B-tree...\n");
    BTree_close(&bt);
    printf("B-tree closed.\n");

    printf("\n--- Storage Statistics ---\n");
    printf("Total Reads: %lu\n", final_reads);
    printf("Total Writes: %lu\n", final_writes);
    printf("Total Allocs: %lu\n", final_allocs);
    printf("Reads during operations: %lu\n", final_reads - initial_reads);
    printf("Writes during operations: %lu\n", final_writes - initial_writes);
    printf("Allocs during operations: %lu\n", final_allocs - initial_allocs);

    free(keys_inserted);
    free(keys_to_query);

    printf("\nExample finished successfully.\n");
    return 0;
}