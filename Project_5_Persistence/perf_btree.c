#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h> /* For memcpy, sprintf */
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

#define PERF_DB_FILE_PREFIX "perf_btree_t"
#define NUM_KEYS 100000
#define NUM_QUERIES 10000

static unsigned long next_rand_perf = 1;
int simple_rand_perf(void) {
    next_rand_perf = next_rand_perf * 1103515245 + 12345;
    return (unsigned int)(next_rand_perf / 65536) % 32768;
}
void simple_srand_perf(unsigned int seed) { next_rand_perf = seed; }

int main(int argc, const char *argv[]) {
    /* --- Declarations at top (ANSI C) --- */
    int min_t = 4;
    int max_t = 128;
    int step_t = 2;
    int num_keys = NUM_KEYS;
    int num_queries = NUM_QUERIES;
    int *keys_to_insert = NULL;
    int *keys_to_query = NULL;
    char db_filename[256];
    int i; /* Loop variable */
    int j; /* Loop variable */
    int t; /* Loop variable for testing different degrees */
    int k; /* For generated key */
    int unique; /* For duplicate check */
    size_t shuffle_idx; /* For shuffle index */
    int tmp; /* For shuffle temporary storage */
    struct BTree bt;
    clock_t start, end;
    double insert_time, query_time;
    unsigned long reads_start_ins, writes_start_ins, allocs_start_ins;
    unsigned long reads_end_ins, writes_end_ins, allocs_end_ins;
    unsigned long reads_start_qry, writes_start_qry, allocs_start_qry;
    unsigned long reads_end_qry, writes_end_qry, allocs_end_qry;
    int val; /* For query result check */

    /* --- Code --- */
    printf("Performance Harness\n");
    printf("Usage: %s [num_keys] [num_queries] [min_t] [max_t] [step_t]\n", argv[0]);
    printf("Defaults: N=%d, Q=%d, min_t=%d, max_t=%d, step=x%d\n\n",
           NUM_KEYS, NUM_QUERIES, min_t, max_t, step_t);

    if (argc > 1) num_keys = atoi(argv[1]);
    if (argc > 2) num_queries = atoi(argv[2]);
    if (argc > 3) min_t = atoi(argv[3]);
    if (argc > 4) max_t = atoi(argv[4]);
    if (argc > 5) step_t = atoi(argv[5]);

    if (num_keys <= 0 || num_queries <= 0 || num_queries > num_keys || min_t < 2 || max_t < min_t || step_t < 1) {
        fprintf(stderr, "Invalid arguments.\n"); return 1;
    }

    keys_to_insert = malloc(num_keys * sizeof(int));
    keys_to_query = malloc(num_queries * sizeof(int));
    if (!keys_to_insert || !keys_to_query) { perror("Failed to allocate key arrays"); return 1; }

    printf("Generating %d unique keys for insertion...\n", num_keys);
    simple_srand_perf((unsigned int)time(NULL));
    /* Use loop variable 'i' declared above */
    for (i = 0; i < num_keys; ++i) {
        do {
            unique = 1;
            k = simple_rand_perf() % (num_keys * 10);
            /* Use loop variable 'j' declared above */
            for (j = 0; j < i; ++j) {
                 if (keys_to_insert[j] == k) { unique = 0; break; }
            }
            if (!unique) continue;
        } while(0);
        keys_to_insert[i] = k;
    }
    printf("Generating %d keys for querying...\n", num_queries);
    /* Use loop variable 'i' declared above */
    for(i=0; i<num_keys; ++i) {
        shuffle_idx = (size_t)i + simple_rand_perf() / (32768 / (num_keys - i) + 1);
        tmp = keys_to_insert[shuffle_idx];
        keys_to_insert[shuffle_idx] = keys_to_insert[i];
        keys_to_insert[i] = tmp;
    }
    memcpy(keys_to_query, keys_to_insert, num_queries * sizeof(int));
    /* Use loop variable 'i' declared above */
    for(i=0; i<num_keys; ++i) {
        shuffle_idx = (size_t)i + simple_rand_perf() / (32768 / (num_keys - i) + 1);
        tmp = keys_to_insert[shuffle_idx];
        keys_to_insert[shuffle_idx] = keys_to_insert[i];
        keys_to_insert[i] = tmp;
    }

    printf("------------------------------------------------------------------------------------------------------------------------------\n");
    printf("| %4s | %12s | %12s | %10s | %10s | %10s | %12s | %12s | %10s | %10s | %10s |\n",
           "T", "Ins Time (s)", "Ins Ops/s", "Ins Reads", "Ins Writes", "Ins Allocs", "Qry Time (s)", "Qry Ops/s", "Qry Reads", "Qry Writes", "Qry Allocs");
    printf("------------------------------------------------------------------------------------------------------------------------------\n");

    /* Use loop variable 't' declared above */
    for (t = min_t; t <= max_t; t = (step_t == 1 ? t + 1 : t * step_t)) {
        sprintf(db_filename, "%s%d.db", PERF_DB_FILE_PREFIX, t);
        remove(db_filename);

        bt = BTree_open(db_filename, t);

        /* --- Insertion Phase --- */
        reads_start_ins = Storage_get_read_count();
        writes_start_ins = Storage_get_write_count();
        allocs_start_ins = Storage_get_alloc_count();
        start = clock();
        /* Use loop variable 'i' declared above */
        for (i = 0; i < num_keys; ++i) {
             BTree_put(&bt, keys_to_insert[i], keys_to_insert[i] + 1);
        }
        end = clock();
        reads_end_ins = Storage_get_read_count();
        writes_end_ins = Storage_get_write_count();
        allocs_end_ins = Storage_get_alloc_count();
        insert_time = (double)(end - start) / CLOCKS_PER_SEC;

        /* --- Query Phase --- */
        reads_start_qry = Storage_get_read_count();
        writes_start_qry = Storage_get_write_count();
        allocs_start_qry = Storage_get_alloc_count();
        start = clock();
        /* Use loop variable 'i' declared above */
        for (i = 0; i < num_queries; ++i) {
            val = -1;
            BTree_get(&bt, keys_to_query[i], &val);
            if (val != keys_to_query[i] + 1) {
                 fprintf(stderr, "WARN: Query failed for key %d (t=%d, val=%d)\n", keys_to_query[i], t, val);
            }
        }
        end = clock();
        reads_end_qry = Storage_get_read_count();
        writes_end_qry = Storage_get_write_count();
        allocs_end_qry = Storage_get_alloc_count();
        query_time = (double)(end - start) / CLOCKS_PER_SEC;

        BTree_close(&bt);

        /* --- Report Results --- */
        printf("| %4d | %12.4f | %12.1f | %10lu | %10lu | %10lu | %12.4f | %12.1f | %10lu | %10lu | %10lu |\n",
               t,
               insert_time,
               insert_time > 0 ? (double)num_keys / insert_time : 0.0,
               reads_end_ins - reads_start_ins,
               writes_end_ins - writes_start_ins,
               allocs_end_ins - allocs_start_ins,
               query_time,
               query_time > 0 ? (double)num_queries / query_time : 0.0,
               reads_end_qry - reads_start_qry,
               writes_end_qry - writes_start_qry,
               allocs_end_qry - allocs_start_qry
               );
        /* remove(db_filename); */
    }

    printf("------------------------------------------------------------------------------------------------------------------------------\n");

    free(keys_to_insert);
    free(keys_to_query);
    printf("Performance Harness Finished.\n");
    return 0;
}