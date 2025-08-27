#ifndef AOS_UPDATE_SESSION_H
#define AOS_UPDATE_SESSION_H
#include <linux/blkdev.h>
#include <linux/types.h>

int update_session(const char *dev_name, struct block_device *bdev);

#endif