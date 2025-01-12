#ifndef AOS_IOCTL_H
#define AOS_IOCTL_H
#include <asm/ioctl.h>
#include <linux/fs.h>
#include <linux/types.h>

#define CHRDEV_IOCTL_MAGIC         0xca
#define CHRDEV_IOCTL_ACTIVATE      _IOWR(CHRDEV_IOCTL_MAGIC, CHRDEV_IOCTL_ACTIVATE_NO, struct ioctl_params)
#define CHRDEV_IOCTL_DEACTIVATE    _IOWR(CHRDEV_IOCTL_MAGIC, CHRDEV_IOCTL_DEACTIVATE_NO, struct ioctl_params)

enum {
    CHRDEV_IOCTL_ACTIVATE_NO = 0x70,
    CHRDEV_IOCTL_DEACTIVATE_NO,
    CHRDEV_IOCTL_ACTIVATE_MAX_NR
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