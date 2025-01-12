#include "include/chrdev.h"
#include <linux/crypto.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/types.h>

static dev_t chrdev_major_number;
module_param(chrdev_major_number, int, 0444);

static int __init snapshot_init(void) {
    chrdev_init(&chrdev_major_number);
    return 0;
}

MODULE_AUTHOR("Francesco Donnini <donnini.francesco00@gmail.com>");
MODULE_DESCRIPTION("Block-device snapshot");
MODULE_LICENSE("GPL");

module_init(snapshot_init);