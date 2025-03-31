#include "include/chrdev.h"
#include "include/mounts.h"
#include "include/pr_format.h"
#include "include/registry.h"
#include <linux/crypto.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/types.h>

static int __init snapshot_init(void) {
    // chrdev_init();
    // registry_init();
    init_procfs();
    pr_debug(pr_format("found=%d\n"), find_mount("/home/francesco/Documents/singlefile-FS/mount"));
    return 0;
}

MODULE_AUTHOR("Francesco Donnini <donnini.francesco00@gmail.com>");
MODULE_DESCRIPTION("Block-device snapshot");
MODULE_LICENSE("GPL");

module_init(snapshot_init);