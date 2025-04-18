#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* For memmove, memcpy */
#include <limits.h> /* For INT_MIN/MAX */
#include <assert.h> /* For assert */

/* Required Struct Definitions */
struct Node { int n; int leaf; int *key; int *value; int *c; };
struct BTree { int root; int t; };

/* Required Prototypes from storage.c */
void          Storage_open (const char *fname, int t);
void          Storage_close(void);
int           Storage_empty(void);
int           Storage_alloc(void);
void          Storage_read (int addr, struct Node *x);
void          Storage_write(int addr, const struct Node *x);
unsigned long Storage_get_read_count(void);
unsigned long Storage_get_write_count(void);
unsigned long Storage_get_alloc_count(void);
int           Storage_get_t(void);

/* --- Constants --- */
/* Sentinel for unused key/value slots */
#define SENTINEL_VALUE ((int)0xDEADBEEF)
/* Sentinel value for marking a key as deleted */
#define DELETION_SENTINEL ((int)0xDEADDEAD)
/* Sentinel for invalid/unused child address pointers */
#define NULL_ADDR (-1)

/* --- Internal Helper Functions --- */

/* Allocate memory for a Node structure AND its internal arrays */
static struct Node* BTree_allocate_node_mem(int t) {
    struct Node *x = NULL; int max_keys; int max_children; int i;
    assert(t >= 2); max_keys = 2 * t - 1; max_children = 2 * t;
    x = malloc(sizeof(struct Node));
    if (!x) { perror("BTree Memory Error"); exit(EXIT_FAILURE); }
    x->key = malloc(max_keys * sizeof(int));
    x->value = malloc(max_keys * sizeof(int));
    x->c = malloc(max_children * sizeof(int));
    if (!x->key || !x->value || !x->c) {
        perror("BTree Memory Error"); free(x->key); free(x->value); free(x->c); free(x); exit(EXIT_FAILURE); }
    for (i = 0; i < max_keys; ++i) { x->key[i] = SENTINEL_VALUE; x->value[i] = SENTINEL_VALUE; }
    for (i = 0; i < max_children; ++i) { x->c[i] = NULL_ADDR; } /* Use NULL_ADDR */
    x->n = 0; x->leaf = 1; return x;
}

static void BTree_free_node_mem(struct Node *x) {
    if (x) { free(x->key); free(x->value); free(x->c); free(x); }
}

static struct Node* BTree_disk_read(int t, int addr) {
    struct Node *x = BTree_allocate_node_mem(t);
    Storage_read(addr, x); return x;
}

static void BTree_disk_write(int addr, const struct Node *x) {
    Storage_write(addr, x);
}


/* --- CLRS Algorithm Implementations (ANSI C, Single-Node Buffer) --- */

/* Internal search: checks for DELETION_SENTINEL */
static int BTree_search_internal(int t, int addr, int k, int *v_out) {
    struct Node *x = NULL; int found = 0; int i = 0; int child_addr;
    x = BTree_disk_read(t, addr);
    while (i < x->n && k > x->key[i]) { i++; }
    if (i < x->n && k == x->key[i]) {
        if (x->value[i] != DELETION_SENTINEL) {
            *v_out = x->value[i]; found = 1;
        } else { found = 0; /* Marked deleted */ }
    } else if (x->leaf) { found = 0; }
    else { /* Not found or deleted, recurse */
        child_addr = x->c[i];
        BTree_free_node_mem(x); x = NULL; /* Free BEFORE recursion */
        if (child_addr == NULL_ADDR) { /* Use NULL_ADDR */
             fprintf(stderr, "BTree Error: Invalid child address during search (addr=%d, i=%d).\n", addr, i); exit(EXIT_FAILURE);
        }
        return BTree_search_internal(t, child_addr, k, v_out);
    }
    if (x != NULL) { BTree_free_node_mem(x); } return found;
}


