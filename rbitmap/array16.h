#ifndef ARRAY16_H
#define ARRAY16_H
#include <linux/types.h>
#define DEFAULT_INITIAL_CAPACITY (16)
#define MAX_ARRAY_SIZE           (65536)

struct array16 {
    int32_t   capacity;
    int32_t   size;
    uint16_t *buffer;
};

struct array16* array16_alloc(int32_t capacity);

void array16_destroy(struct array16 *b);

int array16_add(struct array16 *b, uint16_t x, bool *added);

int array16_add_range(struct array16 *b, uint16_t lo, uint16_t hi_excl, uint64_t *added);

bool array16_contains(const struct array16 *b, uint16_t x);

#endif