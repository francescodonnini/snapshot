#include "array16.h"
#include <linux/printk.h>
#include <linux/slab.h>

static int array16_init(struct array16 *b, int32_t capacity) {
    uint16_t *buffer = kmalloc_array(capacity, sizeof(uint16_t), GFP_KERNEL);
    if (!buffer) {
        return -ENOMEM;
    }
    b->capacity = capacity;
    b->buffer = buffer;
    return 0;
}

struct array16* array16_alloc(int32_t capacity) {
    struct array16 *b;
    b = kzalloc(sizeof(*b), GFP_KERNEL);
    if (!b) {
        return NULL;
    }
    int err = array16_init(b, capacity);
    if (err) {
        kfree(b);
        return NULL; 
    }
    return b;
}

void array16_destroy(struct array16 *b) {
    kfree(b->buffer);
    kfree(b);
}

static inline bool array16_empty(const struct array16 *b) {
    return b->size == 0;
}

static inline bool array16_full(const struct array16 *b) {
    return b->size == b->capacity;
}

static int32_t binsearch(const struct array16 *b, uint16_t x) {
    int32_t lo = 0;
    int32_t hi = b->size - 1;
    while (lo <= hi) {
        int32_t m = lo + ((hi - lo) >> 1);
        uint16_t mx = b->buffer[m];
        if (x < mx) {
            hi = m - 1;
        } else if (x > mx) {
            lo = m + 1;
        } else {
            return m;
        }
    }
    return -(lo + 1); // lo is the smallest element greater than x
}

static inline int32_t grow_capacity(int32_t capacity) {
    if (capacity <= 0) {
        return DEFAULT_INITIAL_CAPACITY;
    }
    if (capacity <= 64) {
        return capacity << 1;
    }
    if (capacity <= 1024) {
        return ((capacity << 1) + capacity) >> 1;
    }
    return ((capacity << 2) + capacity) >> 1;
}

static int array16_grow(struct array16 *b, int32_t min, bool copy) {
    int32_t capacity = b->capacity;
    int32_t new_capacity = clamp_t(int32_t, grow_capacity(capacity), min, MAX_ARRAY_SIZE);
    uint16_t *buffer;
    if (copy) {
        buffer = krealloc_array(b->buffer, new_capacity, sizeof(uint16_t), GFP_KERNEL);
    } else {
        buffer = kmalloc_array(new_capacity, sizeof(uint16_t), GFP_KERNEL);
    }
    if (!buffer) {
        return -ENOMEM;
    }
    if (!copy) {
        kfree(b->buffer);
    }
    b->buffer = buffer;
    b->capacity = new_capacity;
    return 0;
}

static int array16_push(struct array16 *b, uint16_t x, bool *added) {
    if (array16_full(b)) {
        int err = array16_grow(b, b->capacity + 1, true);
        if (err) {
            return err;
        }
    }
    b->buffer[b->size++] = x;
    *added = true;
    return 0;
}

int array16_add(struct array16 *b, uint16_t x, bool *added) {
    if (array16_empty(b) || b->buffer[b->size - 1] < x) {
        return array16_push(b, x, added);
    }
    int32_t pos = binsearch(b, x);
    if (pos>= 0) {
        *added = false;
        return 0;
    }
    if (array16_full(b)) {
        int err = array16_grow(b, b->capacity + 1, true);
        if (err) {
            return err;
        }
    }
    pos = -pos - 1;
    memmove(&b->buffer[pos + 1], &b->buffer[pos], (b->size - pos) * sizeof(uint16_t));
    b->buffer[pos] = x;
    b->size++;
    *added = true;
    return 0;
}

bool array16_contains(const struct array16 *b, uint16_t x) {
    return binsearch(b, x) >= 0;
}