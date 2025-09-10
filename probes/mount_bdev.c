#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "registry.h"
#include "update_session.h"
#include <linux/blk_types.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/printk.h>

int mount_bdev_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct file_system_type *type = get_arg1(struct file_system_type*, regs);
    if (!type || strcmp(type->name, "singlefilefs")) {
        pr_debug(pr_format("filesystem %s is not supported"), type->name);
        return -1;
    }
    // store the device name in the kretprobe instance data
    // so that it can be used in the return handler to check if the device has been successfully mounted
    char **data = (char**)kp->data;
    *data = get_arg3(char*, regs);
    return 0;
}

static inline struct block_device* d_bdev(struct dentry *dentry) {
    if (!dentry || !dentry->d_sb || !dentry->d_sb->s_bdev) {
        return NULL;
    }
    return dentry->d_sb->s_bdev;
}

/**
 * mount_bdev_handler checks whether mount_bdev completed successfully, then it registers
 * the device number (MAJOR, minor) in the registry if the block device just mounted was previously
 * registered by the user (using the 'activate snapshot' command). When the device number is added to the
 * registry, a folder for the blocks snapshot is created in /snapshots.
 */
int mount_bdev_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct dentry *dentry = get_rval(struct dentry*, regs);
    if (IS_ERR(dentry)) {
        return 0;
    }
    char **data = (char**)kp->data;
    struct block_device *bdev = d_bdev(dentry);
    if (bdev) {
        if (singlefilefs_update_session(*data, bdev)) {
            pr_debug(pr_format("cannot create or update session of device %s"), *data);
        }
    }
    return 0;
}