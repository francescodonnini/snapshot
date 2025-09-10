#include "bitset16.h"
#include <linux/bitmap.h>
#include <linux/slab.h>
#include <linux/types.h>
#define index_of(x) (x & (BITS_PER_LONG - 1))

void bitset_destroy(struct bitset16 *b) {
    kfree(b);
}

void bitset16_add_range(struct bitset16 *b, uint16_t lo, uint16_t hi_excl, unsigned long *added) {
    for (uint16_t x = lo; x < hi_excl; ++x) {
        if (bitset16_add(b, x)) {
            bitmap_set(added, x - lo, 1);
        }
    }
    __bitmap_set(b->bitmap, lo, hi_excl - lo);    
}

bool bitset16_add(struct bitset16 *b, uint16_t x) {
    unsigned long old_w = b->bitmap[x >> 6];
    unsigned long new_w = old_w | (1UL << index_of(x));
    bool added = ((old_w ^ new_w) >> index_of(x)) & 1;
    b->size += added;
    b->bitmap[x >> 6] = new_w;
    return added;
}

int bitset16_contains(const struct bitset16 *b, uint16_t x) {
    return b->bitmap[x >> 6] & (1UL << index_of(x));
}