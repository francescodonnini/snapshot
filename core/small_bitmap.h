#ifndef AOS_SMALL_BITMAP_H
#define AOS_SMALL_BITMAP_H
#include <linux/bitmap.h>
#include <linux/types.h>
#define INLINE_MAP_SIZE (4)

struct small_bitmap {
    int   nbits;
    long *map;
    // can contains up to 128 items
    long  __inline_map[INLINE_MAP_SIZE];
};

static inline long *small_bitmap_zeros(struct small_bitmap *b, int nbits) {
    int capacity = BITS_TO_LONGS(nbits);
    if (capacity > INLINE_MAP_SIZE) {
        b->map = bitmap_zalloc(nbits, GFP_KERNEL);
    } else {
        b->map = b->__inline_map;
        bitmap_zero(b->map, nbits);
    }
    b->nbits = nbits;
    return b->map;
}

static inline void small_bitmap_free(struct small_bitmap *b) {
    if (b->map != b->__inline_map && b->map != NULL) {
        bitmap_free(b->map);
    }
}

static inline bool small_bitmap_next_set_region(struct small_bitmap *map, unsigned long *lo, unsigned long *hi) {
    *lo = find_next_bit(map->map, map->nbits, *lo);
    if (*lo >= map->nbits) {
        return false;
    }
    *hi = find_next_zero_bit(map->map, map->nbits, *lo);
    return true;
}

#endif