#include "kretprobe_handlers.h"
#include "loop_utils.h"
#include "pr_format.h"
#include "registry.h"
#include <linux/blk-mq.h>
#include <linux/blk_types.h>
#include <linux/blkdev.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/loop.h>
#include <linux/major.h>
#include <linux/printk.h>

static inline struct file_system_type* get_file_system_type(struct pt_regs *regs) {
    return get_arg1(struct file_system_type*, regs);
}

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

static inline int try_update_loop_dev(struct block_device *bdev, char *buf) {
    const char *ip = backing_loop_device_file(bdev, buf);    
    if (IS_ERR(ip)) {
        return PTR_ERR(ip);
    } else {
        return registry_update(ip, bdev->bd_dev);
    }
}

/**
 * registry_update_loop_device gets the path of the file backing the loop device and
 * updates the registry with the device number associated to the image.
 * Returns 0 on success, < 0 otherwise. Possible errors are -ENOMEM if it is not possible to make
 * enough space to hold the path of the image file or if the update operation to the registry fails
 * (see registry_update for more details).
 */
static int registry_update_loop_device(struct block_device *bdev) {
    char *buf = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!buf) {
        return -ENOMEM;
    }
    int err = try_update_loop_dev(bdev, buf);
    kfree(buf);
    return err;
}

static inline void dbg_already_registered(const char *dev_name, struct block_device *bdev) {
    pr_debug(pr_format("device %s associated to device number (%d, %d) is already registered"),
             dev_name, MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev));
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
    if (registry_lookup_dev(bdev->bd_dev)) {
        dbg_already_registered(data->dev_name, bdev);
        return 0;
    }
    if (is_loop_device(bdev)) {
        registry_update_loop_device(bdev);
    } else {
        registry_update(data->dev_name, bdev->bd_dev);
    }
    return 0;
}