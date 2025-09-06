#ifndef AOS_SNAPSHOT_H
#define AOS_SNAPSHOT_H
#include "bio.h"
#include <linux/bio.h>
#include <linux/time64.h>
#include <linux/types.h>

int snapshot_init(void);

void snapshot_cleanup(void);

void snapshot_save(struct bio *bio, struct timespec64 *arrival_time);

#endif