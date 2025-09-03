#ifndef AOS_IOCTL_H
#define AOS_IOCTL_H
#include <asm/ioctl.h>
#include <linux/fs.h>
#include <linux/types.h>

#define IOCTL_SNAPSHOT_MAGIC      0xca
#define IOCTL_ACTIVATE_SNAPSHOT   _IOWR(IOCTL_SNAPSHOT_MAGIC, IOCTL_ACTIVATE_SNAPSHOT_NO, struct ioctl_params)
#define IOCTL_DEACTIVATE_SNAPSHOT _IOWR(IOCTL_SNAPSHOT_MAGIC, IOCTL_DEACTIVATE_SNAPSHOT_NO, struct ioctl_params)

enum {
    IOCTL_ACTIVATE_SNAPSHOT_NO = 0x70,
    IOCTL_DEACTIVATE_SNAPSHOT_NO,
    IOCTL_SNAPSHOT_MAX_NR
};

struct ioctl_params {
    char   *path;
    size_t path_len;
    char   *password;
    size_t password_len;
    int    error;
};

long chrdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

#endif