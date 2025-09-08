#ifndef AOS_B_RANGE_H
#define AOS_B_RANGE_H
#include <linux/slab.h>
#include <linux/types.h>

struct b_range {
    sector_t start;
    sector_t end;
};

static inline struct b_range *b_range_alloc(sector_t start, unsigned long len) {
    struct b_range *r;
    r = kzalloc(sizeof(*r), GFP_KERNEL);
    if (!r) {
        return NULL;
    }
    r->start = start;
    r->end = start + (len >> 9);
    return r;
}

#endif