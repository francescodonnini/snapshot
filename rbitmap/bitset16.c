#include "bitset16.h"
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/types.h>

void bitset_destroy(struct bitset16 *b) {
    kfree(b);
}

void bitset16_add_range(struct bitset16 *b, uint16_t lo, uint16_t hi_excl, unsigned long *added, unsigned long idx) {
    for (uint16_t x = lo; x <= hi_excl; ++x) {
        if (bitset16_add(b, x)) {
            bitmap_set(added, idx, 1);
        }
        ++idx;
    }
}

bool bitset16_add(struct bitset16 *b, uint16_t x) {
    if (test_bit(x, b->bitmap)) {
        return false;
    }
    bitmap_set(b->bitmap, x, 1);
    b->size++;
    return true;
}