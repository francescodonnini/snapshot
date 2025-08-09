#include "bio.h"
#include "bnull.h"
#include "chrdev.h"
#include "probes.h"
#include "registry.h"
#include "snapshot.h"
#include <linux/init.h>
#include <linux/module.h>

static void __exit bsnapshot_exit(void) {
    probes_cleanup();
    chrdev_cleanup();
    registry_cleanup();
    bnull_cleanup();
    bio_deferred_work_cleanup();
    snapshot_cleanup();
}

module_exit(bsnapshot_exit);
