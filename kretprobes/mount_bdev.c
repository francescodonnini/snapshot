#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "registry.h"
#include <linux/blk_types.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/printk.h>

static inline struct file_system_type* get_fstype(struct pt_regs *regs) {
    return get_arg1(struct file_system_type*, regs);
}

static inline char* get_dev_name(struct pt_regs *regs) {
    return get_arg3(char*, regs);
}

int mount_bdev_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    pr_debug(
        pr_format("mount_bdev(%s, %s) called\n"),
        get_fstype(regs)->name,
        get_dev_name(regs));
    if (!registry_lookup(get_dev_name(regs))) {
        // skip the return handler if the device is not registered
        return 1;
    }
    // store the device name in the kretprobe instance data
    // so that it can be used in the return handler to check if the device has been successfully mounted
    struct mount_bdev_data *data = (struct mount_bdev_data*)kp->data;
    data->dev_name = get_dev_name(regs);
    return 0;
}

int mount_bdev_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct dentry *dentry = get_rval(struct dentry*, regs);
    if (IS_ERR(dentry)) {
        pr_debug(pr_format("mount_bdev failed with error: %ld\n"), PTR_ERR(dentry));
        return 0;
    }
    struct block_device *bdev = dentry->d_sb->s_bdev;
    struct mount_bdev_data *data = (struct mount_bdev_data*)kp->data;
    pr_debug(pr_format("mounted block device (%d, %d) with bdev %s\n"),
             MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev), data->dev_name);
    registry_update(data->dev_name, bdev->bd_dev);
    return 0;
}