/* Internal mark-deleted */
static int BTree_search_and_mark_deleted_internal(int t, int addr, int k) {
    struct Node *x = NULL; int marked = 0; int i = 0; int child_addr;
    x = BTree_disk_read(t, addr);
    while (i < x->n && k > x->key[i]) { i++; }
    if (i < x->n && k == x->key[i]) {
        if (x->value[i] != DELETION_SENTINEL) {
            x->value[i] = DELETION_SENTINEL; BTree_disk_write(addr, x); marked = 1;
        } else { marked = 0; /* Already marked */ }
    } else if (x->leaf) { marked = 0; }
    else { /* Not found or already marked, recurse */
        child_addr = x->c[i];
        BTree_free_node_mem(x); x = NULL; /* Free BEFORE recursion */
        if (child_addr == NULL_ADDR) { /* Use NULL_ADDR */
             fprintf(stderr, "BTree Error: Invalid child address during delete search (addr=%d, i=%d).\n", addr, i); exit(EXIT_FAILURE);
        }
        return BTree_search_and_mark_deleted_internal(t, child_addr, k);
    }
    if (x != NULL) { BTree_free_node_mem(x); } return marked;
}

/* B-TREE-SPLIT-CHILD (Strict Memory Budget Version) */
static void BTree_split_child(int t, int addr_x, int i) {
    /* --- Declarations (ANSI C) --- */
    struct Node *y = NULL; /* Only node buffer needed temporarily */
    int addr_y;
    int addr_z;
    int median_key, median_val;
    int t_minus_1;
    /* Stack buffers for temporary storage */
    int *z_keys = NULL;
    int *z_values = NULL;
    int *z_children = NULL; /* Only if internal node */
    int j;
    int y_is_leaf;

    /* --- Code --- */
    t_minus_1 = t - 1;

    /* 1. Read child node y */
    /* (Assume parent x is NOT in memory yet) */
    /* First, we need the parent temporarily just to get addr_y */
    {
        struct Node *temp_x = BTree_disk_read(t, addr_x);
        addr_y = temp_x->c[i];
        BTree_free_node_mem(temp_x); /* Free parent immediately */
    }
    if (addr_y == NULL_ADDR) { fprintf(stderr, "BTree Error: Invalid child address in parent (addr_x=%d, i=%d) before split.\n", addr_x, i); exit(EXIT_FAILURE); }

    y = BTree_disk_read(t, addr_y); /* Read child */

    /* Sanity check y */
    if (y->n != 2 * t - 1) { fprintf(stderr, "BTree Internal Error: Attempted to split non-full node y (addr=%d, n=%d, t=%d)\n", addr_y, y->n, t); BTree_free_node_mem(y); exit(EXIT_FAILURE); }
    y_is_leaf = y->leaf; /* Store leaf status before modifying y */

    /* 2. Allocate stack buffers (careful with size for large t!) */
    /* Note: Large t might cause stack overflow here. A heap buffer might */
    /* be needed if t is very large, slightly bending the rules. */
    /* Assume t is reasonable for stack allocation here. */
    z_keys = malloc(t_minus_1 * sizeof(int));
    z_values = malloc(t_minus_1 * sizeof(int));
    if (!y_is_leaf) { z_children = malloc(t * sizeof(int)); }
    if (!z_keys || !z_values || (!y_is_leaf && !z_children)) {
        perror("BTree Memory Error: Failed to allocate stack buffers for split");
        free(z_keys); free(z_values); free(z_children); BTree_free_node_mem(y); exit(EXIT_FAILURE);
    }

    /* 3. Copy upper half of y to stack buffers */
    memcpy(z_keys, &y->key[t], t_minus_1 * sizeof(int));
    memcpy(z_values, &y->value[t], t_minus_1 * sizeof(int));
    if (!y_is_leaf) { memcpy(z_children, &y->c[t], t * sizeof(int)); }

    /* 4. Get median key/value (before clearing y) */
    median_key = y->key[t_minus_1];
    median_val = y->value[t_minus_1];

    /* 5. Modify y (lower half) */
    y->n = t_minus_1;
    /* Clear upper half keys/values/children in 'y' with sentinels/NULL_ADDR */
    for (j = t_minus_1; j < 2*t - 1; ++j) { y->key[j] = SENTINEL_VALUE; y->value[j] = SENTINEL_VALUE; }
    if (!y_is_leaf) { for (j = t; j < 2*t; ++j) { y->c[j] = NULL_ADDR; } }

    /* 6. Write modified y back */
    BTree_disk_write(addr_y, y);
    BTree_free_node_mem(y); y = NULL; /* Free y memory */

    /* 7. Allocate disk space and memory for new node z */
    addr_z = Storage_alloc();
    /* Use 'y' variable temporarily for node z */
    y = BTree_allocate_node_mem(t); /* Re-use 'y' pointer for node 'z' */
    y->leaf = y_is_leaf;
    y->n = t_minus_1;

    /* 8. Fill z from stack buffers */
    memcpy(y->key, z_keys, t_minus_1 * sizeof(int));
    memcpy(y->value, z_values, t_minus_1 * sizeof(int));
    if (!y_is_leaf) { memcpy(y->c, z_children, t * sizeof(int)); }

    /* 9. Write z to disk */
    BTree_disk_write(addr_z, y);
    BTree_free_node_mem(y); y = NULL; /* Free z memory */

    /* 10. Free stack buffers */
    free(z_keys); free(z_values); free(z_children);

    /* 11. Read parent x */
    /* Use 'y' variable temporarily for node x */
    y = BTree_disk_read(t, addr_x); /* Re-use 'y' pointer for node 'x' */

    /* 12. Modify parent x */
    /* Make space for new child ptr in x */
    memmove(&y->c[i + 2], &y->c[i + 1], (y->n - i) * sizeof(int));
    y->c[i + 1] = addr_z; /* Link new node z */
    /* Make space for median key/value */
    memmove(&y->key[i + 1], &y->key[i], (y->n - i) * sizeof(int));
    memmove(&y->value[i + 1], &y->value[i], (y->n - i) * sizeof(int));
    y->key[i] = median_key; /* Insert median */
    y->value[i] = median_val;
    y->n = y->n + 1;

    /* 13. Write modified parent x back */
    BTree_disk_write(addr_x, y);
    BTree_free_node_mem(y); y = NULL; /* Free x memory */
}


