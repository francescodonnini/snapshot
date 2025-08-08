#ifndef AOS_SNAPSHOT_H
#define AOS_SNAPSHOT_H
#include "bio.h"
#include "hashset.h"
#include <linux/bio.h>
#include <linux/types.h>

int snapshotfs_init(void);

void snapshotfs_cleanup(void);

int snapshot_create(dev_t dev, const char *session, struct hashset *set);

int snapshot_restore(dev_t dev);

int snapshot_save(struct bio *bio);

#endif