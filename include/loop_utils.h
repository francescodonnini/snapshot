#ifndef AOS_LOOP_H
#define AOS_LOOP_H
#include <linux/blkdev.h>
#include <linux/kdev_t.h>
#include <linux/major.h>

static inline bool is_loop_device(struct block_device *bdev) {
    return MAJOR(bdev->bd_dev) == LOOP_MAJOR;
}

char *backing_loop_device_file(struct block_device *bdev, char *buf);

#endif