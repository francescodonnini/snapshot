#ifndef AOS_BIO_H
#define AOS_BIO_H
#include <linux/bio.h>
#include <linux/mm_types.h>
#include <linux/types.h>

struct page_iter {
    struct page  *page;
    unsigned int  offset;
    unsigned int  len;
};

/**
 * bio_private_data contains the original write bio request, the sector from which the write starts,
 * the number of pages to use to contains the data and an auxiliary struct to hold the data read from the device
 */
struct bio_private_data {
    struct bio       *orig_bio;
    sector_t          sector;
    int               iter_len;
    struct page_iter  iter[];
};

int bio_deferred_work_init(void);

void bio_deferred_work_cleanup(void);

int bio_enqueue(struct bio *bio);

void dbg_dump_bio(const char *prefix, struct bio *bio);

#endif