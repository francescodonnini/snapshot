#include "include/chrdev.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/crypto.h>

MODULE_AUTHOR("Francesco Donnini <donnini.francesco00@gmail.com>");
MODULE_DESCRIPTION("Block-device snapshot");
MODULE_LICENSE("GPL");

static int __init snapshot_init(void) {
    chrdev_init();
    return 0;
}

module_init(snapshot_init);