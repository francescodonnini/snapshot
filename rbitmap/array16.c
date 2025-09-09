#include "array16.h"
#include <linux/printk.h>
#include <linux/slab.h>
#define TO_UINT64(c) ((uint64_t)c)

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
    memmove(&v[to], &v[from], n * sizeof(uin16_t));
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
    memmove_u16(b->buffer, pos, pos + 1, b->size - pos);
    b->buffer[pos] = x;
    b->size++;
    *added = true;
    return 0;
}

static int array16_push_range(struct array16 *b, uint16_t lo, uint16_t hi_excl, uint64_t *added) {
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
    for (uint16_t x = lo; x < hi_excl; ++x) {
        b->buffer[b->size++] = x;
        added[i / 64] |= (TO_UINT64(1) << (i & 63));
    }
    return 0;
}

static inline void memmove_u16(uint16_t *v, int from, int to, int n) {
    memmove(&v[to], &v[from], n * sizeof(uint16_t));
}

int array16_add_range(struct array16 *b, uint16_t lo, uint16_t hi_excl, uint64_t *added) {
    if (array16_empty(b) || b->buffer[b->size - 1] < lo) {
        return array16_push_range(b, lo, hi_excl, added);
    }
    // pos is either the smallest element greater than lo (but smaller than the last item in the array)
    // or the position of lo in the array
    int32_t pos = binsearch(b, lo);
    int32_t i = pos >= 0 ? pos : -pos - 1;
    // number of items in the range [lo, hi_excl) which are already present in the array
    int32_t common = 0;
    uint16_t x = lo;
    // this loop counts how many items in the range are already present in the array and
    // it registers them in the output bitmap. This operation is linear to the minimum size betweem the range and
    // the size of the array
    while (x < hi_excl && i < b->size && b->buffer[i] < hi_excl) {
        if (b->buffer[i] <= x) {
            if (b->buffer[i] == x) {
                int32_t bit_pos = x - lo;
                added[bit_pos / 64] |= (TO_UINT64(1) << (bit_pos & 63));
                ++common;
            }
            ++i;
        } else {
            ++x;
        }
    }
    // items to add
    int32_t n = hi_excl - lo - common;
    if (n > 0) {
        if (b->size + n >= b->capacity) {
            int err = array16_grow(b, b->size + n, true);
            if (err) return err;
        }
        memmove_u16(b->buffer, pos + common, pos + n, b->size - (pos + common));
        int32_t i = pos;
        for (uint16_t x = lo; x < hi_excl; ++x) {
            b->buffer[i++] = x;
        }
        b->size += n;
    }
    for (uint16_t i = 0; i < DIV_ROUND_UP(hi_excl - lo, 64); ++i) {
        added[i] = ~added[i];
    }
    return 0;
}

bool array16_contains(const struct array16 *b, uint16_t x) {
    return binsearch(b, x) >= 0;
}