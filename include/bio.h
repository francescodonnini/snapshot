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

struct bio_private_data {
    struct bio       *orig_bio;
    sector_t          sector;
    int               nr_pages;
    struct page_iter  iter[];
};

int bio_deferred_work_init(void);

void bio_deferred_work_cleanup(void);

bool bio_enqueue(struct bio *bio);

void dbg_dump_bio(const char *prefix, struct bio *bio);

#endif