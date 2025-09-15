#ifndef BITSET16_H
#define BITSET16_H
#include <linux/bitops.h>
#include <linux/types.h>

struct bitset16 {
    int32_t       size;
    unsigned long bitmap[BITS_TO_LONGS(65536)];
};

void bitset_destroy(struct bitset16 *b);

bool bitset16_add(struct bitset16 *b, uint16_t x);

void bitset16_add_range(struct bitset16 *b, uint16_t lo, uint16_t hi, unsigned long *added, unsigned long idx);

#endif