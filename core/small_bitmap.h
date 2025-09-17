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

/**
 * small_bitmap_next_set_region iterates over all the bitmap regions made up entirely of ones. It returns false if all the
 * elements in the bitmap have been consumed the iterator, true otherwise. lo is an input/output parameter, it's the index in the bitmap from which
 * to start the reading, after the function completes it is updated with the position of the first bit equal to one starting from its old value.
 * hi is an output parameter, after the function completes (and returns true) it's the position of the first zero after the last one seen starting from lo.
 * 
 * lo initial value is usually zero, it should get the value of hi after each iteration. For example:
 * 
 * unsigned long lo = 0, hi;
 * while (small_bitmap_next_set_region(&map, &lo, &hi)) {
 *      // do something
 *      lo = hi;
 * }
 */
static inline bool small_bitmap_next_set_region(struct small_bitmap *map, unsigned long *lo, unsigned long *hi) {
    *lo = find_next_bit(map->map, map->nbits, *lo);
    if (*lo >= map->nbits) {
        return false;
    }
    *hi = find_next_zero_bit(map->map, map->nbits, *lo);
    return true;
}

#endif