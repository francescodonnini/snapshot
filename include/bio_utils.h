#ifndef AOS_BIO_UTILS_H
#define AOS_BIO_UTILS_H
#include <linux/bio.h>

static inline dev_t bio_devno(struct bio *bio) {
    return bio->bi_bdev->bd_dev;
}

static inline dev_t* bio_denvo_safe(struct bio *bio, dev_t *devno) {
    if (!bio || !bio->bi_bdev) {
        return NULL;
    }
    *devno = bio->bi_bdev->bd_dev;
    return devno;
}

static unsigned long bio_len(struct bio *bio) {
    unsigned long n = 0;
    struct bio_vec bvec;
	struct bvec_iter it;
    bio_for_each_bvec(bvec, bio, it) {
        n += bvec.bv_len;
    }
    return n;
}

static inline sector_t bio_sector(struct bio *bio) {
    return bio->bi_iter.bi_sector;
}

#endif