#ifndef AOS_UPDATE_SESSION_H
#define AOS_UPDATE_SESSION_H
#include <linux/blk_types.h>

int ext4_update_session(const char *dev_name, struct block_device *bdev);

int singlefilefs_update_session(const char *dev_name, struct block_device *bdev);

#endif