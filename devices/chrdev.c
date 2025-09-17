#include "chrdev_ioctl.h"
#include "chrdev.h"
#include "pr_format.h"
#include "registry.h"
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/device/class.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>

#define DEVICE_NAME  "bsnapshot"
#define DEVICE_CLASS "bsnapshot_cls"

struct bsnapshot_cdev {
    dev_t           dev;
    struct cdev     cdev;
    struct class   *class;
};

static struct bsnapshot_cdev device;

static const struct file_operations ops = {
    .owner   = THIS_MODULE,
    .unlocked_ioctl = chrdev_ioctl
};

static int my_uevent(const struct device *dev, __attribute__((unused)) struct kobj_uevent_env *env) {
    add_uevent_var(env, "DEVMODE=%#o", 0440);
    return 0;
}

static ssize_t session_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return registry_show_session(buf, PAGE_SIZE - 1);
}

DEVICE_ATTR(active, 0440, session_show, NULL);

static struct attribute *bsnapshot_dev_attrs[] = {
    &dev_attr_active.attr,
    NULL,
};

ATTRIBUTE_GROUPS(bsnapshot_dev);

/**
 * chrdev_init - create a device to handle ioctl operations from user space.
 */
int chrdev_init(void) {
    int err = alloc_chrdev_region(&device.dev, 0, 1, DEVICE_NAME);
    if (err) {
        pr_err("cannot register char-device %s, got error %d", DEVICE_NAME, err);
        return err;
    }

    cdev_init(&device.cdev, &ops);
    device.cdev.owner = THIS_MODULE;
    err = cdev_add(&device.cdev, device.dev, 1);
    if (err) {
        pr_err("cannot add device %d:%d, got error %d", MAJOR(device.dev), MINOR(device.dev), err);
        goto out;
    }

    struct class *device_class = class_create(DEVICE_CLASS);
    if (IS_ERR(device_class)) {
        err = PTR_ERR(device_class);
        pr_err("cannot create class %s, got error %d", DEVICE_CLASS, err);
        goto out2;
    }
    device_class->dev_uevent = my_uevent;
    device.class = device_class;

    struct device *dp = device_create_with_groups(device.class, NULL, device.dev, NULL, bsnapshot_dev_groups, DEVICE_NAME);
    if (IS_ERR(dp)) {
        err = PTR_ERR(dp);
        pr_err("cannot create device /dev/%s, got error %d", DEVICE_NAME, err);
        goto out3;
    }
    return 0;

out3:
    class_destroy(device.class);
    device.class = NULL;
out2:
    cdev_del(&device.cdev);
out:
    unregister_chrdev_region(device.dev, 1);
    device.dev = 0;
    return err;
}

void chrdev_cleanup(void) {
    device_destroy(device.class, device.dev);
    class_destroy(device.class);
    device.class = NULL;
    cdev_del(&device.cdev);
    unregister_chrdev_region(device.dev, 1);
    device.dev = 0;
}
