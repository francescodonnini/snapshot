#include "auth.h"
#include "bio.h"
#include "bnull.h"
#include "chrdev.h"
#include "pr_format.h"
#include "probes.h"
#include "registry.h"
#include "snapshot.h"
#include <linux/crypto.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/types.h>

static char *password;
module_param(password, charp, 0);
MODULE_PARM_DESC(password, "Password required to use snapshot service");

static int __init bsnapshot_init(void) {
    int err = auth_set_password(password);
    if (err) {
        return err;
    }
    err = registry_init();
    if (err) {
        goto registry_failed;
    }
    err = chrdev_init();
    if (err) {
        goto chrdev_failed;
    }
    err = snapshot_init();
    if (err) {
        goto snapshot_init_failed;
    }
    err = probes_init();
    if (err) {
        goto probes_init_failed;
    }
    err = bnull_init();
    if (err) {
        goto bnull_init_failed;
    }
    return err;

bnull_init_failed:
    probes_cleanup();
probes_init_failed:
    snapshot_cleanup();
snapshot_init_failed:
    chrdev_cleanup();
chrdev_failed:
    registry_cleanup();
registry_failed:
    auth_clear_password();
    pr_err("bsnapshots_init failed, got error %d", err);
    return err;
}

static void __exit bsnapshot_exit(void) {
    bnull_cleanup();
    probes_cleanup();
    snapshot_cleanup();
    chrdev_cleanup();
    registry_cleanup();
    auth_clear_password();
}

MODULE_AUTHOR("Francesco Donnini <donnini.francesco00@gmail.com>");
MODULE_DESCRIPTION("Block-device snapshot");
MODULE_LICENSE("GPL");

module_init(bsnapshot_init);
module_exit(bsnapshot_exit);