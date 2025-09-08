#ifndef SNAP_MAP_H
#define SNAP_MAP_H
#include "../rbitmap/rbitmap32.h"
#include "session.h"
#include <linux/list.h>
#include <linux/time64.h>

struct snap_map {
    struct list_head  list;
    struct rbitmap32  bitmap;
    struct timespec64 session_created_on;
    dev_t             device;
};

int snap_map_init(void);

void snap_map_cleanup(void);

int snap_map_add_sector(dev_t dev, struct timespec64 *created_on, sector_t sector, bool *added);

int snap_map_create(dev_t dev, struct timespec64 *created_on);

#endif