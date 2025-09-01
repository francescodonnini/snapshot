#include "itree.h"
#include "pr_format.h"
#include <linux/interval_tree.h>
#include <linux/rbtree.h>
#include <linux/slab.h>

int itree_create(struct session *s) {
    s->rb_root = RB_ROOT_CACHED;
    spin_lock_init(&s->rb_lock);
    return 0;
}

void itree_destroy(struct session *s) {
    struct rb_node *it = rb_first_cached(&s->rb_root);
    while (it) {
        struct rb_node *next = rb_next(it);
        struct interval_tree_node *node = container_of(it, struct interval_tree_node, rb);
        kfree(node);
        it = next;
    }
}

int itree_add(struct session *s, sector_t start, unsigned long len, bool *added) {
    struct interval_tree_node *node;
    node = kzalloc(sizeof(*node), GFP_KERNEL);
    if (!node) {
        return -ENOMEM;
    }
    node->start = start;
    node->last = start + DIV64_U64_ROUND_UP(len, 512);
    unsigned long flags;
    spin_lock_irqsave(&s->rb_lock, flags);
    if (itree_subset_of(s, start, len)) {
        *added = false;
        goto out;
    }
    interval_tree_insert(node, &s->rb_root);
    *added = true;
out:
    spin_unlock_irqrestore(&s->rb_lock, flags);
    return 0;
}

/**
 * itree_subset_of returns true if the range identified by start and len is completely contained in the tree,
 * false otherwise.
 */
bool itree_subset_of(struct session *s, sector_t start, unsigned long len) {
    unsigned long last = start + DIV64_U64_ROUND_UP(len, 512);
    bool contained = false;
    struct interval_tree_node *node = interval_tree_iter_first(&s->rb_root, start, last);
    while (node) {
        contained = start >= node->start && last <= node->last;
        if (contained) {
            break;
        }
        node = interval_tree_iter_next(node, start, last);
    }
    pr_debug(pr_format("[%llu, %lu] %s"), start, last, contained ? "is completely contained" : "is not completely contained or not present at all");
    return contained;
}