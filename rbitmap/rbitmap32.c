#include "rbitmap32.h"
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/srcu.h>
#include <linux/string.h>
#include <linux/wordpart.h>
#define ARRAY_CONTAINER_THRESHOLD (4096)

int rbitmap32_init(struct rbitmap32 *r) {
    xa_init(&r->containers);
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
    if (rcontainer_null(c)) {
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
}

void rbitmap32_destroy(struct rbitmap32 *r) {
    struct rcontainer *c;
    unsigned long idx;
    xa_for_each(&r->containers, idx, c) {
        rcontainer_destroy(c);
    }
    xa_destroy(&r->containers);
}

static struct bitset16* bitset16_alloc(void) {
    struct bitset16 *b;
    return kzalloc(sizeof(*b), GFP_KERNEL);
}

static int array16_to_bitset(struct rcontainer *c) {
    if (c->c_type != ARRAY_CONTAINER) {
        return -1;
    }
    struct bitset16 *bitset = bitset16_alloc();
    if (!bitset) {
        return -ENOMEM;
    }
    if (!rcontainer_null(c)) {
        for (int32_t i = 0; i < c->array->size; ++i) {
            bitmap_set(bitset->bitmap, c->array->buffer[i], 1);
        }
        bitset->size = c->array->size;
        array16_destroy(c->array);
    }
    c->c_type = BITSET_CONTAINER;
    c->bitset = bitset;
    return 0;
}

static int rcontainer_alloc_array16(struct rcontainer *c, uint32_t n) {
    c->array = array16_alloc(n);
    if (!c->array) {
        return -ENOMEM;
    }
    c->c_type = ARRAY_CONTAINER;
    return 0;
}

static int rcontainer_alloc_bitset16(struct rcontainer *c) {
    c->bitset = bitset16_alloc();
    if (!c->array) {
        return -ENOMEM;
    }
    c->c_type = BITSET_CONTAINER;
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

static struct rcontainer *rcontainer_alloc(uint32_t n) {
    struct rcontainer *c = kzalloc(sizeof(*c), GFP_KERNEL);
    if (!c) {
        return NULL;
    }
    mutex_init(&c->lock);
    int err;
    if (n > ARRAY_CONTAINER_THRESHOLD) {
        err = rcontainer_alloc_bitset16(c);
    } else {
        n = n > 0 ? n : DEFAULT_INITIAL_CAPACITY;
        err = rcontainer_alloc_array16(c, n);
    }
    if (err) {
        kfree(c);
        return NULL;
    }
    return c;
}

static inline int32_t container_index(uint32_t x) {
    return upper_16_bits(x);
}

static inline struct rcontainer* rcontainer_nth(struct rbitmap32 *r, uint32_t x) {
    return xa_load(&r->containers, container_index(x));
}

static struct rcontainer* rcontainer_get_or_create(struct rbitmap32 *r, uint32_t x, uint32_t n) {
    struct rcontainer *c = rcontainer_nth(r, x);
    if (c) {
        return c;
    }
    c = rcontainer_alloc(n);
    if (!c) {
        return ERR_PTR(-ENOMEM);
    }
    int err = xa_insert(&r->containers, container_index(x), c, GFP_KERNEL);
    if (err) {
        kfree(c);
        if (err != -EBUSY) {
            return ERR_PTR(err);
        } else {
            return rcontainer_nth(r, x);
        }
    }
    return c;
}

/**
 * rbitmap32_add adds an integer x to the bitmap. It returns 0 on success, <0 otherwise. It
 * sets added to true if the x wasn't already in the set, false otherwise. added is ignored
 * if the insertion fails.
 */
int rbitmap32_add(struct rbitmap32 *r, uint32_t x, bool *added) {
    struct rcontainer *c = rcontainer_get_or_create(r, x, 0);
    if (IS_ERR(c)) {
        return PTR_ERR(c);
    }
    mutex_lock(&c->lock);
    int err;
    switch (c->c_type) {
        case ARRAY_CONTAINER:
            // ARRAY_CONTAINER should hold maximum ARRAY_CONTAINER_THRESHOLD items, past that threshold it is
            // necessary to convert the array to a bitset
            err = array16_add(c->array, lower_16_bits(x), added);
            if (!err && rcontainer_length(c) >= ARRAY_CONTAINER_THRESHOLD) {
                array16_to_bitset(c);
            } 
            break;
        case BITSET_CONTAINER:
            *added = bitset16_add(c->bitset, lower_16_bits(x));
            err = 0;
            break;
        default:
            pr_err("invalid container type %d", c->c_type);
            err = -1;
    }
    mutex_unlock(&c->lock);
    return err;
}

static int rcontainer_add_range_unlocked(struct rcontainer *c, uint16_t lo, uint16_t hi, unsigned long *added, unsigned long idx) {
    int err;
    switch (c->c_type) {
        case ARRAY_CONTAINER:
            err = array16_add_range(c->array, lo, hi, added, idx);
            if (!err && rcontainer_length(c) >= ARRAY_CONTAINER_THRESHOLD) {
                err = array16_to_bitset(c);
            }
            return err; 
        case BITSET_CONTAINER:
            bitset16_add_range(c->bitset, lo, hi, added, idx);
            return 0;
        default:
            pr_err("invalid container type %d", c->c_type);
            return -1;
    }
}

/**
 * Returns the first element that is not in the same container as x, or the maximum 32-bit integer
 * if x is in the last container
 */
static inline uint32_t next_container_start(uint32_t x) {
    uint32_t next = x | 0xffff;
    if (upper_16_bits(x) < 0xffff) {
        next++;
    }
    return next;
}

/**
 * Returns the last item that belongs to the same container as x or hi_excl - 1 if the range
 * [x, hi_excl) does not span multiple containers
 */
static inline uint16_t last_item(uint32_t x, uint32_t hi_excl) {
    // [x, hi_excl) spans multiple containers
    if (upper_16_bits(x) < upper_16_bits(hi_excl - 1)) {
        return 0xffff;
    } else {
        return lower_16_bits(hi_excl - 1);
    }
}

/**
 * rbitmap32_add_range adds the 32-bit integer interval [lo, hi_excl) to the bitmap, it sets the corresponding bit (the position
 * is relative to lo) of the bitmap added to one if the integer item is actually inserted, to zero otherwise. It returns 0 on success,
 * <0 otherwise.
 */
int rbitmap32_add_range(struct rbitmap32 *r, uint32_t lo, uint32_t hi_excl, unsigned long *added) {
    unsigned long idx = 0;
    // the items in the range [lo, hi_excl) can reside in different containers, it is necessary
    // to determine in order the subranges associated to container 0, 1, 2, ...
    while (lo < hi_excl) {
        uint16_t last = last_item(lo, hi_excl);
        uint16_t n = last - lower_16_bits(lo) + 1;
        struct rcontainer *c = rcontainer_get_or_create(r, lo, n);
        mutex_lock(&c->lock);
        // idx is the index of the bitmap added where to start writing
        int err = rcontainer_add_range_unlocked(c, lower_16_bits(lo), last, added, idx);
        mutex_unlock(&c->lock);
        if (err) {
            return err;
        }
        idx += n;
        lo = min_t(uint32_t, next_container_start(lo), hi_excl);
    }
    return 0;
}