#ifndef AOS_BIO_UTILS_H
#define AOS_BIO_UTILS_H
#include <linux/bio.h>

static inline dev_t bio_devnum(struct bio *bio) {
    return bio->bi_bdev->bd_dev;
}

static inline sector_t bio_sector(struct bio *bio) {
    return bio->bi_iter.bi_sector;
}

#endif