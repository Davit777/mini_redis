#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <set>
#include <algorithm>

#include "avl.h"

#define CONTAINER_OF(ptr, type, member) ({ \
    const typeof( ((type*)0)->member )* __mptr = (ptr); \
    (type *) ( (char*)__mptr - offsetof(type, member) ); })

struct Data
{
    AVLNode node;
    uint32_t value;
};

struct Container
{
    AVLNode* root;
};

static inline void add(Container& container, const uint32_t value)
{
    Data* data = new Data();
    avl_init(&data->node);
    data->value = value;
    
    AVLNode* current = NULL;
    AVLNode** from = &container.root;

    while (*from) {
        current = *from;
        const uint32_t node_value = CONTAINER_OF(current, struct Data, node)->value;
        from = (value < node_value) ? &current->left : &current->right;
    }
    *from = &data->node;
    data->node.parent = current;
    container.root = avl_fix(&data->node);
}

static inline bool del(Container& container, const uint32_t value)
{
    AVLNode* current = container.root;
    while (current != NULL) {
        const uint32_t node_value = CONTAINER_OF(current, struct Data, node)->value;
        if (value == node_value) {
            break;
        }
        current = value < node_value ? current->left : current->right;
    }
    if (current == NULL) {
        return false;
    }
    container.root = avl_del(current);
    delete CONTAINER_OF(current, struct Data, node);
    return true;
}

static inline void avl_verify(AVLNode* parent, AVLNode* node)
{
    if (node == NULL) {
        return;
    }

    assert(node->parent == parent);
    avl_verify(node, node->left);
    avl_verify(node, node->right);

    assert(node->value == 1 + avl_value(node->left) + avl_value(node->right));

    const uint32_t left_depth = avl_depth(node->left);
    const uint32_t right_depth = avl_depth(node->right);
    assert(left_depth == right_depth || left_depth + 1 == right_depth || left_depth == right_depth + 1);
    assert(node->depth == 1 + std::max(left_depth, right_depth));

    const uint32_t value = CONTAINER_OF(node, struct Data, node)->value;
    if (node->left) {
        assert(node->left->parent == node);
        assert(CONTAINER_OF(node->left, struct Data, node)->value <= value);
    }
    if (node->right) {
        assert(node->right->parent == node);
        assert(CONTAINER_OF(node->right, struct Data, node)->value >= value);
    }
}

static inline void extract(AVLNode* node, std::multiset<uint32_t>& extracted)
{
    if (node == NULL) {
        return;
    }
    extract(node->left, extracted);
    extracted.insert(CONTAINER_OF(node, struct Data, node)->value);
    extract(node->right, extracted);
}

static inline void container_verify(Container& container, const std::multiset<uint32_t>& ref)
{
    avl_verify(NULL, container.root);
    assert(avl_value(container.root) == ref.size());
    std::multiset<uint32_t> extracted;
    extract(container.root, extracted);
    assert(extracted == ref);
}

static inline void dispose(Container& container)
{
    while (container.root != NULL) {
        AVLNode* node = container.root;
        container.root = avl_del(container.root);
        delete CONTAINER_OF(node, struct Data, node);
    }
}

static inline void test_insert(const uint32_t size)
{
    for (uint32_t value = 0; value < size; ++value) {
        Container container;
        std::multiset<uint32_t> ref;
        for (uint32_t i = 0; i < size; ++i) {
            if (i == value) {
                continue;
            }
            add(container, i);
            ref.insert(i);
        }
        container_verify(container, ref);
        
        add(container, value);
        ref.insert(value);
        container_verify(container, ref);
        dispose(container);
    }
}

static inline void test_insert_dup(const uint32_t size)
{
    for (uint32_t value = 0; value < size; ++value) {
        Container container;
        std::multiset<uint32_t> ref;
        for (uint32_t i = 0; i < size; ++i) {
            add(container, i);
            ref.insert(i);
        }
        container_verify(container, ref);
        
        add(container, value);
        ref.insert(value);
        container_verify(container, ref);
        dispose(container);
    }
}

static inline void test_remove(const uint32_t size)
{
    for (uint32_t value = 0; value < size; ++value) {
        Container container;
        std::multiset<uint32_t> ref;
        for (uint32_t i = 0; i < size; ++i) {
            add(container, i);
            ref.insert(i);
        }
        container_verify(container, ref);

        assert(del(container, value));
        ref.erase(value);
        container_verify(container, ref);
        dispose(container);
    }
}

int main()
{
    Container container = {NULL};

    {
        container_verify(container, std::multiset<uint32_t>());
        add(container, 123);
        std::multiset<uint32_t> m; m.insert(123);
        container_verify(container, m);
        assert(!del(container, 124));
        assert(del(container, 123));
        container_verify(container, std::multiset<uint32_t>());
    }

    {
        std::multiset<uint32_t> ref;
        for (uint32_t i = 0; i < 1000; i += 3) {
            add(container, i);
            ref.insert(i);
            container_verify(container, ref);
        }

        for (uint32_t i = 0; i < 1000; ++i) {
            const uint32_t value = (uint32_t)rand() % 1000;
            add(container, value);
            ref.insert(value);
            container_verify(container, ref);
        }

        for (uint32_t i = 0; i < 200; ++i) {
            const uint32_t value = (uint32_t)rand() % 1000;
            std::multiset<uint32_t>::const_iterator it = ref.find(value);
            if (it == ref.end()) {
                assert(!del(container, value));
            } else {
                assert(del(container, value));
                ref.erase(it);
            }
            container_verify(container, ref);
        }
    }
    dispose(container);

    for (uint32_t i = 0; i < 200; ++i) {
        test_insert(i);
        test_insert_dup(i);
        test_remove(i);
    }


    return 0;
}
