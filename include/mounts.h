#ifndef AOS_MOUNTS_H
#define AOS_MOUNTS_H
#include <linux/types.h>

int procfs_init(void);

int find_mount(const char *dev_name, bool *found);

#endif