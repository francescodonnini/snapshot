#include "bio.h"
#include "bnull.h"
#include "chrdev.h"
#include "find_mount.h"
#include "probes.h"
#include "registry.h"
#include "snapshot.h"
#include <linux/init.h>
#include <linux/module.h>

static void __exit snapshot_exit(void) {
    probes_cleanup();
    chrdev_cleanup();
    registry_cleanup();
    procfs_cleanup();
    bnull_cleanup();
    bio_deferred_work_cleanup();
    snapshotfs_cleanup();
}

module_exit(snapshot_exit);
