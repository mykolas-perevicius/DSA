#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* For memset, memcpy, perror */
#include <errno.h>
#include <assert.h> /* For assert */

/* Definition needed by Storage_read/Storage_write (formerly in btree.h) */
struct Node {
    int n;
    int leaf;
    int *key;
    int *value;
    int *c;
};

/* --- Static Global State (Singleton) --- */
static FILE *g_dataFile = NULL;
static int g_degree = 0;     /* Minimum degree 't' read from/written to file */
static long g_nodeSize = 0;  /* Calculated size of a node on disk */

/* File Header Constants */
static const int MAGIC_NUMBER = 0xBEEFCAFE;
static const int VERSION = 1;
/* Header Layout: magic(int), version(int), t(int) */
static const long HEADER_SIZE = sizeof(int) * 3;

/* Sentinel for unused slots - adjusted for int */
#define SENTINEL_VALUE ((int)0xDEADBEEF)

/* Statistics Counters */
static unsigned long g_read_count = 0;
static unsigned long g_write_count = 0;
static unsigned long g_alloc_count = 0;

/* --- Helper Functions --- */

/* Calculates node size based on t */
static long calculate_node_size(int t) {
     assert(t >= 2);
     return (long)(6 * t) * sizeof(int);
}

/* Calculates disk offset for a given node address */
static long calculate_offset(int addr) {
    if (g_nodeSize <= 0) {
        fprintf(stderr, "Storage Error: Node size not initialized or invalid.\n");
        exit(EXIT_FAILURE);
    }
    assert(addr >= 0);
    return HEADER_SIZE + (long)addr * g_nodeSize;
}

/* --- API Implementation --- */

int Storage_get_t(void) {
     if (g_dataFile == NULL) {
        fprintf(stderr, "Storage Error: Cannot get t, storage not open.\n");
        exit(EXIT_FAILURE);
     }
     if (g_degree < 2) {
         fprintf(stderr, "Storage Error: Invalid degree t=%d stored internally.\n", g_degree);
         exit(EXIT_FAILURE);
     }
    return g_degree;
}

void Storage_open(const char *fname, int t_user) {
    /* Removed file_exists variable as usage was implicit */
    int stored_t = 0;
    int magic = 0, version = 0;

    if (g_dataFile != NULL) {
        fprintf(stderr, "Storage Error: Storage already open.\n");
        exit(EXIT_FAILURE);
    }

    g_dataFile = fopen(fname, "r+b");
    if (g_dataFile != NULL) {
        /* File exists, opened in r+b mode */
        /* Read header */
        if (fseek(g_dataFile, 0, SEEK_SET) != 0) {
            perror("Storage Error: Cannot seek to header (r+b)");
            fclose(g_dataFile); g_dataFile = NULL;
            exit(EXIT_FAILURE);
        }
        if (fread(&magic, sizeof(int), 1, g_dataFile) != 1 ||
            fread(&version, sizeof(int), 1, g_dataFile) != 1 ||
            fread(&stored_t, sizeof(int), 1, g_dataFile) != 1)
        {
            fprintf(stderr, "Storage Error: Cannot read header from existing file %s\n", fname);
            if (ferror(g_dataFile)) perror("fread error");
            fclose(g_dataFile); g_dataFile = NULL;
            exit(EXIT_FAILURE);
        }

        /* Validate header */
        if (magic != MAGIC_NUMBER || version != VERSION) {
            fprintf(stderr, "Storage Error: Invalid file format or version (Magic: %x, Version: %d).\n", magic, version);
            fclose(g_dataFile); g_dataFile = NULL;
            exit(EXIT_FAILURE);
        }
        if (stored_t < 2) {
            fprintf(stderr, "Storage Error: Invalid minimum degree t=%d found in file header.\n", stored_t);
            fclose(g_dataFile); g_dataFile = NULL;
            exit(EXIT_FAILURE);
        }
        g_degree = stored_t;
        g_nodeSize = calculate_node_size(g_degree);

        /* Optional: Verify file size consistency */
        /* Reduce scope of file_size */
        {
            long file_size;
            if (fseek(g_dataFile, 0, SEEK_END) != 0) {
                 perror("Storage Error: Cannot seek to end (size check)");
                 fclose(g_dataFile); g_dataFile = NULL;
                 exit(EXIT_FAILURE);
            }
            file_size = ftell(g_dataFile);
             if (file_size < 0) {
                 perror("Storage Error: Cannot get file size (size check)");
                 fclose(g_dataFile); g_dataFile = NULL;
                 exit(EXIT_FAILURE);
            }
            if ((file_size - HEADER_SIZE) % g_nodeSize != 0) {
                fprintf(stderr, "Storage Warning: File size %ld does not align with header (t=%d, nodeSize=%ld).\n",
                        file_size, g_degree, g_nodeSize);
            }
        }

    } else {
        /* File does not exist or fopen("r+b") failed */
        if (errno != ENOENT) {
             perror("Storage Error: Cannot open existing file (r+b)");
             exit(EXIT_FAILURE);
        }

        /* Create new file with "w+b" */
        g_dataFile = fopen(fname, "w+b");
        if (g_dataFile == NULL) {
            perror("Storage Error: Cannot create new file (w+b)");
            exit(EXIT_FAILURE);
        }

        if (t_user < 2) {
             fprintf(stderr, "Storage Error: Minimum degree t must be >= 2 for new file.\n");
             fclose(g_dataFile); g_dataFile = NULL; remove(fname);
             exit(EXIT_FAILURE);
        }

        /* Write header */
        magic = MAGIC_NUMBER;
        version = VERSION;
        stored_t = t_user;
        g_degree = t_user;
        g_nodeSize = calculate_node_size(g_degree);

        if (fwrite(&magic, sizeof(int), 1, g_dataFile) != 1 ||
            fwrite(&version, sizeof(int), 1, g_dataFile) != 1 ||
            fwrite(&stored_t, sizeof(int), 1, g_dataFile) != 1)
        {
            fprintf(stderr, "Storage Error: Cannot write header to new file.\n");
            perror("fwrite");
            fclose(g_dataFile); g_dataFile = NULL; remove(fname);
            exit(EXIT_FAILURE);
        }
         if (fflush(g_dataFile) != 0) {
            perror("Storage Error: Cannot flush header");
            fclose(g_dataFile); g_dataFile = NULL; remove(fname);
            exit(EXIT_FAILURE);
        }
    }

    g_read_count = 0;
    g_write_count = 0;
    g_alloc_count = 0;
}

