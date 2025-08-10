#ifndef AOS_SNAPSHOT_H
#define AOS_SNAPSHOT_H
#include "bio.h"
#include "hashset.h"
#include <linux/bio.h>
#include <linux/types.h>

int snapshot_init(void);

void snapshot_cleanup(void);

int snapshot_create(const char *session);

int snapshot_save(struct bio *bio);

#endif