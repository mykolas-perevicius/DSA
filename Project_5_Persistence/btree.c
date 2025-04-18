#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* For memmove, memcpy */
#include <limits.h> /* For INT_MIN/MAX if needed */
#include <assert.h> /* For assert */

/* Required Struct Definitions */
struct Node {
    int n;
    int leaf;
    int *key;
    int *value;
    int *c;
};

struct BTree {
    int root;
    int t;
};

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
#define SENTINEL_VALUE ((int)0xDEADBEEF)
#define DELETION_SENTINEL ((int)0xDEADDEAD)

/* --- Internal Helper Functions --- */

static struct Node* BTree_allocate_node_mem(int t) {
    /* --- Declarations at top of block (ANSI C) --- */
    struct Node *x = NULL;
    int max_keys;
    int max_children;
    int i; /* Loop variable */

    /* --- Code --- */
    assert(t >= 2);
    max_keys = 2 * t - 1;
    max_children = 2 * t;

    x = malloc(sizeof(struct Node));
    if (!x) {
        perror("BTree Memory Error: Cannot allocate node struct");
        exit(EXIT_FAILURE);
    }
    x->key = malloc(max_keys * sizeof(int));
    x->value = malloc(max_keys * sizeof(int));
    x->c = malloc(max_children * sizeof(int));
    if (!x->key || !x->value || !x->c) {
        perror("BTree Memory Error: Cannot allocate node arrays");
        free(x->key); free(x->value); free(x->c); free(x);
        exit(EXIT_FAILURE);
    }

    /* Use loop variable 'i' declared above */
    for (i = 0; i < max_keys; ++i) {
        x->key[i] = SENTINEL_VALUE;
        x->value[i] = SENTINEL_VALUE;
    }
    /* Use loop variable 'i' declared above */
    for (i = 0; i < max_children; ++i) {
        x->c[i] = SENTINEL_VALUE;
    }
    x->n = 0;
    x->leaf = 1;
    return x;
}

static void BTree_free_node_mem(struct Node *x) {
    if (x) {
        free(x->key); free(x->value); free(x->c); free(x);
    }
}

static struct Node* BTree_disk_read(int t, int addr) {
    struct Node *x = BTree_allocate_node_mem(t);
    Storage_read(addr, x);
    return x;
}

static void BTree_disk_write(int addr, const struct Node *x) {
    Storage_write(addr, x);
}

static int BTree_search_and_mark_deleted_internal(int t, int addr, int k);


/* --- CLRS Algorithm Implementations --- */

static int BTree_search_internal(int t, int addr, int k, int *v_out) {
    /* --- Declarations at top of block (ANSI C) --- */
    struct Node *x = NULL;
    int found = 0;
    int i = 0;
    int child_addr;

    /* --- Code --- */
    x = BTree_disk_read(t, addr);

    /* Use 'i' declared above */
    while (i < x->n && k > x->key[i]) {
        i++;
    }
    if (i < x->n && k == x->key[i]) {
        /* Key Found - check if marked deleted */
        if (x->value[i] != DELETION_SENTINEL) {
            *v_out = x->value[i];
            found = 1;
        } else {
            /* Key found but marked deleted, treat as not found */
            found = 0;
        }
    } else if (x->leaf) {
        /* Key not found and it's a leaf */
        found = 0;
    } else {
        /* Not found here, or marked deleted, recurse into child */
        child_addr = x->c[i];
        BTree_free_node_mem(x); x = NULL;
        if (child_addr == SENTINEL_VALUE || child_addr < 0) {
             fprintf(stderr, "BTree Error: Invalid child address during search (addr=%d, i=%d).\n", addr, i);
             exit(EXIT_FAILURE);
        }
        /* Recursive call - result determines 'found' */
        return BTree_search_internal(t, child_addr, k, v_out);
    }
    if (x != NULL) { BTree_free_node_mem(x); }
    return found;
}

