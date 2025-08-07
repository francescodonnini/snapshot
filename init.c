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

static int __init snapshot_init(void) {
    int err = probes_init();
    if (err) {
        goto probes_failed;
    }
    err = registry_init();
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
    err = bio_deferred_work_init();
    if (err) {
        goto bio_dw_failed;
    }
    err = snapshotfs_init();
    if (err) {
        goto snapshotfs_init_failed;
    }
    return 0;

snapshotfs_init_failed:
    bio_deferred_work_cleanup();
bio_dw_failed:
    bnull_cleanup();
bnull_failed:
    chrdev_cleanup();
chrdev_failed:
    registry_cleanup();
registry_failed:
    probes_cleanup();
probes_failed:
    return err;
}

MODULE_AUTHOR("Francesco Donnini <donnini.francesco00@gmail.com>");
MODULE_DESCRIPTION("Block-device snapshot");
MODULE_LICENSE("GPL");

module_init(snapshot_init);