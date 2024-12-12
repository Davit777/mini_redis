#include <assert.h>
#include <stdlib.h>

#include "hashtable.h"

#define RESIZING_WORK 128
#define MAX_LOAD_FACTOR 8
#define MIN_CAPACITY 4

static inline void h_init(Hash_Table* htab, const size_t n)
{
    // Must be power of 2
    assert(n > 0 && ((n - 1) & n) == 0);
    htab->table = (Hash_Node**)calloc(sizeof(Hash_Node *), n);
    htab->mask = n - 1;
    htab->size = 0;
}

static inline void h_insert(Hash_Table* htab, Hash_Node* node)
{
    const size_t pos = node->hcode & htab->mask;
    Hash_Node* next = htab->table[pos];
    node->next = next;
    htab->table[pos] = node;
    ++htab->size;
}

static inline Hash_Node** h_lookup(Hash_Table* htab, Hash_Node* key, bool (*eq)(Hash_Node *, Hash_Node *))
{
    if (NULL == htab->table) {
        return NULL;
    }

    const size_t pos = key->hcode & htab->mask;
    Hash_Node **from = &htab->table[pos];
    for (Hash_Node* current; (current = *from) != NULL; from = &current->next) {
        if (current->hcode == key->hcode && eq(current, key)) {
            return from;
        }
    }
    return NULL;
}

static inline Hash_Node* h_detach(Hash_Table* htab, Hash_Node** from)
{
    Hash_Node* node = *from;
    *from = node->next;
    --htab->size;
    return node;
}

static inline void hm_resizing_helper(Hash_Map* hmap)
{
    size_t nwork = 0;
    while (nwork < RESIZING_WORK && hmap->table2.size > 0) {
        Hash_Node** from = &hmap->table2.table[hmap->resizing_pos];
        if (*from != NULL) {
            ++hmap->resizing_pos;
            continue;
        }
        h_insert(&hmap->table1, h_detach(&hmap->table2, from));
        ++nwork;
    }
    if (hmap->table2.size == 0 && hmap->table2.table != NULL) {
        free(hmap->table2.table);
        hmap->table2.table = NULL;
        hmap->table2.mask = hmap->table2.size = 0;
    }
}

static inline void hm_start_resizing(Hash_Map* hmap)
{
    assert(hmap->table2.table == NULL);
    hmap->table2 = hmap->table1;
    h_init(&hmap->table1, (hmap->table1.mask + 1) * 2);
    hmap->resizing_pos = 0;
}

// Main Interface

Hash_Node* hm_lookup(Hash_Map* hmap, Hash_Node* key, bool (*eq)(Hash_Node *, Hash_Node *))
{
    hm_resizing_helper(hmap);
    Hash_Node** from = h_lookup(&hmap->table1, key, eq);
    from = from ? from : h_lookup(&hmap->table2, key, eq);
    return from ? *from : NULL;
}

void hm_insert(Hash_Map* hmap, Hash_Node* node)
{
    if (NULL == hmap->table1.table) {
        h_init(&hmap->table1, MIN_CAPACITY);
    }
    h_insert(&hmap->table1, node);

    if (NULL == hmap->table2.table) {
        const size_t load_factor = hmap->table1.size / (hmap->table1.mask + 1);
        if (load_factor >= MAX_LOAD_FACTOR) {
            hm_start_resizing(hmap);
        }
    }
    hm_resizing_helper(hmap);
}

Hash_Node* hm_pop(Hash_Map* hmap, Hash_Node* key, bool (*eq)(Hash_Node *, Hash_Node *))
{
    hm_resizing_helper(hmap);
    Hash_Node** from = h_lookup(&hmap->table1, key, eq);
    if (from != NULL) {
       return h_detach(&hmap->table1, from); 
    }
    from = h_lookup(&hmap->table2, key, eq);
    if (from != NULL) {
        return h_detach(&hmap->table2, from);
    }
    return NULL;
}

size_t hm_size(Hash_Map* hmap)
{
    return hmap->table1.size + hmap->table2.size;
}

void hm_destroy(Hash_Map* hmap)
{
    free(hmap->table1.table);
    free(hmap->table2.table);
    hmap->table1.table = NULL;
    hmap->table1.mask = hmap->table2.size = 0;
    hmap->table2.table = NULL;
    hmap->table2.mask = hmap->table2.size = 0;
    hmap->resizing_pos = 0;
}


