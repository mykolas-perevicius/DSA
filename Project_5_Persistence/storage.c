#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* For memset, memcpy, perror */
#include <errno.h>
#include <assert.h> /* For assert */

/* Required struct definition (repeated for no-header build) */
struct Node {
    int n;
    int leaf;
    int *key;
    int *value;
    int *c;
};

/* --- Combined Static Global State (Singleton) --- */
/* Combine core state variables into one struct */
static struct {
    FILE *dataFile;
    int degree;      /* Minimum degree 't' */
    long nodeSize;   /* Calculated size of a node on disk */
} g_storage = { NULL, 0, 0 };

/* Combine statistics counters into one struct */
static struct {
    unsigned long reads;
    unsigned long writes;
    unsigned long allocs;
} g_stats = { 0, 0, 0 };

/* --- Constants --- */
static const int MAGIC_NUMBER = 0xBEEFCAFE;
static const int VERSION = 1;
/* Header Layout: magic(int), version(int), t(int) */
static const long HEADER_SIZE = sizeof(int) * 3;

/* --- Helper Functions --- */

/* Calculates node size based on t */
static long calculate_node_size(int t) {
     assert(t >= 2);
     /* n(int), leaf(int), key[(2t-1)*int], value[(2t-1)*int], c[2t*int] = 6t*sizeof(int) */
     return (long)(6 * t) * sizeof(int);
}

/* Calculates disk offset for a given node address */
static long calculate_offset(int addr) {
    /* Access global state via struct */
    if (g_storage.nodeSize <= 0) {
        fprintf(stderr, "Storage Error: Node size not initialized or invalid.\n");
        exit(EXIT_FAILURE);
    }
    assert(addr >= 0);
    return HEADER_SIZE + (long)addr * g_storage.nodeSize;
}

/* --- API Implementation --- */

int Storage_get_t(void) {
     if (g_storage.dataFile == NULL) {
        fprintf(stderr, "Storage Error: Cannot get t, storage not open.\n");
        exit(EXIT_FAILURE);
     }
     if (g_storage.degree < 2) {
         fprintf(stderr, "Storage Error: Invalid degree t=%d stored internally.\n", g_storage.degree);
         exit(EXIT_FAILURE);
     }
    return g_storage.degree;
}

