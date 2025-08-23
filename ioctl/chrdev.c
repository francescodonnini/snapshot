#include "chrdev_ioctl.h"
#include "chrdev.h"
#include "pr_format.h"
#include "registry.h"
#include <linux/cdev.h>
#include <linux/device/class.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/uaccess.h>

#define MY_CHRDEV_NAME  "blkdev_snapshot"
#define MY_CHRDEV_CLASS "blkdev_snapshot_class"
#define MY_CHRDEV_MNT   "blkdev_snapshot_device"

struct chrdev {
    dev_t           dev;
    struct cdev     cdev;
    struct class   *class;
    struct kobject *kobj;
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

static ssize_t session_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return registry_show_session(buf, PAGE_SIZE - 1);
}

static struct kobj_attribute session_attribute = __ATTR(active, 0664, session_show, NULL);

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

    device.kobj = kobject_create_and_add("sessions", kernel_kobj);
    if (!device.kobj) {
        pr_debug(pr_format("cannot create kobject '%s'"), "sessions");
        goto no_kobj;
    }
    
    err = sysfs_create_file(device.kobj, &session_attribute.attr);
    if (err) {
        pr_debug(pr_format("cannot create attribute '%s'"), session_attribute.attr.name);
        goto no_attr;
    }
    return 0;

no_attr:
    kobject_put(device.kobj);
no_kobj:
    device_destroy(device.class, device.dev);
no_device:
    class_destroy(device.class);
no_cdev_class:
    cdev_del(&device.cdev);
no_cdev_add:
    unregister_chrdev_region(device.dev, 1);
    return err;
}

void chrdev_cleanup(void) {
    sysfs_remove_file(device.kobj, &session_attribute.attr);
    kobject_put(device.kobj);
    device_destroy(device.class, device.dev);
    class_destroy(device.class);
    cdev_del(&device.cdev);
    unregister_chrdev_region(device.dev, 1);
}
