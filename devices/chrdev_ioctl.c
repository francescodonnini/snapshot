#include "chrdev_ioctl.h"
#include "api.h"
#include "pr_format.h"
#include <asm/errno.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

static int copy_password(struct ioctl_params *buffer) {
    if (!access_ok(buffer->password, buffer->password_len)) {
        return -EINVAL;
    }
    char *password = kzalloc(buffer->password_len + 1, GFP_KERNEL);
    if (!password) {
        return -ENOMEM;
    }
    unsigned long err = copy_from_user(password, buffer->password, buffer->password_len);
    if (err < 0) {
        kfree(password);
        return -EINVAL;
    }
    buffer->password = password;
    return 0;
}

static int copy_path(struct ioctl_params *buffer) {
    if (!access_ok(buffer->path, buffer->path_len)) {
        return -EINVAL;
    }
    char *path = kzalloc(buffer->path_len + 1, GFP_KERNEL);
    if (!path) {
        return -ENOMEM;
    }
    unsigned long err = copy_from_user(path, buffer->path, buffer->path_len);
    if (err < 0) {
        kfree(path);
        return -EINVAL;
    }
    buffer->path = path;
    return 0;
}

static int copy_buffer(struct ioctl_params **kernel_buffer, struct ioctl_params *user_buffer) {
    if (!access_ok(user_buffer, sizeof(*user_buffer))) {
        return -EINVAL;
    }
    struct ioctl_params *buffer = kzalloc(sizeof(*user_buffer), GFP_KERNEL);
    if (buffer == NULL) {
        return -ENOMEM;
    }
    long err = copy_from_user(buffer, user_buffer, sizeof(*user_buffer));
    if (err < 0) {
        kfree(buffer);
        return -EINVAL;
    }
    *kernel_buffer = buffer;
    return 0;
}

static struct ioctl_params *copy_params(struct ioctl_params *user_buffer) {
    struct ioctl_params *buffer;
    int err = copy_buffer(&buffer, user_buffer);
    if (err) {
        goto out;
    }
    err = copy_path(buffer);
    if (err) {
        goto out;
    }
    err = copy_password(buffer);    
    if (err) {
        goto no_password;
    }
    return buffer;

no_password:
    kfree(buffer->path);
out:
    kfree(buffer);
    return ERR_PTR(err);
}

static void free_kernel_buffer(struct ioctl_params *kernel_buffer) {
    kfree(kernel_buffer->path);
    kfree(kernel_buffer->password);
    kfree(kernel_buffer);
}

static long do_activate(struct ioctl_params *params) {
    struct ioctl_params *p = copy_params(params);
    if (IS_ERR(p)) {
        return PTR_ERR(p);
    }
    long err = 0;
    int irval = activate_snapshot(p->path, p->password);
    long rem = copy_to_user(&(params->error), &irval, sizeof(irval));
    if (rem < 0) {
        err = -EINVAL;
    }
    free_kernel_buffer(p);
    return err;
}

static long do_deactivate(struct ioctl_params *params) {
    struct ioctl_params *p = copy_params(params);
    if (IS_ERR(p)) {
        return PTR_ERR(p);
    }
    long err = 0;
    int irval = deactivate_snapshot(p->path, p->password);
    long rem = copy_to_user(&(params->error), &irval, sizeof(irval));
    if (rem < 0) {
        err = -EINVAL;
    }
    free_kernel_buffer(p);
    return err;
}

static long check_ioctl_cmd(unsigned int cmd) {
    if (_IOC_TYPE(cmd) != IOCTL_SNAPSHOT_MAGIC) {
        pr_err("wrong magic number, expected %d but got %d", IOCTL_SNAPSHOT_MAGIC, _IOC_TYPE(cmd));
        return -EINVAL;
    }
    if (_IOC_NR(cmd) > IOCTL_SNAPSHOT_MAX_NR) {
        pr_err("number too high");
        return -EINVAL;
    }
    return 0;
}

long chrdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    long err = check_ioctl_cmd(cmd);
    if (err) {
        return err;
    }
    switch (cmd) {
        case IOCTL_ACTIVATE_SNAPSHOT:
            if (!(_IOC_DIR(cmd) & (_IOC_READ | _IOC_WRITE))) {
                return -EINVAL;
            }
            return do_activate((struct ioctl_params*)arg);
        case IOCTL_DEACTIVATE_SNAPSHOT:
            if (!(_IOC_DIR(cmd) & (_IOC_READ | _IOC_WRITE))) {
                return -EINVAL;
            }
            return do_deactivate((struct ioctl_params*)arg);
        default:
            return -ENOTTY;
    }
    return 0;
}
