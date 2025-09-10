#ifndef AOS_B_RANGE_H
#define AOS_B_RANGE_H
#include <linux/slab.h>
#include <linux/types.h>

struct b_range {
    unsigned long start;
    unsigned long end_excl;
};

static inline struct b_range *b_range_alloc(unsigned long start, unsigned long end_excl) {
    struct b_range *r;
    r = kzalloc(sizeof(*r), GFP_KERNEL);
    if (!r) {
        return NULL;
    }
    r->start = start;
    r->end_excl = end_excl;
    return r;
}

#endif