/* B-TREE-INSERT-NONFULL (Checks for update/undelete) */
static void BTree_insert_nonfull(int t, int addr_x, int k, int v) {
    struct Node *x = NULL; int i; int child_addr; struct Node *child = NULL; int needs_split;
    x = BTree_disk_read(t, addr_x);
    i = 0; while (i < x->n && k > x->key[i]) { i++; }

    if (i < x->n && k == x->key[i]) { /* Key Found: Update */
        x->value[i] = v; BTree_disk_write(addr_x, x); BTree_free_node_mem(x); return;
    }
    /* Key Not Found: Insert */
    if (x->leaf) { /* Case 1: Leaf */
        if (x->n > i) { memmove(&x->key[i + 1], &x->key[i], (x->n - i) * sizeof(int)); memmove(&x->value[i + 1], &x->value[i], (x->n - i) * sizeof(int)); }
        x->key[i] = k; x->value[i] = v; x->n = x->n + 1;
        BTree_disk_write(addr_x, x); BTree_free_node_mem(x);
    } else { /* Case 2: Internal */
        child_addr = x->c[i];
        if (child_addr == NULL_ADDR) { fprintf(stderr, "BTree Error: Invalid child address (insert descent).\n"); BTree_free_node_mem(x); exit(EXIT_FAILURE); }
        BTree_free_node_mem(x); x = NULL; /* Free parent BEFORE child read */
        child = BTree_disk_read(t, child_addr); needs_split = (child->n == 2 * t - 1);
        BTree_free_node_mem(child); child = NULL; /* Free child */
        if (needs_split) {
            BTree_split_child(t, addr_x, i); /* Split handles its own memory */
            /* Re-read parent needed to find correct child after split */
            x = BTree_disk_read(t, addr_x);
            if (k > x->key[i]) { i++; } /* Check key that moved up */
            child_addr = x->c[i];
            BTree_free_node_mem(x); x = NULL; /* Free parent again */
             if (child_addr == NULL_ADDR) { fprintf(stderr, "BTree Error: Invalid child address after split.\n"); exit(EXIT_FAILURE); }
        }
        BTree_insert_nonfull(t, child_addr, k, v); /* Recurse */
    }
}


/* --- Public API Implementation --- */
void BTree_delete(struct BTree *bt, int k); /* Prototype */

struct BTree BTree_open(const char *name, int t_user) {
    struct BTree bt; int root_addr; struct Node *root_node_mem = NULL;
    Storage_open(name, t_user);
    bt.t = Storage_get_t(); bt.root = 0;
    if (Storage_empty()) {
        root_addr = Storage_alloc(); if (root_addr != 0) { fprintf(stderr, "BTree Error: Initial root alloc not addr 0.\n"); Storage_close(); exit(EXIT_FAILURE); }
        root_node_mem = BTree_allocate_node_mem(bt.t); root_node_mem->leaf = 1; root_node_mem->n = 0;
        BTree_disk_write(root_addr, root_node_mem); BTree_free_node_mem(root_node_mem);
    } return bt;
}

