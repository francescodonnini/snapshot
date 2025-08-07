#ifndef AOS_SNAPSHOT_H
#define AOS_SNAPSHOT_H
#include "bio.h"
#include <linux/bio.h>

int snapshotfs_init(void);

void snapshotfs_cleanup(void);

int snapshot_create(dev_t dev, const char *snapshot);

int snapshot_restore(dev_t dev);

int snapshot_save(struct bio *bio);

#endif