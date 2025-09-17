#ifndef SNAP_MAP_H
#define SNAP_MAP_H
#include "../rbitmap/rbitmap32.h"
#include "session.h"
#include <linux/list.h>
#include <linux/time64.h>

/**
 * This struct keeps track of the sectors of a certain device which have been already saved by the module.
 * A snap_map is uniquely identified by the pair device number and session_created_on. It uses srcu because
 * the operations provided by rbitmap32 may block.
 */
struct snap_map {
    struct callback_head head;
    struct list_head     list;
    dev_t                device;
    struct timespec64    session_created_on;
    struct rbitmap32     bitmap;
};

int snap_map_init(void);

void snap_map_cleanup(void);

void snap_map_destroy(dev_t dev, struct timespec64 *created_on);

int snap_map_add_range(dev_t dev, struct timespec64 *created_on, sector_t lo, sector_t hi_excl, unsigned long *added);

int snap_map_create(dev_t dev, struct timespec64 *created_on);

#endif