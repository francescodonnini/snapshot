#include "include/chrdev_ioctl.h"
#include "include/api.h"
#include "include/pr_format.h"
#include <asm/errno.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

static struct ioctl_params *copy_ioctl_params_from_user(struct ioctl_params *user_buffer) {
    long err = 0;
    if (!access_ok(user_buffer, sizeof(struct ioctl_params))) {
        err = -EINVAL;
        goto no_buffer;
    }
    struct ioctl_params *buffer = kmalloc(sizeof(struct ioctl_params), GFP_KERNEL);
    if (buffer == NULL) {
        err = -ENOMEM;
        goto no_buffer;
    }
    long rem = copy_from_user(buffer, user_buffer, sizeof(struct ioctl_params));
    if (rem < 0) {
        err = -EINVAL;
        goto no_ioctl_params_copy;
    }
    char *path = kmalloc(buffer->path_len + 1, GFP_KERNEL);
    if (path == NULL) {
        err = -ENOMEM;
        goto no_ioctl_params_copy;
    }
    char *password = kmalloc(buffer->password_len + 1, GFP_KERNEL);
    if (password == NULL) {
        err = -ENOMEM;
        goto no_password;
    }
    if (!access_ok(buffer->path, buffer->path_len)) {
        err = -EINVAL;
        goto no_param_copy;
    }
    rem = copy_from_user(path, buffer->path, buffer->path_len);
    if (rem < 0) {
        err = -EINVAL;
        goto no_param_copy;
    }
    if (!access_ok(buffer->password, buffer->password_len)) {
        err = -EINVAL;
        goto no_param_copy;
    }
    rem = copy_from_user(password, buffer->password, buffer->password_len);
    if (rem < 0) {
        err = -EINVAL;
        goto no_param_copy;
    }
    buffer->path = path;
    buffer->password = password;
    return buffer;

no_param_copy:
    kfree(password);
no_password:
    kfree(path);
no_ioctl_params_copy:
    kfree(buffer);
no_buffer:
    return ERR_PTR(err);
}

static void free_kernel_buffer(struct ioctl_params *kernel_buffer) {
    kfree(kernel_buffer->path);
    kfree(kernel_buffer->password);
    kfree(kernel_buffer);
}

static long do_activate(struct ioctl_params *params) {
    struct ioctl_params *p = copy_ioctl_params_from_user(params);
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
    struct ioctl_params *p = copy_ioctl_params_from_user(params);
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
    if (_IOC_TYPE(cmd) != CHRDEV_IOCTL_MAGIC) {
        pr_debug(pr_format("wrong magic number, expected %d but got %d\n"), CHRDEV_IOCTL_MAGIC, _IOC_TYPE(cmd));
        return -EINVAL;
    }
    if (_IOC_NR(cmd) > CHRDEV_IOCTL_ACTIVATE_MAX_NR) {
        pr_debug(pr_format("number too high\n"));
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
        case CHRDEV_IOCTL_ACTIVATE:
            if (!(_IOC_DIR(cmd) & (_IOC_READ | _IOC_WRITE))) {
                return -EINVAL;
            }
            return do_activate((struct ioctl_params*)arg);
        case CHRDEV_IOCTL_DEACTIVATE:
            if (!(_IOC_DIR(cmd) & (_IOC_READ | _IOC_WRITE))) {
                return -EINVAL;
            }
            return do_deactivate((struct ioctl_params*)arg);
        default:
            return -ENOTTY;
    }
    return 0;
}
