#include "include/chrdev.h"
#include "include/pr_format.h"
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/fs.h>

#define MY_CHARDEV_NAME "beautiful_device"
#define MY_MAX_MINORS 1

struct my_chrdev {
    struct cdev cdev;
};

static dev_t my_dev;

static struct my_chrdev devices[MY_MAX_MINORS];

static int my_open(struct inode *inode, struct file *file) {
    pr_debug(ss_pr_format("open() called\n"));
    return 0;
}

static ssize_t my_read(struct file *file, char __user *user_buffer, size_t size, loff_t *offset) {
    pr_debug(ss_pr_format("read() called\n"));
    return 0;
}

static ssize_t my_write(struct file *file, const char __user *user_buffer, size_t size, loff_t *offset) {
    pr_debug(ss_pr_format("write() called\n"));
    return 0;
}

static int my_release(struct inode *inode, struct file *file) {
    pr_debug(ss_pr_format("close() called\n"));
    return 0;
}

static long my_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    pr_debug(ss_pr_format("ioctl() called\n"));
    return 0;
}

struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .read = my_read,
    .write = my_write,
    .release = my_release,
    .unlocked_ioctl = my_unlocked_ioctl
};

static void cleanup(int max_minors) {
    for (int i = 0; i < max_minors; ++i) {
        cdev_del(&devices[i].cdev);
    }
    unregister_chrdev_region(my_dev, MY_MAX_MINORS);
}

int chrdev_init(void) {
    int err = alloc_chrdev_region(&my_dev, 0, MY_MAX_MINORS, MY_CHARDEV_NAME);
    if (err) {
        pr_debug(ss_pr_format("cannot register char-device \"%s\" because of error %d\n"), MY_CHARDEV_NAME, err);
        return err;
    }
    pr_debug(ss_pr_format("MAJOR NUMBER of \"%s\" is %d\n"), MY_CHARDEV_NAME, my_dev);
    for (int i = 0; i < MY_MAX_MINORS; ++i) {
        cdev_init(&devices[i].cdev, &my_fops);
        int err = cdev_add(&devices[i].cdev, MKDEV(my_dev, i), 1);
        if (err) {
            pr_debug(ss_pr_format("cannot add device with number (%d, %d) because of error %d\n"), my_dev, i, err);
            cleanup(i);
            return err;
        }
    }
    return 0;
}

void chrdev_cleanup(void) {
    cleanup(MY_MAX_MINORS);
}
