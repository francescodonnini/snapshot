#include "array16.h"
#include <linux/bitmap.h>
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

static inline void memmove_u16(uint16_t *v, int from, int to, int n) {
    memmove(&v[to], &v[from], n * sizeof(uint16_t));
}

static int32_t binsearch(const struct array16 *b, uint16_t x) {
    int32_t lo = 0;
    int32_t hi = b->size - 1;
    while (lo <= hi) {
        int32_t m = lo + ((hi - lo) / 2);
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
        return capacity * 2;
    }
    if (capacity <= 1024) {
        return capacity * 3 / 2;
    }
    return capacity * 5 / 4;
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
    memmove_u16(b->buffer, pos, pos + 1, b->size - pos);
    b->buffer[pos] = x;
    b->size++;
    *added = true;
    return 0;
}

static int array16_push_range(struct array16 *b, uint16_t lo, uint16_t hi_excl, unsigned long *added) {
    if (lo >= hi_excl) {
        return 0;
    }
    uint16_t n = hi_excl - lo;
    if (b->size + n >= b->capacity) {
        int err = array16_grow(b, b->size + n, true);
        if (err) {
            return err;
        }
    }
    bitmap_set(added, 0, hi_excl - lo);
    return 0;
}

int array16_add_range(struct array16 *b, uint16_t lo, uint16_t hi_excl, unsigned long *added) {
    if (array16_empty(b) || b->buffer[b->size - 1] < lo) {
        return array16_push_range(b, lo, hi_excl, added);
    }
    // pos is either the smallest element greater than lo (but smaller than the last item in the array)
    // or the position of lo in the array
    int32_t start = binsearch(b, lo);
    start = start >= 0 ? start : -start - 1;
    // number of items in the range [lo, hi_excl) which are already present in the array
    int32_t common = 0;
    int32_t i = start;
    uint16_t x = lo;
    // this loop counts how many items in the range are already present in the array and
    // it registers them in the output bitmap. This operation is linear to the minimum size betweem the range and
    // the size of the array
    while (x < hi_excl && i < b->size && b->buffer[i] < hi_excl) {
        if (b->buffer[i] <= x) {
            if (b->buffer[i] == x) {
                ++common;
                ++x;
            }
            ++i;
        } else {
            // if x < b->buffer[i] then x cannot be in the array
            bitmap_set(added, x - lo, 1);
            ++x;
        }
    }
    if (x < hi_excl) {
        bitmap_set(added, x - lo, hi_excl - x);
    }
    // n is the number of items to add to the bitmap
    int32_t remaining = hi_excl - lo - common;
    if (remaining > 0) {
        if (b->size + remaining >= b->capacity) {
            int err = array16_grow(b, b->size + remaining, true);
            if (err) {
                return err;
            }
        }
        memmove_u16(b->buffer, start + common, start + (hi_excl - lo), b->size - (start + common));
        for (uint16_t x = lo; x < hi_excl; ++x) {
            b->buffer[start++] = x;
        }
        b->size += remaining;
    }
    return 0;
}

bool array16_contains(const struct array16 *b, uint16_t x) {
    return binsearch(b, x) >= 0;
}