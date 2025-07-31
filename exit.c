#include "bio.h"
#include "chrdev.h"
#include "find_mount.h"
#include "probes.h"
#include "registry.h"
#include <linux/init.h>
#include <linux/module.h>

static void __exit snapshot_exit(void) {
    probes_cleanup();
    chrdev_cleanup();
    registry_cleanup();
    procfs_cleanup();
    bio_deferred_work_cleanup();
}

module_exit(snapshot_exit);