void Storage_open(const char *fname, int t_user) {
    int stored_t = 0;
    int magic = 0, version = 0;

    if (g_storage.dataFile != NULL) {
        fprintf(stderr, "Storage Error: Storage already open.\n");
        exit(EXIT_FAILURE);
    }

    /* Try opening existing file first */
    g_storage.dataFile = fopen(fname, "r+b");
    if (g_storage.dataFile != NULL) {
        /* File exists */
        if (fseek(g_storage.dataFile, 0, SEEK_SET) != 0) {
            perror("Storage Error: Cannot seek to header (r+b)");
            fclose(g_storage.dataFile); g_storage.dataFile = NULL; exit(EXIT_FAILURE);
        }
        if (fread(&magic, sizeof(int), 1, g_storage.dataFile) != 1 ||
            fread(&version, sizeof(int), 1, g_storage.dataFile) != 1 ||
            fread(&stored_t, sizeof(int), 1, g_storage.dataFile) != 1)
        {
            fprintf(stderr, "Storage Error: Cannot read header from existing file %s\n", fname);
            if (ferror(g_storage.dataFile)) perror("fread error");
            fclose(g_storage.dataFile); g_storage.dataFile = NULL; exit(EXIT_FAILURE);
        }
        if (magic != MAGIC_NUMBER || version != VERSION) {
            fprintf(stderr, "Storage Error: Invalid file format or version (Magic: %x, Version: %d).\n", magic, version);
            fclose(g_storage.dataFile); g_storage.dataFile = NULL; exit(EXIT_FAILURE);
        }
        if (stored_t < 2) {
            fprintf(stderr, "Storage Error: Invalid minimum degree t=%d found in file header.\n", stored_t);
            fclose(g_storage.dataFile); g_storage.dataFile = NULL; exit(EXIT_FAILURE);
        }
        g_storage.degree = stored_t;
        g_storage.nodeSize = calculate_node_size(g_storage.degree);

        /* Check file size consistency */
        {
            long file_size;
            if (fseek(g_storage.dataFile, 0, SEEK_END) != 0) {
                 perror("Storage Error: Cannot seek to end (size check)");
                 fclose(g_storage.dataFile); g_storage.dataFile = NULL; exit(EXIT_FAILURE);
            }
            file_size = ftell(g_storage.dataFile);
             if (file_size < 0) {
                 perror("Storage Error: Cannot get file size (size check)");
                 fclose(g_storage.dataFile); g_storage.dataFile = NULL; exit(EXIT_FAILURE);
            }
            if ((file_size - HEADER_SIZE) % g_storage.nodeSize != 0) {
                fprintf(stderr, "Storage Warning: File size %ld does not align with header (t=%d, nodeSize=%ld).\n",
                        file_size, g_storage.degree, g_storage.nodeSize);
            }
        }

    } else {
        /* File doesn't exist or couldn't open r+b */
        if (errno != ENOENT) {
             perror("Storage Error: Cannot open existing file (r+b)"); exit(EXIT_FAILURE);
        }
        /* Create new file */
        g_storage.dataFile = fopen(fname, "w+b");
        if (g_storage.dataFile == NULL) {
            perror("Storage Error: Cannot create new file (w+b)"); exit(EXIT_FAILURE);
        }
        if (t_user < 2) {
             fprintf(stderr, "Storage Error: Minimum degree t must be >= 2 for new file.\n");
             fclose(g_storage.dataFile); g_storage.dataFile = NULL; remove(fname); exit(EXIT_FAILURE);
        }
        magic = MAGIC_NUMBER; version = VERSION; stored_t = t_user;
        g_storage.degree = t_user;
        g_storage.nodeSize = calculate_node_size(g_storage.degree);
        if (fwrite(&magic, sizeof(int), 1, g_storage.dataFile) != 1 ||
            fwrite(&version, sizeof(int), 1, g_storage.dataFile) != 1 ||
            fwrite(&stored_t, sizeof(int), 1, g_storage.dataFile) != 1)
        {
            fprintf(stderr, "Storage Error: Cannot write header to new file.\n");
            perror("fwrite"); fclose(g_storage.dataFile); g_storage.dataFile = NULL; remove(fname); exit(EXIT_FAILURE);
        }
         if (fflush(g_storage.dataFile) != 0) {
            perror("Storage Error: Cannot flush header");
            fclose(g_storage.dataFile); g_storage.dataFile = NULL; remove(fname); exit(EXIT_FAILURE);
        }
    }

    /* Reset statistics */
    g_stats.reads = 0;
    g_stats.writes = 0;
    g_stats.allocs = 0;
}

void Storage_close(void) {
    if (g_storage.dataFile != NULL) {
        if (fflush(g_storage.dataFile) != 0) {
             perror("Storage Warning: Error flushing file before close");
        }
        if (fclose(g_storage.dataFile) != 0) {
            perror("Storage Warning: Error closing file");
        }
        /* Reset global state */
        g_storage.dataFile = NULL;
        g_storage.degree = 0;
        g_storage.nodeSize = 0;
    }
}

int Storage_empty(void) {
    long file_size;
    if (g_storage.dataFile == NULL) {
        fprintf(stderr, "Storage Error: Storage not open in Storage_empty.\n");
        exit(EXIT_FAILURE);
    }
    if (fflush(g_storage.dataFile) != 0) {
         perror("Storage Warning: fflush failed in Storage_empty");
    }
    if (fseek(g_storage.dataFile, 0, SEEK_END) != 0) {
        perror("Storage Error: fseek failed in Storage_empty"); exit(EXIT_FAILURE);
    }
    file_size = ftell(g_storage.dataFile);
    if (file_size < 0) {
         perror("Storage Error: ftell failed in Storage_empty"); exit(EXIT_FAILURE);
    }
    return (file_size == HEADER_SIZE);
}

