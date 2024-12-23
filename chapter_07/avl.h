#ifndef __AVL_H__
#define __AVL_H__

#include <stddef.h>
#include <stdint.h>

struct AVLNode
{
    uint32_t depth;
    uint32_t value;
    AVLNode* left;
    AVLNode* right;
    AVLNode* parent;
};

void avl_init(AVLNode* node);
uint32_t avl_depth(AVLNode* node);
uint32_t avl_value(AVLNode* node);
AVLNode* avl_del(AVLNode* node);
AVLNode* avl_fix(AVLNode* node);

#endif // __AVL_H__

