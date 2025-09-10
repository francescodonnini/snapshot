#include "itree.h"
#include "pr_format.h"
#include <linux/interval_tree.h>
#include <linux/rbtree.h>
#include <linux/slab.h>

int itree_create(struct session *s) {
    mt_init_flags(&s->tree, MT_FLAGS_ALLOC_RANGE | MT_FLAGS_USE_RCU);
    return 0;
}

void itree_destroy(struct session *s) {
    mtree_destroy(&s->tree);
}

int itree_add(struct session *s, struct b_range *range) {
    if (itree_subset_of(s, range->start, range->end_excl)) {
        return -EEXIST;
    }
    return mtree_store_range(&s->tree, range->start, range->end_excl, range, GFP_KERNEL);
}

/**
 * itree_subset_of returns true if the range identified by start and len is completely contained in the tree,
 * false otherwise.
 */
bool itree_subset_of(struct session *s, unsigned long start, unsigned long end_excl) {
    rcu_read_lock();
    long unsigned index = start;
    struct b_range *r = mt_find(&s->tree, &index, end_excl);
    bool contained = r && start >= r->start && end_excl <= r->end_excl;
    rcu_read_unlock();
    return contained;
}