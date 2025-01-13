#include "include/chrdev_ioctl.h"
#include "include/chrdev.h"
#include "include/pr_format.h"
#include <linux/cdev.h>
#include <linux/device/class.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/uaccess.h>

#define MY_CHRDEV_NAME  "chrdev_snapshot"
#define MY_CHRDEV_CLASS "chrdev_cls_snapshot"
#define MY_CHRDEV_MNT   "snapshot_test"
#define MAX_BUF_SIZE    4096

struct fl_data {
    uint8_t *data;
    size_t  capacity;
};


struct chrdev {
    dev_t        dev;
    struct cdev  cdev;
    struct class *class;
};

static struct chrdev device;

static struct fl_data *mk_fl_data(size_t capacity) {
    struct fl_data *fp = kmalloc(sizeof(struct fl_data), GFP_KERNEL);
    if (fp == NULL) {
        return ERR_PTR(-ENOMEM);
    }
    uint8_t *buffer = kmalloc(capacity, GFP_KERNEL);
    if (buffer == NULL) {
        kfree(fp);
        return ERR_PTR(-ENOMEM);
    }
    fp->data = buffer;
    fp->capacity = capacity;
    return fp;
}

static int chrdev_open(struct inode *inode, struct file *file) {
    struct fl_data *fl_data = mk_fl_data(MAX_BUF_SIZE);
    if (IS_ERR(fl_data)) {
        pr_debug(pr_format("open() failed to allocate enough memory\n"));
        return PTR_ERR(fl_data);
    }
    file->private_data = fl_data;
    return 0;
}

static int chrdev_release(struct inode *inode, struct file *file) {
    kfree(file->private_data);
    return 0;
}

struct file_operations my_fops = {
    .owner   = THIS_MODULE,
    .open    = chrdev_open,
    .release = chrdev_release,
    .unlocked_ioctl = chrdev_ioctl
};

static int my_uevent(const struct device *dev, __attribute__((unused)) struct kobj_uevent_env *env) {
    add_uevent_var(env, "DEVMODE=%#o", 0440);
    return 0;
}

int chrdev_init(void) {
    int err = alloc_chrdev_region(&device.dev, 0, 1, MY_CHRDEV_NAME);
    if (err) {
        pr_debug(pr_format("cannot register char-device \"%s\" because of error %d\n"), MY_CHRDEV_NAME, err);
        return err;
    }
    
    cdev_init(&device.cdev, &my_fops);
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
