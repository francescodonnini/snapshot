#include "include/chrdev.h"
#include "include/registry.h"
#include <linux/init.h>
#include <linux/module.h>

static void __exit snapshot_exit(void) {
    chrdev_cleanup();
    registry_cleanup();
}

module_exit(snapshot_exit);
