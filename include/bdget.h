#ifndef AOS_BDGET_H
#define AOS_BDGET_H
#include <linux/blk_types.h>

int bdev_from_file(const char *path, dev_t *dev);

#endif /* AOS_BDGET_H */