#ifndef AOS_BIO_H
#define AOS_BIO_H
#include <linux/bio.h>
#include <linux/types.h>

int bio_deferred_work_init(void);

void bio_deferred_work_cleanup(void);

bool bio_enqueue(struct bio *bio);

void dbg_dump_bio(const char *prefix, struct bio *bio);

#endif