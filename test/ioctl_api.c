#include "include/ioctl_api.h"
#include <asm-generic/ioctl.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define CHRDEV_IOCTL_MAGIC      0xca
#define CHRDEV_IOCTL_ACTIVATE   _IOWR(CHRDEV_IOCTL_MAGIC, CHRDEV_IOCTL_ACTIVATE_NO, struct ioctl_params)
#define CHRDEV_IOCTL_DEACTIVATE _IOWR(CHRDEV_IOCTL_MAGIC, CHRDEV_IOCTL_DEACTIVATE_NO, struct ioctl_params)

enum {
    CHRDEV_IOCTL_ACTIVATE_NO = 0x70,
    CHRDEV_IOCTL_DEACTIVATE_NO,
    CHRDEV_IOCTL_ACTIVATE_MAX_NR
};

struct ioctl_params {
    const char   *path;
    size_t path_len;
    const char   *password;
    size_t password_len;
    int    error;
};

static int get_fd() {
    return open("/dev/snapshot_test", O_RDWR);
}

static void init_params(struct ioctl_params *p, const char *device, const char *pw) {
    p->path = device;
    p->path_len = strlen(device);
    p->password = pw;
    p->password_len = strlen(pw);
    p->error = 0xdeadbeef;
}

int activate(const char *device, const char *password) {
    int fd = get_fd();
    if (fd < 0) {
        return fd;
    }
    struct ioctl_params p;
    init_params(&p, device, password);
    int err = ioctl(fd, CHRDEV_IOCTL_ACTIVATE, &p);
    if (!err) {
        err = p.error;
    }
    close(fd);
    return err;
}

int deactivate(const char *device, const char *password) {
    int fd = get_fd();
    if (fd < 0) {
        return fd;
    }
    struct ioctl_params p;
    init_params(&p, device, password);
    int err = ioctl(fd, CHRDEV_IOCTL_DEACTIVATE, &p);
    if (!err) {
        err = p.error;
    }
    close(fd);
    return err;
}