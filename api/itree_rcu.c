#include "itree.h"
#include "pr_format.h"
#include <linux/interval_tree.h>
#include <linux/rbtree.h>
#include <linux/slab.h>

struct brange {
    sector_t start;
    sector_t end;
};

int itree_create(struct session *s) {
    mt_init_flags(&s->tree, MT_FLAGS_ALLOC_RANGE | MT_FLAGS_USE_RCU);
    return 0;
}

void itree_destroy(struct session *s) {
    mtree_destroy(&s->tree);
}

int itree_add(struct session *s, sector_t start, unsigned long len, bool *added) {
    struct brange *range;
    range = kzalloc(sizeof(*range), GFP_KERNEL);
    if (!range) {
        return -ENOMEM;
    }
    range->start = start;
    range->end = start + (len >> 9);
    if (itree_subset_of(s, start, len)) {
        *added = false;
        return 0;
    }
    int err = mtree_store_range(&s->tree, range->start, range->end, range, GFP_KERNEL);
    if (!err) {
        *added = true;
    }
    return err;
}

/**
 * itree_subset_of returns true if the range identified by start and len is completely contained in the tree,
 * false otherwise.
 */
bool itree_subset_of(struct session *s, sector_t start, unsigned long len) {
    rcu_read_lock();
    long unsigned index = start;
    long unsigned end = start + (len >> 9);
    struct brange *r = mt_find(&s->tree, &index, end);
    bool contained = r && start >= r->start && end <= r->end;
    rcu_read_unlock();
    return contained;
}