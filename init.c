#include "include/chrdev.h"
#include "include/find_mount.h"
#include "include/pr_format.h"
#include "include/probes.h"
#include "include/registry.h"
#include <linux/crypto.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/types.h>

static int __init snapshot_init(void) {
    probes_init();
    registry_init();
    procfs_init();
    chrdev_init();
    return 0;
}

MODULE_AUTHOR("Francesco Donnini <donnini.francesco00@gmail.com>");
MODULE_DESCRIPTION("Block-device snapshot");
MODULE_LICENSE("GPL");

module_init(snapshot_init);