/* Storage_alloc: Use efficient fseek/fputc method */
int Storage_alloc(void) {
    long file_size;
    int addr;
    long target_offset;

    if (g_storage.dataFile == NULL) {
        fprintf(stderr, "Storage Error: Storage not open in Storage_alloc.\n"); exit(EXIT_FAILURE);
    }
    if (g_storage.nodeSize <= 0) {
         fprintf(stderr, "Storage Error: Invalid node size in Storage_alloc.\n"); exit(EXIT_FAILURE);
    }
    if (fseek(g_storage.dataFile, 0, SEEK_END) != 0) {
        perror("Storage Error: fseek to end failed in Storage_alloc"); exit(EXIT_FAILURE);
    }
    file_size = ftell(g_storage.dataFile);
     if (file_size < 0) {
         perror("Storage Error: ftell failed in Storage_alloc"); exit(EXIT_FAILURE);
    }
    if ((file_size - HEADER_SIZE) % g_storage.nodeSize != 0) {
        fprintf(stderr, "Storage Error: File size corruption detected before alloc (size %ld, header %ld, nodeSize %ld).\n",
                file_size, HEADER_SIZE, g_storage.nodeSize); exit(EXIT_FAILURE);
    }

    addr = (int)((file_size - HEADER_SIZE) / g_storage.nodeSize);

    /* Efficiently extend file: seek to one byte before end of new block and write null */
    target_offset = calculate_offset(addr) + g_storage.nodeSize - 1;
    if (fseek(g_storage.dataFile, target_offset, SEEK_SET) != 0) {
        perror("Storage Error: fseek to target offset failed in Storage_alloc"); exit(EXIT_FAILURE);
    }
    if (fputc('\0', g_storage.dataFile) == EOF) {
        perror("Storage Error: fputc failed to extend file in Storage_alloc"); exit(EXIT_FAILURE);
    }

    /* Optional: Ensure write is effective */
    /* fflush(g_storage.dataFile); */

    g_stats.allocs++;
    return addr;
}


void Storage_read(int addr, struct Node *x) {
    long offset;
    size_t elements_to_read;
    size_t elements_read;
    int max_keys, max_children;

    if (g_storage.dataFile == NULL) { fprintf(stderr, "Storage Error: Storage not open in Storage_read.\n"); exit(EXIT_FAILURE); }
    if (x == NULL || x->key == NULL || x->value == NULL || x->c == NULL) { fprintf(stderr, "Storage Error: Null node or internal buffer passed to Storage_read.\n"); exit(EXIT_FAILURE); }
    if (g_storage.degree <= 1 || g_storage.nodeSize <= 0) { fprintf(stderr, "Storage Error: Storage not properly initialized (t=%d, nodeSize=%ld).\n", g_storage.degree, g_storage.nodeSize); exit(EXIT_FAILURE); }

    max_keys = 2 * g_storage.degree - 1;
    max_children = 2 * g_storage.degree;
    offset = calculate_offset(addr);

    if (fseek(g_storage.dataFile, offset, SEEK_SET) != 0) { perror("Storage Error: fseek failed in Storage_read"); fprintf(stderr, "Attempted offset: %ld for address %d\n", offset, addr); exit(EXIT_FAILURE); }

    if (fread(&(x->n), sizeof(int), 1, g_storage.dataFile) != 1 || fread(&(x->leaf), sizeof(int), 1, g_storage.dataFile) != 1) {
        fprintf(stderr, "Storage Error: Failed to read node header (n, leaf) at addr %d.\n", addr);
        if (feof(g_storage.dataFile)) fprintf(stderr, " Read past EOF.\n"); else if(ferror(g_storage.dataFile)) perror(" fread error"); else fprintf(stderr, " Short read.\n"); exit(EXIT_FAILURE);
    }
    elements_to_read = max_keys;
    elements_read = fread(x->key, sizeof(int), elements_to_read, g_storage.dataFile); if (elements_read != elements_to_read) goto read_error;
    elements_read = fread(x->value, sizeof(int), elements_to_read, g_storage.dataFile); if (elements_read != elements_to_read) goto read_error;
    elements_to_read = max_children;
    elements_read = fread(x->c, sizeof(int), elements_to_read, g_storage.dataFile); if (elements_read != elements_to_read) goto read_error;

    g_stats.reads++;
    return;

read_error:
    { long current_pos = ftell(g_storage.dataFile); long file_size_on_error = -1;
        fprintf(stderr, "Storage Error: Failed to read node data block at addr %d. Elements read: %lu / Expected: %lu\n", addr, (unsigned long)elements_read, (unsigned long)elements_to_read);
        if (feof(g_storage.dataFile)) fprintf(stderr, " Read past EOF.\n"); else if(ferror(g_storage.dataFile)) perror(" fread error"); else fprintf(stderr, " Short read.\n");
        if (fseek(g_storage.dataFile, 0, SEEK_END) == 0) { file_size_on_error = ftell(g_storage.dataFile); }
        fprintf(stderr, " File size: %ld, Expected offset: %ld, Pos after failed read: %ld\n", file_size_on_error, offset, current_pos);
        exit(EXIT_FAILURE);
    }
}


