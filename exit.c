#include "include/chrdev.h"
#include <linux/init.h>
#include <linux/module.h>

static void __exit snapshot_exit(void) {
    chrdev_cleanup();
}

module_exit(snapshot_exit);