void Storage_close(void) {
    if (g_dataFile != NULL) {
        if (fflush(g_dataFile) != 0) {
             perror("Storage Warning: Error flushing file before close");
        }
        if (fclose(g_dataFile) != 0) {
            perror("Storage Warning: Error closing file");
        }
        g_dataFile = NULL;
        g_degree = 0;
        g_nodeSize = 0;
    }
}

int Storage_empty(void) {
    long file_size;
    if (g_dataFile == NULL) {
        fprintf(stderr, "Storage Error: Storage not open in Storage_empty.\n");
        exit(EXIT_FAILURE);
    }
    if (fflush(g_dataFile) != 0) {
         perror("Storage Warning: fflush failed in Storage_empty");
    }
    if (fseek(g_dataFile, 0, SEEK_END) != 0) {
        perror("Storage Error: fseek failed in Storage_empty");
        exit(EXIT_FAILURE);
    }
    file_size = ftell(g_dataFile);
    if (file_size < 0) {
         perror("Storage Error: ftell failed in Storage_empty");
         exit(EXIT_FAILURE);
    }
    return (file_size == HEADER_SIZE);
}

int Storage_alloc(void) {
    long file_size;
    int addr;

    if (g_dataFile == NULL) {
        fprintf(stderr, "Storage Error: Storage not open in Storage_alloc.\n");
        exit(EXIT_FAILURE);
    }
    if (g_nodeSize <= 0) {
         fprintf(stderr, "Storage Error: Invalid node size in Storage_alloc.\n");
         exit(EXIT_FAILURE);
    }
    if (fseek(g_dataFile, 0, SEEK_END) != 0) {
        perror("Storage Error: fseek to end failed in Storage_alloc");
        exit(EXIT_FAILURE);
    }
    file_size = ftell(g_dataFile);
     if (file_size < 0) {
         perror("Storage Error: ftell failed in Storage_alloc");
         exit(EXIT_FAILURE);
    }
    if ((file_size - HEADER_SIZE) % g_nodeSize != 0) {
        fprintf(stderr, "Storage Error: File size corruption detected before alloc (size %ld, header %ld, nodeSize %ld).\n",
                file_size, HEADER_SIZE, g_nodeSize);
        exit(EXIT_FAILURE);
    }

    addr = (int)((file_size - HEADER_SIZE) / g_nodeSize);

    {
        char *dummy_buffer = malloc(g_nodeSize);
        if (!dummy_buffer) {
            perror("Storage Error: Failed to allocate dummy buffer for alloc padding");
            exit(EXIT_FAILURE);
        }
        memset(dummy_buffer, 0, g_nodeSize);
        if (fwrite(dummy_buffer, g_nodeSize, 1, g_dataFile) != 1) {
             perror("Storage Error: Failed to write padding block during alloc");
             free(dummy_buffer);
             exit(EXIT_FAILURE);
        }
        free(dummy_buffer);
    }

    g_alloc_count++;
    return addr;
}