static void BTree_split_child(int t, int addr_x, int i) {
    /* --- Declarations at top of block (ANSI C) --- */
    int addr_y;
    int addr_z;
    int median_key;
    int median_val;
    int t_minus_1;
    struct Node *z = NULL; /* Moved declaration up */
    struct Node *x = NULL;
    struct Node *y = NULL; /* Moved declaration up */
    int j; /* Loop variable */

    /* --- Code --- */
    t_minus_1 = t - 1;
    addr_z = Storage_alloc();
    z = BTree_allocate_node_mem(t);
    x = BTree_disk_read(t, addr_x);
    addr_y = x->c[i];
     if (addr_y == SENTINEL_VALUE || addr_y < 0) {
        fprintf(stderr, "BTree Internal Error: Invalid child address in parent (addr_x=%d, i=%d) before split.\n", addr_x, i);
        BTree_free_node_mem(x); BTree_free_node_mem(z); exit(EXIT_FAILURE);
     }
    y = BTree_disk_read(t, addr_y);
    if (y->n != 2 * t - 1) {
        fprintf(stderr, "BTree Internal Error: Attempted to split non-full node y (addr=%d, n=%d, t=%d)\n", addr_y, y->n, t);
        BTree_free_node_mem(x); BTree_free_node_mem(y); BTree_free_node_mem(z);
        exit(EXIT_FAILURE);
    }
    z->leaf = y->leaf;
    z->n = t_minus_1;
    memcpy(z->key, &y->key[t], t_minus_1 * sizeof(int));
    memcpy(z->value, &y->value[t], t_minus_1 * sizeof(int));
    if (!y->leaf) { memcpy(z->c, &y->c[t], t * sizeof(int)); }
    y->n = t_minus_1;
    median_key = y->key[t_minus_1];
    median_val = y->value[t_minus_1];
    y->key[t_minus_1] = SENTINEL_VALUE;
    y->value[t_minus_1] = SENTINEL_VALUE;

    /* Use loop variable 'j' declared above */
    for (j = t; j < 2 * t - 1; ++j) {
        y->key[j] = SENTINEL_VALUE;
        y->value[j] = SENTINEL_VALUE;
    }
    if (!y->leaf) {
        /* Use loop variable 'j' declared above */
        for (j = t; j < 2 * t; ++j) {
            y->c[j] = SENTINEL_VALUE;
        }
    }
    memmove(&x->c[i + 2], &x->c[i + 1], (x->n - i) * sizeof(int));
    x->c[i + 1] = addr_z;
    memmove(&x->key[i + 1], &x->key[i], (x->n - i) * sizeof(int));
    memmove(&x->value[i + 1], &x->value[i], (x->n - i) * sizeof(int));
    x->key[i] = median_key;
    x->value[i] = median_val;
    x->n = x->n + 1;
    BTree_disk_write(addr_x, x);
    BTree_disk_write(addr_y, y);
    BTree_disk_write(addr_z, z);
    BTree_free_node_mem(x); BTree_free_node_mem(y); BTree_free_node_mem(z);
}

static void BTree_insert_nonfull(int t, int addr_x, int k, int v) {
    /* --- Declarations at top of block (ANSI C) --- */
    struct Node *x = NULL;
    int i;
    int child_addr;
    struct Node *child = NULL;
    int needs_split;

    /* --- Code --- */
    x = BTree_disk_read(t, addr_x);

    i = 0;
    while (i < x->n && k > x->key[i]) {
        i++;
    }

    if (i < x->n && k == x->key[i]) {
        /* --- Key Found: Update Value (Handles undelete) --- */
        /* No need to check if it was DELETION_SENTINEL, just overwrite */
        x->value[i] = v;
        BTree_disk_write(addr_x, x);
        BTree_free_node_mem(x);
        return; /* Update complete */
    }

    /* --- Key Not Found: Proceed with Insertion --- */
    if (x->leaf) {
        /* --- Case 1: Insert into Leaf --- */
        if (x->n > i) {
             memmove(&x->key[i + 1], &x->key[i], (x->n - i) * sizeof(int));
             memmove(&x->value[i + 1], &x->value[i], (x->n - i) * sizeof(int));
        }
        x->key[i] = k;
        x->value[i] = v;
        x->n = x->n + 1;
        BTree_disk_write(addr_x, x);
        BTree_free_node_mem(x);

    } else {
        /* --- Case 2: Recurse into Internal Node Child --- */
        child_addr = x->c[i];
        if (child_addr == SENTINEL_VALUE || child_addr < 0) {
             fprintf(stderr, "BTree Error: Invalid child address in parent (addr_x=%d, i=%d) before insert descent.\n", addr_x, i);
             BTree_free_node_mem(x); exit(EXIT_FAILURE);
        }
        BTree_free_node_mem(x); x = NULL;
        child = BTree_disk_read(t, child_addr);
        needs_split = (child->n == 2 * t - 1);
        BTree_free_node_mem(child); child = NULL;
        if (needs_split) {
            BTree_split_child(t, addr_x, i);
            x = BTree_disk_read(t, addr_x);
            if (k > x->key[i]) { i++; }
            child_addr = x->c[i];
            BTree_free_node_mem(x); x = NULL;
        }
        BTree_insert_nonfull(t, child_addr, k, v);
    }
}

static int BTree_search_and_mark_deleted_internal(int t, int addr, int k) {
    /* --- Declarations at top of block (ANSI C) --- */
    struct Node *x = NULL;
    int marked = 0;
    int i = 0;
    int child_addr;

    /* --- Code --- */
    x = BTree_disk_read(t, addr);

    /* Use 'i' declared above */
    while (i < x->n && k > x->key[i]) {
        i++;
    }

    if (i < x->n && k == x->key[i]) {
        /* Key Found - check if already marked */
        if (x->value[i] != DELETION_SENTINEL) {
            x->value[i] = DELETION_SENTINEL; /* Mark as deleted */
            BTree_disk_write(addr, x);       /* Write back */
            marked = 1;                      /* Signal success */
        } else {
            /* Already marked, treat as "not modified" */
            marked = 0;
        }
    } else if (x->leaf) {
        /* Key not found and it's a leaf */
        marked = 0;
    } else {
        /* Not found here, or already marked, recurse into child */
        child_addr = x->c[i];
        BTree_free_node_mem(x); x = NULL;
        if (child_addr == SENTINEL_VALUE || child_addr < 0) {
             fprintf(stderr, "BTree Error: Invalid child address during delete search (addr=%d, i=%d).\n", addr, i);
             exit(EXIT_FAILURE);
        }
        /* Recursive call - result determines 'marked' */
        return BTree_search_and_mark_deleted_internal(t, child_addr, k);
    }
    if (x != NULL) { BTree_free_node_mem(x); }
    return marked;
}


