#ifndef RBITMAP32_H
#define RBITMAP32_H
#include "array16.h"
#include "bitset16.h"
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/xarray.h>

enum container_type {
    ARRAY_CONTAINER,
    BITSET_CONTAINER
};

struct rcontainer {
    struct mutex         lock;
    enum container_type  c_type;
    union {
        struct array16  *array;
        struct bitset16 *bitset;
    };
};

struct rbitmap32 {
    struct xarray containers;
};

int rbitmap32_init(struct rbitmap32 *r);

void rbitmap32_destroy(struct rbitmap32 *r);

int rbitmap32_add(struct rbitmap32 *r, uint32_t x, bool *added);

int rbitmap32_add_range(struct rbitmap32 *r, uint32_t lo, uint32_t hi_excl, unsigned long *added);

#endif