void Storage_write(int addr, const struct Node *x) {
    long offset;
    size_t elements_to_write;
    size_t elements_written;
    int max_keys, max_children;

    if (g_storage.dataFile == NULL) { fprintf(stderr, "Storage Error: Storage not open in Storage_write.\n"); exit(EXIT_FAILURE); }
    if (x == NULL || x->key == NULL || x->value == NULL || x->c == NULL) { fprintf(stderr, "Storage Error: Null node or internal buffer passed to Storage_write.\n"); exit(EXIT_FAILURE); }
    if (g_storage.degree <= 1 || g_storage.nodeSize <= 0) { fprintf(stderr, "Storage Error: Storage not properly initialized (t=%d, nodeSize=%ld).\n", g_storage.degree, g_storage.nodeSize); exit(EXIT_FAILURE); }

    max_keys = 2 * g_storage.degree - 1;
    max_children = 2 * g_storage.degree;
    offset = calculate_offset(addr);

    if (fseek(g_storage.dataFile, offset, SEEK_SET) != 0) { perror("Storage Error: fseek failed in Storage_write"); fprintf(stderr, "Attempted offset: %ld for address %d\n", offset, addr); exit(EXIT_FAILURE); }

    if (fwrite(&(x->n), sizeof(int), 1, g_storage.dataFile) != 1 || fwrite(&(x->leaf), sizeof(int), 1, g_storage.dataFile) != 1) {
        fprintf(stderr, "Storage Error: Failed to write node header (n, leaf) at addr %d.\n", addr); perror(" fwrite error"); exit(EXIT_FAILURE);
    }
    elements_to_write = max_keys;
    elements_written = fwrite(x->key, sizeof(int), elements_to_write, g_storage.dataFile); if (elements_written != elements_to_write) goto write_error;
    elements_written = fwrite(x->value, sizeof(int), elements_to_write, g_storage.dataFile); if (elements_written != elements_to_write) goto write_error;
    elements_to_write = max_children;
    elements_written = fwrite(x->c, sizeof(int), elements_to_write, g_storage.dataFile); if (elements_written != elements_to_write) goto write_error;

    g_stats.writes++;
    return;

write_error:
    fprintf(stderr, "Storage Error: Failed to write node data block at addr %d. Elements written: %lu / Expected: %lu\n", addr, (unsigned long)elements_written, (unsigned long)elements_to_write);
    perror(" fwrite error"); exit(EXIT_FAILURE);
}


/* --- Statistics Accessors --- */
unsigned long Storage_get_read_count(void) { return g_stats.reads; }
unsigned long Storage_get_write_count(void) { return g_stats.writes; }
unsigned long Storage_get_alloc_count(void) { return g_stats.allocs; }