/* --- Public API Implementation --- */

void BTree_delete(struct BTree *bt, int k);

struct BTree BTree_open(const char *name, int t_user) {
    /* --- Declarations at top of block (ANSI C) --- */
    struct BTree bt;
    int root_addr; /* Moved declaration up */
    struct Node *root_node_mem = NULL; /* Moved declaration up */

    /* --- Code --- */
    Storage_open(name, t_user);
    bt.t = Storage_get_t();
    bt.root = 0;
    if (Storage_empty()) {
        root_addr = Storage_alloc();
        if (root_addr != 0) {
             fprintf(stderr, "BTree Error: Initial root allocation did not return address 0 (got %d).\n", root_addr);
             Storage_close(); exit(EXIT_FAILURE);
        }
        /* Can nest a block for temporary node scope if desired, but not required */
        /* { */
            root_node_mem = BTree_allocate_node_mem(bt.t);
            root_node_mem->leaf = 1; root_node_mem->n = 0;
            BTree_disk_write(root_addr, root_node_mem);
            BTree_free_node_mem(root_node_mem);
        /* } */
    }
    return bt;
}

void BTree_close(struct BTree *bt) {
    Storage_close();
    bt->root = -1;
    bt->t = 0;
}

void BTree_put(const struct BTree *bt, int k, int v) {
    /* --- Declarations at top of block (ANSI C) --- */
    int root_addr;
    int t;
    struct Node *r = NULL;
    int addr_r_new; /* Moved declaration up */
    int addr_z; /* Moved declaration up */
    struct Node *s = NULL; /* Moved declaration up */
    struct Node *z = NULL; /* Moved declaration up */
    int j; /* Loop variable */

    /* --- Code --- */
    root_addr = bt->root;
    t = bt->t;
    r = BTree_disk_read(t, root_addr);

    if (r->n == 2 * t - 1) {
        addr_r_new = Storage_alloc();
        addr_z = Storage_alloc();
        s = BTree_allocate_node_mem(t);
        z = BTree_allocate_node_mem(t);
        s->leaf = 0; s->n = 1; s->c[0] = addr_r_new; s->c[1] = addr_z;
        s->key[0] = r->key[t - 1]; s->value[0] = r->value[t - 1];
        z->leaf = r->leaf; z->n = t - 1;
        memcpy(z->key, &r->key[t], (t - 1) * sizeof(int));
        memcpy(z->value, &r->value[t], (t - 1) * sizeof(int));
        if (!r->leaf) { memcpy(z->c, &r->c[t], t * sizeof(int)); }
        assert(r != NULL);
        r->n = t - 1;
        /* Use loop variable 'j' declared above */
        for(j = t - 1; j < 2*t - 1; ++j) {
            r->key[j] = SENTINEL_VALUE;
            r->value[j] = SENTINEL_VALUE;
        }
        if (!r->leaf) {
            /* Use loop variable 'j' declared above */
            for(j = t; j < 2*t; ++j) {
                 r->c[j] = SENTINEL_VALUE;
            }
        }
        BTree_disk_write(addr_r_new, r);
        BTree_disk_write(addr_z, z);
        BTree_disk_write(root_addr, s);
        BTree_free_node_mem(r); r = NULL;
        BTree_free_node_mem(s); s = NULL;
        BTree_free_node_mem(z); z = NULL;
        BTree_insert_nonfull(t, root_addr, k, v);
    } else {
        BTree_free_node_mem(r); r = NULL;
        BTree_insert_nonfull(t, root_addr, k, v);
    }
}

void BTree_get(const struct BTree *bt, int k, int *v) {
    /* --- Declarations at top of block (ANSI C) --- */
    int root_addr;
    int t;

    /* --- Code --- */
    root_addr = bt->root;
    t = bt->t;
    assert(bt != NULL); assert(v != NULL); assert(t >= 2);
    (void) BTree_search_internal(t, root_addr, k, v);
}

void BTree_delete(struct BTree *bt, int k) {
    /* --- Declarations at top (ANSI C) --- */
    int root_addr;
    int t;

    /* --- Code --- */
    assert(bt != NULL);
    root_addr = bt->root;
    t = bt->t;
    assert(t >= 2);

    /* Call internal helper to find and mark */
    (void) BTree_search_and_mark_deleted_internal(t, root_addr, k);
    /* We don't return anything, it's best effort marking */
}