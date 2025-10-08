#ifndef AOS_BIO_H
#define AOS_BIO_H
#include <linux/bio.h>
#include <linux/mm_types.h>
#include <linux/time64.h>
#include <linux/types.h>

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
    struct bio        *orig_bio;
    dev_t              dev;
    sector_t           sector;
    unsigned long      bytes;
    unsigned long      iter_capacity;
    int                iter_len;
    struct page_iter   iter[];
};

#define page_iter_for_each(pos, pd)\
        for ((pos) = (pd)->iter; pos < &(pd)->iter[(pd)->iter_len]; ++pos)\

int write_bio_enqueue(struct bio *bio);

void dbg_dump_bio(const char *prefix, struct bio *bio);

#endif