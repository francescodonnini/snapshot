#include "bitset16.h"
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/types.h>

void bitset_destroy(struct bitset16 *b) {
    kfree(b);
}

void bitset16_add_range(struct bitset16 *b, uint16_t lo, uint16_t hi_excl, unsigned long *added) {
    for (uint16_t x = lo; x < hi_excl; ++x) {
        if (bitset16_add(b, x)) {
            bitmap_set(added, x - lo, 1);
        }
    }
}

bool bitset16_add(struct bitset16 *b, uint16_t x) {
    if (test_bit(x, b->bitmap)) {
        return false;
    }
    bitmap_set(b->bitmap, x, 1);
    return true;
}

int bitset16_contains(const struct bitset16 *b, uint16_t x) {
    return test_bit(x, b->bitmap);
}