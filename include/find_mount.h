#ifndef AOS_MOUNTS_H
#define AOS_MOUNTS_H
#include <linux/types.h>

int procfs_init(void);

void procfs_cleanup(void);

int get_fdev(const char *mntpnt, dev_t *dev);

#endif