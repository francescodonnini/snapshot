#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "registry.h"
#include "update_session.h"
#include <linux/blk-mq.h>
#include <linux/blk_types.h>
#include <linux/blkdev.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/loop.h>
#include <linux/major.h>
#include <linux/printk.h>

static inline char* get_dev_name(struct pt_regs *regs) {
    return get_arg3(char*, regs);
}

int mount_bdev_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    // store the device name in the kretprobe instance data
    // so that it can be used in the return handler to check if the device has been successfully mounted
    struct mount_bdev_data *data = (struct mount_bdev_data*)kp->data;
    data->dev_name = get_dev_name(regs);
    return 0;
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
        pr_debug(pr_format("mount_bdev failed with error: %ld"), PTR_ERR(dentry));
        return 0;
    }
    struct mount_bdev_data *data = (struct mount_bdev_data*)kp->data;
    struct block_device *bdev = dentry->d_sb->s_bdev;
    if (registry_lookup_active(bdev->bd_dev)) {
        pr_debug(pr_format("device %s with device number (%d, %d) does not exist or is already registered"),
                 data->dev_name, MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev));
    } else {
        update_session(data->dev_name, bdev);
    }
    return 0;
}