void Storage_read(int addr, struct Node *x) {
    long offset;
    size_t elements_to_read;
    size_t elements_read;
    int max_keys, max_children;

    if (g_dataFile == NULL) {
        fprintf(stderr, "Storage Error: Storage not open in Storage_read.\n");
        exit(EXIT_FAILURE);
    }
    if (x == NULL || x->key == NULL || x->value == NULL || x->c == NULL) {
         fprintf(stderr, "Storage Error: Null node or internal buffer passed to Storage_read.\n");
         exit(EXIT_FAILURE);
    }
     if (g_degree <= 1 || g_nodeSize <= 0) {
        fprintf(stderr, "Storage Error: Storage not properly initialized (t=%d, nodeSize=%ld).\n", g_degree, g_nodeSize);
        exit(EXIT_FAILURE);
     }

    max_keys = 2 * g_degree - 1;
    max_children = 2 * g_degree;
    offset = calculate_offset(addr);

    if (fseek(g_dataFile, offset, SEEK_SET) != 0) {
        perror("Storage Error: fseek failed in Storage_read");
        fprintf(stderr, "Attempted offset: %ld for address %d\n", offset, addr);
        exit(EXIT_FAILURE);
    }

    if (fread(&(x->n), sizeof(int), 1, g_dataFile) != 1 ||
        fread(&(x->leaf), sizeof(int), 1, g_dataFile) != 1) {
        fprintf(stderr, "Storage Error: Failed to read node header (n, leaf) at addr %d.\n", addr);
         if (feof(g_dataFile)) fprintf(stderr, " Read past EOF.\n");
         else if(ferror(g_dataFile)) perror(" fread error"); else fprintf(stderr, " Short read.\n");
        exit(EXIT_FAILURE);
    }

    elements_to_read = max_keys;
    elements_read = fread(x->key, sizeof(int), elements_to_read, g_dataFile);
    if (elements_read != elements_to_read) goto read_error;

    elements_read = fread(x->value, sizeof(int), elements_to_read, g_dataFile);
    if (elements_read != elements_to_read) goto read_error;

    elements_to_read = max_children;
    elements_read = fread(x->c, sizeof(int), elements_to_read, g_dataFile);
    if (elements_read != elements_to_read) goto read_error;

    g_read_count++;
    return;

read_error:
    {
        long current_pos = ftell(g_dataFile);
        long file_size_on_error = -1;

        fprintf(stderr, "Storage Error: Failed to read node data block at addr %d. Elements read: %lu / Expected: %lu\n",
                addr, (unsigned long)elements_read, (unsigned long)elements_to_read);
        if (feof(g_dataFile)) fprintf(stderr, " Read past EOF.\n");
        else if(ferror(g_dataFile)) perror(" fread error"); else fprintf(stderr, " Short read.\n");

        if (fseek(g_dataFile, 0, SEEK_END) == 0) {
             file_size_on_error = ftell(g_dataFile);
        }
        fprintf(stderr, " File size: %ld, Expected offset: %ld, Pos after failed read: %ld\n", file_size_on_error, offset, current_pos);
        exit(EXIT_FAILURE);
    }
}


void Storage_write(int addr, const struct Node *x) {
    long offset;
    size_t elements_to_write;
    size_t elements_written;
    int max_keys, max_children;

    if (g_dataFile == NULL) {
        fprintf(stderr, "Storage Error: Storage not open in Storage_write.\n");
        exit(EXIT_FAILURE);
    }
     if (x == NULL || x->key == NULL || x->value == NULL || x->c == NULL) {
         fprintf(stderr, "Storage Error: Null node or internal buffer passed to Storage_write.\n");
         exit(EXIT_FAILURE);
    }
     if (g_degree <= 1 || g_nodeSize <= 0) {
        fprintf(stderr, "Storage Error: Storage not properly initialized (t=%d, nodeSize=%ld).\n", g_degree, g_nodeSize);
        exit(EXIT_FAILURE);
     }

    max_keys = 2 * g_degree - 1;
    max_children = 2 * g_degree;
    offset = calculate_offset(addr);

    if (fseek(g_dataFile, offset, SEEK_SET) != 0) {
        perror("Storage Error: fseek failed in Storage_write");
         fprintf(stderr, "Attempted offset: %ld for address %d\n", offset, addr);
        exit(EXIT_FAILURE);
    }

    if (fwrite(&(x->n), sizeof(int), 1, g_dataFile) != 1 ||
        fwrite(&(x->leaf), sizeof(int), 1, g_dataFile) != 1) {
        fprintf(stderr, "Storage Error: Failed to write node header (n, leaf) at addr %d.\n", addr);
        perror(" fwrite error");
        exit(EXIT_FAILURE);
    }

    elements_to_write = max_keys;
    elements_written = fwrite(x->key, sizeof(int), elements_to_write, g_dataFile);
    if (elements_written != elements_to_write) goto write_error;

    elements_to_write = max_keys;
    elements_written = fwrite(x->value, sizeof(int), elements_to_write, g_dataFile);
    if (elements_written != elements_to_write) goto write_error;

    elements_to_write = max_children;
    elements_written = fwrite(x->c, sizeof(int), elements_to_write, g_dataFile);
    if (elements_written != elements_to_write) goto write_error;

    g_write_count++;
    return;

write_error:
    fprintf(stderr, "Storage Error: Failed to write node data block at addr %d. Elements written: %lu / Expected: %lu\n",
            addr, (unsigned long)elements_written, (unsigned long)elements_to_write);
    perror(" fwrite error");
    exit(EXIT_FAILURE);
}

unsigned long Storage_get_read_count(void) { return g_read_count; }
unsigned long Storage_get_write_count(void) { return g_write_count; }
unsigned long Storage_get_alloc_count(void) { return g_alloc_count; }