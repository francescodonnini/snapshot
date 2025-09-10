#ifndef AOS_SMALL_BITMAP_H
#define AOS_SMALL_BITMAP_H
#include <linux/bitmap.h>
#include <linux/types.h>

struct small_bitmap {
    int   nbits;
    long *map;
    // can contains up to 512 items
    long  __inline_map[8];
};

static inline long *small_bitmap_zeros(struct small_bitmap *b, int n) {
    int capacity = BITS_TO_LONGS(n);
    if (capacity > 8) {
        b->map = bitmap_zalloc(n, GFP_KERNEL);
    } else {
        b->map = b->__inline_map;
        bitmap_zero(b->map, n);
    }
    b->nbits = n;
    return b->map;
}

static inline void small_bitmap_free(struct small_bitmap *b) {
    if (b->map != b->__inline_map && b->map != NULL) {
        bitmap_free(b->map);
    }
}

static inline bool small_bitmap_next_set_region(struct small_bitmap *map, unsigned long *lo, unsigned long *hi) {
    unsigned long _lo, _hi;
    _lo = find_next_bit(map->map, map->nbits, *hi);
    if (_lo >= map->nbits) {
        return false;
    }
    _hi = min_t(long, find_next_zero_bit(map->map, map->nbits, _lo), map->nbits);
    *lo = _lo;
    *hi = _hi;
    return true;
}

#endif