#include "chrdev_ioctl.h"
#include "chrdev.h"
#include "pr_format.h"
#include <linux/cdev.h>
#include <linux/device/class.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/uaccess.h>

#define MY_CHRDEV_NAME  "chrdev_snapshot"
#define MY_CHRDEV_CLASS "chrdev_cls_snapshot"
#define MY_CHRDEV_MNT   "snapshot_test"

struct chrdev {
    dev_t        dev;
    struct cdev  cdev;
    struct class *class;
};

static struct chrdev device;

struct file_operations chrdev_fops = {
    .owner   = THIS_MODULE,
    .unlocked_ioctl = chrdev_ioctl
};

static int my_uevent(const struct device *dev, __attribute__((unused)) struct kobj_uevent_env *env) {
    add_uevent_var(env, "DEVMODE=%#o", 0440);
    return 0;
}

/**
 * chrdev_init - create a device to handle ioctl operations from user space.
 */
int chrdev_init(void) {
    int err = alloc_chrdev_region(&device.dev, 0, 1, MY_CHRDEV_NAME);
    if (err) {
        pr_debug(pr_format("cannot register char-device \"%s\" because of error %d\n"), MY_CHRDEV_NAME, err);
        return err;
    }
    
    cdev_init(&device.cdev, &chrdev_fops);
    device.cdev.owner = THIS_MODULE;
    err = cdev_add(&device.cdev, device.dev, 1);
    if (err) {
        pr_debug(pr_format("cannot add device with number (%d, %d) because of error %d\n"), MAJOR(device.dev), MINOR(device.dev), err);
        goto no_cdev_add;
    }

    device.class = class_create(MY_CHRDEV_CLASS);
    if (IS_ERR(device.class)) {
        err = PTR_ERR(device.class);
        pr_debug(pr_format("cannot create class \"%s\" because of error %d\n"), MY_CHRDEV_CLASS, err);
        goto no_cdev_class;
    }
    device.class->dev_uevent = my_uevent;
    struct device *d = device_create(device.class, NULL, device.dev, NULL, MY_CHRDEV_MNT);
    if (IS_ERR(d)) {
        err = PTR_ERR(d);
        pr_debug(pr_format("cannot create device /dev/%s because of error %d\n"), MY_CHRDEV_MNT, err);
        goto no_device;
    }
    return 0;

no_device:
    class_destroy(device.class);
no_cdev_class:
    cdev_del(&device.cdev);
no_cdev_add:
    unregister_chrdev_region(device.dev, 1);
    return err;
}

void chrdev_cleanup(void) {
    device_destroy(device.class, device.dev);
    class_destroy(device.class);
    cdev_del(&device.cdev);
    unregister_chrdev_region(device.dev, 1);
}
