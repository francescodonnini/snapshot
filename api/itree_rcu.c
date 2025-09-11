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
    int err = mtree_store_range(&s->tree, range->start, range->end_excl, range, GFP_NOWAIT);
    return err;
}

static unsigned long update_span(struct b_range *q, struct b_range *r) {
    if (q->start >= r->start && q->end_excl <= r->end_excl) {
        // q:     [-----)
        // r:  [------------)
        q->start = q->end_excl;
    } else if (q->start >= r->start) {
        // if true then q->end_excl > r->end_excl
        // q:       [---------------)
        // r:  [------------)
        q->start = r->end_excl;
    } else if (q->end_excl <= r->end_excl) {
        // if true then q->start < r->start
        // q:  [----------)
        // r:      [----------------)
        q->end_excl = r->start;
    }
    return q->end_excl - q->start;
}

/**
 * itree_subset_of returns true if the range identified by start and len is completely contained in the tree,
 * false otherwise.
 */
bool itree_subset_of(struct session *s, unsigned long start, unsigned long end_excl) {
    rcu_read_lock();
    struct b_range query = { .start = start, .end_excl = end_excl };
    struct b_range *r;
    unsigned long index = start;
    mt_for_each(&s->tree, r, index, end_excl) {
        if (!update_span(&query, r)) {
            break;
        }
    }
    rcu_read_unlock();
    return query.start == query.end_excl;
}