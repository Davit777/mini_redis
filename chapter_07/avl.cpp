#include <assert.h>

#include "avl.h"

void avl_init(AVLNode* node)
{
    node->depth = 1;
    node->value = 1;
    node->left = node->right = node->parent = NULL;
}

uint32_t avl_depth(AVLNode* node)
{
    return node ? node->depth : 0;
}

uint32_t avl_value(AVLNode* node)
{
    return node ? node->value : 0;
}

static inline uint32_t max(uint32_t lhs, uint32_t rhs)
{
    return lhs < rhs ? rhs : lhs;
}

static inline void avl_update(AVLNode* node)
{
    node->depth = 1 + max(avl_depth(node->left), avl_depth(node->right));
    node->value = 1 + avl_value(node->left) + avl_value(node->right);
}

/*
 * Left Rotation (Single Rotation) on AVL Tree with Parent Pointer
 *
 * This function performs a left rotation on the subtree rooted at the node `node`.
 * Left rotation is used to maintain the AVL tree balance when the right subtree
 * of `node` is taller than the left subtree (i.e., the tree is "right-heavy").
 * The function also takes into account the parent pointers of the affected nodes.
 *
 * After the left rotation:
 * - The right child of `node` (`node->right`) becomes the new root of the subtree.
 * - The current root node `node` becomes the left child of the new root `new_node`.
 * - The parent pointer of the involved nodes is properly updated.
 *
 * Steps:
 * 1. Let `new_node = node->right` (the right child of `node`).
 * 2. Let `new_node_left = new_node->left` (the left child of `new_node`).
 * 3. Set `node->right = new_node_left`. This removes `new_node->left` from `new_node` and attaches it as
 *    the right child of `node`.
 * 4. If `new_node_left` is not NULL, update its parent pointer to point to `node`.
 * 5. Set `new_node->left = x`. This makes `node` the left child of `new_node`.
 * 6. If `node` has a parent, update the parent pointer of `new_node`:
 *    - If `node` was the left child of its parent, set `x->parent->left = new_node`.
 *    - If `node` was the right child of its parent, set `x->parent->right = new_node`.
 * 7. Update the parent pointer of `node` to point to `new_node`.
 * 8. Set `new_node->parent = x->parent`, establishing the correct parent-child relationship.
 * 9. Update the heights and balance factors of the affected nodes (`node` and `new_node`).
 *
 * The result of this rotation is that `new_node` becomes the new root of the subtree,
 * and `node` becomes its left child. This maintains the AVL tree balance by ensuring
 * the height difference between the left and right subtrees is within the allowed
 * range.
 *
 *     node                      new_node
 *      / \                        / \
 *     A   new_node    =>       node  B
 *        / \                   / \
 *  nn_left  B                 A  nn_left
 *
 */
static inline AVLNode* rotate_left(AVLNode* node)
{
    AVLNode* new_node = node->right;
    if (new_node->left) {
        new_node->left->parent = node;
    }
    node->right = new_node->left;
    new_node->left = node;
    new_node->parent = node->parent;
    node->parent = new_node;
    avl_update(node);
    avl_update(new_node);
    return new_node;
}

/*
 * Right Rotation (Single Rotation) on AVL Tree with Parent Pointer
 *
 * This function performs a right rotation on the subtree rooted at the node `node`.
 * Right rotation is used to maintain the AVL tree balance when the left subtree
 * of `node` is taller than the right subtree (i.e., the tree is "left-heavy").
 * The function also takes into account the parent pointers of the affected nodes.
 *
 * After the right rotation:
 * - The left child of `node` (`node->left`) becomes the new root of the subtree.
 * - The current root node `node` becomes the right child of the new root `new_node`.
 * - The parent pointer of the involved nodes is properly updated.
 *
 * Steps:
 * 1. Let `new_node = node->left` (the left child of `node`).
 * 2. Let `new_node_right = new_node->right` (the right child of `new_node`).
 * 3. Set `node->left = new_node_right`. This removes `new_node->right` from `new_node` and attaches it as
 *    the left child of `node`.
 * 4. If `new_node_right` is not NULL, update its parent pointer to point to `node`.
 * 5. Set `new_node->right = node`. This makes `node` the right child of `new_node`.
 * 6. If `node` has a parent, update the parent pointer of `new_node`:
 *    - If `node` was the left child of its parent, set `node->parent->left = new_node`.
 *    - If `node` was the right child of its parent, set `node->parent->right = new_node`.
 * 7. Update the parent pointer of `node` to point to `new_node`.
 * 8. Set `new_node->parent = node->parent`, establishing the correct parent-child relationship.
 * 9. Update the heights and balance factors of the affected nodes (`node` and `new_node`).
 *
 * The result of this rotation is that `new_node` becomes the new root of the subtree,
 * and `node` becomes its right child. This maintains the AVL tree balance by ensuring
 * the height difference between the left and right subtrees is within the allowed
 * range.
 *
 *       node                new_node
 *        / \                  / \
 * new_node  C           nn_left  node
 *      /  \        =>     / \     / \
 * nn_left  D             A   B   D   C 
 *    / \
 *   A   B 
 *
 */
static inline AVLNode* rotate_right(AVLNode* node)
{
    AVLNode* new_node = node->left;
    if (new_node->right) {
        new_node->right->parent = node;
    }
    node->left = new_node->right;
    new_node->right = node;
    new_node->parent = node->parent;
    node->parent = new_node;
    avl_update(node);
    avl_update(new_node);
    return new_node;
}

static inline AVLNode* avl_fix_left(AVLNode* root)
{
    if (avl_depth(root->left->left) < avl_depth(root->left->right)) {
        root->left = rotate_left(root->left);
    }
    return rotate_right(root);
}

static inline AVLNode* avl_fix_right(AVLNode* root)
{
    if (avl_depth(root->right->right) < avl_depth(root->right->left)) {
        root->right = rotate_right(root->right);
    }
    return rotate_left(root);
}

static inline bool is_depth_violated(const uint32_t left_depth, const uint32_t right_depth)
{
    return left_depth == right_depth + 2;
}

AVLNode* avl_fix(AVLNode* node)
{
    while (true) {
        avl_update(node);
        const uint32_t left_depth = avl_depth(node->left);
        const uint32_t right_depth = avl_depth(node->right);
        AVLNode** from = NULL;
        if (node->parent) {
            from = (node->parent->left == node) ? &node->parent->left : &node->parent->right;
        }
        if (is_depth_violated(left_depth, right_depth)) {
            node = avl_fix_left(node);
        } else if (is_depth_violated(right_depth, left_depth)) {
            node = avl_fix_right(node);
        }
        if (!from) {
            return node;
        }
        *from = node;
        node = node->parent;
    }

    assert(false);
    return NULL;
}

AVLNode* avl_del(AVLNode* node)
{
    if (node->right == NULL) {
        AVLNode* parent = node->parent;
        if (node->left) {
            node->left->parent = parent;
        }
        if (parent) {
            (parent->left == node ? parent->left : parent->right) = node->left;
            return avl_fix(parent);
        } else {
            return node->left;
        }
    } else {
        AVLNode* victim = node->right;
        while (victim->left) {
            victim = victim->left;
        }
        AVLNode* root = avl_del(victim);
        *victim = *node;
        if (victim->left) {
            victim->left->parent = victim;
        }
        if (victim->right) {
            victim->right->parent = victim;
        }
        AVLNode* parent = node->parent;
        if (parent) {
            (parent->left == node ? parent->left : parent->right) = victim;
            return root;
        } else {
            return victim;
        }
    }
}


