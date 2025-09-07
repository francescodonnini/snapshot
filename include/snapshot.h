#ifndef AOS_SNAPSHOT_H
#define AOS_SNAPSHOT_H
#include "bio.h"
#include <linux/bio.h>
#include <linux/time64.h>
#include <linux/types.h>

int snapshot_init(void);

void snapshot_cleanup(void);

int write_bio_enqueue(struct bio *bio);

#endif