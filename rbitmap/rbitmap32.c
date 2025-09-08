#include "rbitmap32.h"
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/wordpart.h>

int rbitmap32_init(struct rbitmap32 *r) {
    struct rcontainer *pos;
    rcontainer_for_each(pos, r) {
        pos->c_type = ARRAY_CONTAINER;
        pos->array = NULL;
        mutex_init(&pos->lock);
    }
    return 0;
}

static inline bool rcontainer_null(const struct rcontainer *c) {
    switch (c->c_type) {
        case ARRAY_CONTAINER:
            return c->array == NULL;
        case BITSET_CONTAINER:
            return c->bitset == NULL;
        default:
            return true;
    }
}

static inline void rcontainer_destroy(struct rcontainer *c) {
    mutex_lock(&c->lock);
    if (rcontainer_null(c)) {
        mutex_unlock(&c->lock);
        return;
    }
    switch (c->c_type) {
        case ARRAY_CONTAINER:
            if (c->array) {
                array16_destroy(c->array);
            }
            break;
        case BITSET_CONTAINER:
            if (c->bitset) {
                bitset_destroy(c->bitset);
            }
            break;
        default:
            pr_err("invalid container type %d", c->c_type);
    }
    c->c_type = ARRAY_CONTAINER;
    c->array = NULL;
    mutex_unlock(&c->lock);
}

void rbitmap32_destroy(struct rbitmap32 *r) {
    struct rcontainer *pos;
    rcontainer_for_each(pos, r) {
        rcontainer_destroy(pos);
    }
}

static struct bitset16* bitset16_alloc(void) {
    struct bitset16 *b;
    return kzalloc(sizeof(*b), GFP_KERNEL);
}

static int array16_to_bitset(struct rcontainer *c) {
    if (c->c_type != ARRAY_CONTAINER || c->array == NULL) {
        return -1;
    }
    struct bitset16 *bitset = bitset16_alloc();
    if (!bitset) {
        return -ENOMEM;
    }
    struct array16 *array = c->array;
    for (int32_t i = 0; i < array->size; ++i) {
        int32_t pos = array->buffer[i] >> 6;
        bitset->bitmap[pos] |= (pos & 63);
    }
    array16_destroy(array);
    c->c_type = BITSET_CONTAINER;
    c->bitset = bitset;
    return 0;
}

static int rcontainer_alloc_array16(struct rcontainer *c) {
    c->array = array16_alloc(DEFAULT_INITIAL_CAPACITY);
    if (!c->array) {
        return -ENOMEM;
    }
    c->c_type = ARRAY_CONTAINER;
    return 0;
}

static int32_t rcontainer_length(const struct rcontainer *c) {
    switch (c->c_type) {
        case ARRAY_CONTAINER:
            return c->array->size;
        case BITSET_CONTAINER:
            return c->bitset->size;
        default:
            pr_err("invalid container type %d", c->c_type);
            return 0;
    }
}

static inline int32_t container_index(uint32_t x) {
    return upper_16_bits(x) >> 12;
}

static struct rcontainer* rcontainer_nth(struct rbitmap32 *r, uint32_t x) {
    return &r->containers[container_index(x)];
}

/**
 * rbitmap32_add adds an integer x to the bitmap. It returns 0 on success, <0 otherwise. It
 * sets added to true if the x wasn't already in the set, false otherwise. added is ignored
 * if the insertion fails.
 */
int rbitmap32_add(struct rbitmap32 *r, uint32_t x, bool *added) {
    struct rcontainer *c = rcontainer_nth(r, x);
    mutex_lock(&c->lock);
    if (rcontainer_null(c)) {
        int err = rcontainer_alloc_array16(c);
        if (!c) {
            mutex_unlock(&c->lock);
            return err;
        }
    }
    int err;
    switch (c->c_type) {
        case ARRAY_CONTAINER:
            err = array16_add(c->array, lower_16_bits(x), added);
            break;
        case BITSET_CONTAINER:
            *added = bitset16_add(c->bitset, lower_16_bits(x));
            err = 0;
            break;
        default:
            pr_err("invalid container type %d", c->c_type);
            err = -1;
    }
    if (rcontainer_length(c) >= 4096) {
        array16_to_bitset(c);
    }
    mutex_unlock(&c->lock);
    return err;
}

bool rbitmap32_contains(struct rbitmap32 *r, uint32_t x) {
    struct rcontainer *c = rcontainer_nth(r, x);
    mutex_lock(&c->lock);
    if (rcontainer_null(c)) {
        mutex_unlock(&c->lock);
        return false;
    }
    bool bret;
    switch (c->c_type) {
        case ARRAY_CONTAINER:
            bret = array16_contains(c->array, lower_16_bits(x));
            break;
        case BITSET_CONTAINER:
            bret = bitset16_contains(c->bitset, lower_16_bits(x));
            break;
        default:
            pr_err("invalid container type %d", c->c_type);
            bret = false;
            break;
    }
    mutex_unlock(&c->lock);
    return bret;
}