#ifndef AOS_MOUNTS_H
#define AOS_MOUNTS_H
#include <linux/types.h>

int init_procfs(void);

int find_mount(const char *dev_name, bool *found);

#endif