#ifndef __HASH_TABLE_H__
#define __HASH_TABLE_H__

#include <stddef.h>
#include <stdint.h>

struct Hash_Node
{
    Hash_Node* next;
    uint64_t hcode;
};

struct Hash_Table
{
    Hash_Node** table;
    size_t mask;
    size_t size;
};

struct Hash_Map
{
    Hash_Table table1;
    Hash_Table table2;
    size_t resizing_pos;
};

Hash_Node* hm_lookup(Hash_Map* hmap, Hash_Node* key, bool (*eq)(Hash_Node *, Hash_Node *));
void hm_insert(Hash_Map* hmap, Hash_Node* node);
Hash_Node* hm_pop(Hash_Map* hmap, Hash_Node* key, bool (*eq)(Hash_Node *, Hash_Node *));
size_t hm_size(Hash_Map* hmap);
void hm_destroy(Hash_Map* hmap);

#endif // __HASH_TABLE_H__

