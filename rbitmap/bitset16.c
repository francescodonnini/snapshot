#include "bitset16.h"
#include <linux/slab.h>
#include <linux/types.h>
#define TO_UINT64(c) ((uint64_t)c)

void bitset_destroy(struct bitset16 *b) {
    kfree(b);
}

bool bitset16_add(struct bitset16 *b, uint16_t x) {
    uint64_t old_w = b->bitmap[x >> 6];
    uint64_t new_w = old_w | (TO_UINT64(1) << (x & 63));
    int32_t added = ((old_w ^ new_w) >> (x & 63)) & 1;
    b->size += added;
    b->bitmap[x >> 6] = new_w;
    return added;
}

int bitset16_contains(const struct bitset16 *b, uint16_t x) {
    return b->bitmap[x >> 6] & (TO_UINT64(1) << (x & 63));
}