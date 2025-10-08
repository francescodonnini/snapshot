#ifndef AOS_SNAPSHOT_H
#define AOS_SNAPSHOT_H
#include "bio.h"
#include <linux/bio.h>
#include <linux/time64.h>
#include <linux/types.h>

int snapshot_init(const char *directory);

void snapshot_cleanup(void);

void snap_map_destroy(dev_t dev, struct timespec64 *created_on);

int write_bio_enqueue(struct bio *bio);

#endif