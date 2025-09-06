#ifndef AOS_BIO_H
#define AOS_BIO_H
#include <linux/bio.h>
#include <linux/mm_types.h>
#include <linux/time64.h>
#include <linux/types.h>

struct bwrapper {
    struct timespec64  arrival_time;
    struct bio        *orig_bio;
};

struct page_iter {
    struct page       *page;
    unsigned int       offset;
    unsigned int       len;
};

/**
 * bio_private_data contains the original write bio request, the sector from which the write starts,
 * the number of pages to use to contains the data and an auxiliary struct to hold the data read from the device
 */
struct bio_private_data {
    struct timespec64  arrival_time;
    struct bio        *orig_bio;
    sector_t           sector;
    int                iter_len;
    struct page_iter   iter[];
};

#define page_iter_for_each(pos, pd)\
        for ((pos) = (pd)->iter; pos < &(pd)->iter[(pd)->iter_len]; ++pos)\

int bio_deferred_work_init(void);

void bio_deferred_work_cleanup(void);

int bio_enqueue(struct bwrapper *wrp);

void dbg_dump_bio(const char *prefix, struct bio *bio);

#endif