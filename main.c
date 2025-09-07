#include "bio.h"
#include "bnull.h"
#include "chrdev.h"
#include "pr_format.h"
#include "probes.h"
#include "registry.h"
#include "snapshot.h"
#include <linux/crypto.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/types.h>

static int __init bsnapshot_init(void) {
    int err = registry_init();
    if (err) {
        goto registry_failed;
    }
    err = chrdev_init();
    if (err) {
        goto chrdev_failed;
    }
    err = bnull_init();
    if (err) {
        goto bnull_failed;
    }
    err = snapshot_init();
    if (err) {
        goto bio_dw_failed;
    }
    err = snapshot_init();
    if (err) {
        goto snapshot_init_failed;
    }
    err = probes_init();
    if (err) {
        goto probes_failed;
    }
    return 0;

probes_failed:
    snapshot_cleanup();
snapshot_init_failed:
    snapshot_cleanup();
bio_dw_failed:
    bnull_cleanup();
bnull_failed:
    chrdev_cleanup();
chrdev_failed:
    registry_cleanup();
registry_failed:
    return err;
}

static void __exit bsnapshot_exit(void) {
    probes_cleanup();
    chrdev_cleanup();
    registry_cleanup();
    bnull_cleanup();
    snapshot_cleanup();
    snapshot_cleanup();
}

MODULE_AUTHOR("Francesco Donnini <donnini.francesco00@gmail.com>");
MODULE_DESCRIPTION("Block-device snapshot");
MODULE_LICENSE("GPL");

module_init(bsnapshot_init);
module_exit(bsnapshot_exit);