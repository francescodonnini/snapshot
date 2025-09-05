#ifndef BITSET16_H
#define BITSET16_H
#include <linux/types.h>
#define BITSET_SIZE (1024)

struct bitset16 {
    int32_t  size;
    uint64_t bitmap[BITSET_SIZE];
};

void bitset_destroy(struct bitset16 *b);

bool bitset16_add(struct bitset16 *b, uint16_t x);

int bitset16_contains(const struct bitset16 *b, uint16_t x);

#endif