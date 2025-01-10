#include "include/pr_format.h"
#include "include/hash.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/crypto.h>

MODULE_AUTHOR("Francesco Donnini <donnini.francesco00@gmail.com>");
MODULE_DESCRIPTION("Block-device snapshot");
MODULE_LICENSE("GPL");

static int __init snapshot_init(void) {
    char h[21];
    if (hash("sha1", "hello", strlen("hello"), h)) {
        return 0;
    }
    h[20] = 0;
    printk(ss_pr_format("%s\n"), h);
    return 0;
}

module_init(snapshot_init);