void BTree_close(struct BTree *bt) { Storage_close(); bt->root = -1; bt->t = 0; }

/* BTree_put (Strict Memory Budget Root Split Version) */
void BTree_put(const struct BTree *bt, int k, int v) {
    int root_addr = bt->root; int t = bt->t;
    struct Node *r = NULL; /* Only node buffer needed */
    int addr_r_new; int addr_z;
    /* Stack buffers for root split */
    int *z_keys = NULL; int *z_values = NULL; int *z_children = NULL;
    int median_key, median_val;
    int root_is_leaf;
    int j;

    r = BTree_disk_read(t, root_addr);

    if (r->n == 2 * t - 1) { /* Root is full, handle split */
        /* 1. Allocate disk space for the two children */
        addr_r_new = Storage_alloc();
        addr_z = Storage_alloc();

        /* 2. Allocate stack buffers */
        z_keys = malloc((t - 1) * sizeof(int));
        z_values = malloc((t - 1) * sizeof(int));
        root_is_leaf = r->leaf;
        if (!root_is_leaf) { z_children = malloc(t * sizeof(int)); }
        if (!z_keys || !z_values || (!root_is_leaf && !z_children)) {
             perror("BTree Memory Error: Failed stack buffers for root split");
             free(z_keys); free(z_values); free(z_children); BTree_free_node_mem(r); exit(EXIT_FAILURE);
        }

        /* 3. Copy upper half of r to stack buffers */
        memcpy(z_keys, &r->key[t], (t - 1) * sizeof(int));
        memcpy(z_values, &r->value[t], (t - 1) * sizeof(int));
        if (!root_is_leaf) { memcpy(z_children, &r->c[t], t * sizeof(int)); }

        /* 4. Get median key/value */
        median_key = r->key[t - 1];
        median_val = r->value[t - 1];

        /* 5. Modify original root 'r' to become the left child */
        r->n = t - 1;
        /* Clear upper half in 'r' */
        for(j = t - 1; j < 2*t - 1; ++j) { r->key[j] = SENTINEL_VALUE; r->value[j] = SENTINEL_VALUE; }
        if (!root_is_leaf) { for(j = t; j < 2*t; ++j) { r->c[j] = NULL_ADDR; } }

        /* 6. Write modified r to its NEW address */
        BTree_disk_write(addr_r_new, r);
        BTree_free_node_mem(r); r = NULL; /* Free r's memory */

        /* 7. Allocate memory for new sibling z (use 'r' pointer temporarily) */
        r = BTree_allocate_node_mem(t);
        r->leaf = root_is_leaf; r->n = t - 1;

        /* 8. Fill z from stack buffers */
        memcpy(r->key, z_keys, (t - 1) * sizeof(int));
        memcpy(r->value, z_values, (t - 1) * sizeof(int));
        if (!root_is_leaf) { memcpy(r->c, z_children, t * sizeof(int)); }

        /* 9. Write z to its address */
        BTree_disk_write(addr_z, r);
        BTree_free_node_mem(r); r = NULL; /* Free z's memory */

        /* 10. Free stack buffers */
        free(z_keys); free(z_values); free(z_children);

        /* 11. Allocate memory for new root s (use 'r' pointer temporarily) */
        r = BTree_allocate_node_mem(t);
        r->leaf = 0; r->n = 1;
        r->key[0] = median_key; r->value[0] = median_val;
        r->c[0] = addr_r_new; r->c[1] = addr_z;

        /* 12. Write new root s to address 0 */
        BTree_disk_write(root_addr /* 0 */, r);
        BTree_free_node_mem(r); r = NULL; /* Free s's memory */

        /* 13. Insertion must now start from the new root */
        BTree_insert_nonfull(t, root_addr, k, v);

    } else { /* Root is not full */
        BTree_free_node_mem(r); r = NULL;
        BTree_insert_nonfull(t, root_addr, k, v);
    }
}


void BTree_get(const struct BTree *bt, int k, int *v) {
    int root_addr; int t;
    assert(bt != NULL); assert(v != NULL); assert(bt->t >= 2);
    root_addr = bt->root; t = bt->t;
    (void) BTree_search_internal(t, root_addr, k, v);
}

void BTree_delete(struct BTree *bt, int k) {
    int root_addr; int t;
    assert(bt != NULL); assert(bt->t >= 2);
    root_addr = bt->root; t = bt->t;
    (void) BTree_search_and_mark_deleted_internal(t, root_addr, k);
}