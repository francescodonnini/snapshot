#ifndef SNAP_MAP_H
#define SNAP_MAP_H
#include "../rbitmap/rbitmap32.h"
#include "session.h"
#include <linux/list.h>
#include <linux/time64.h>

struct snap_map {
    struct list_head     list;
    struct rbitmap32     bitmap;
    struct timespec64    session_created_on;
    dev_t                device;
    struct callback_head head;
};

int snap_map_init(void);

void snap_map_cleanup(void);

void snap_map_destroy(dev_t dev, struct timespec64 *created_on);

int snap_map_add_range(dev_t dev, struct timespec64 *created_on, sector_t lo, sector_t hi_excl, unsigned long *added);

int snap_map_add_sector(dev_t dev, struct timespec64 *created_on, sector_t sector, bool *added);

int snap_map_create(dev_t dev, struct timespec64 *created_on);

#endif