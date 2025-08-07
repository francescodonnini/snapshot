#ifndef AOS_BIO_H
#define AOS_BIO_H
#include <linux/bio.h>
#include <linux/mm_types.h>
#include <linux/types.h>

struct bio_block {
    sector_t      sector;
    int           nr_pages;
    struct page **pages;
};

struct bio_private_data {
    struct bio       *orig_bio;
    struct bio_block  block;
};

int bio_deferred_work_init(void);

void bio_deferred_work_cleanup(void);

bool bio_enqueue(struct bio *bio);

void dbg_dump_bio(const char *prefix, struct bio *bio);

#endif