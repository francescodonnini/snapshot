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

static inline struct block_device* s_bdev(struct super_block *sb) {
    if (!sb || !sb->s_bdev) {
        pr_err("cannot read struct block_device from super_block");
        return NULL;
    }
    return sb->s_bdev;
}

int ext4_fill_super_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct super_block *sb = get_arg1(struct super_block*, regs);
    struct fs_context *fc = get_arg2(struct fs_context*, regs);
    struct block_device *bdev = s_bdev(sb);
    if (bdev) {
        ext4_update_session(fc->source, bdev);
    }
    dev_t *data = (dev_t*)kp->data;
    *data = bdev->bd_dev;
    return 0;
}

int ext4_fill_super_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    int err = get_rval(int, regs);
    dev_t *dev = (dev_t*)kp->data;
    // The session has to be destroyed if the mount operation fails
    if (err) {
        registry_session_destroy(*dev);
        pr_debug(pr_format("cannot fill super block of device %d:%d, got error %d"), MAJOR(*dev), MINOR(*dev), err);
    }
    return 0;
}

static const char* f_source(struct fs_context *fc) {
    if (!fc || !fc->source) {
        return NULL;
    }
    return fc->source;
}

int get_tree_bdev_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct fs_context *fc = get_arg1(struct fs_context*, regs);
    if (!f_source(fc)) {
        return -1;
    }
    struct fs_context **data = (struct fs_context**)kp->data;
    *data = fc;
    return 0;
}

static struct block_device *f_bdev(struct fs_context *fc) {
    if (!fc->root || !fc->root->d_sb || !fc->root->d_sb->s_bdev) {
        return NULL;
    }
    return fc->root->d_sb->s_bdev;
}

int get_tree_bdev_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    int err = get_rval(int, regs);
    struct fs_context **data = (struct fs_context**)kp->data;
    struct fs_context *fc = *data;
    struct block_device *bdev = f_bdev(fc);
    if (!err && bdev) {
        registry_session_get(fc->source, bdev->bd_dev);
    }
    return 0;
}

int mount_bdev_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
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
        pr_debug(pr_format("failed with error: %ld"), PTR_ERR(dentry));
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

static inline dev_t* p_dev(struct path *path, dev_t *dev) {
    if (!path || !path->mnt || !path->mnt->mnt_sb || !path->mnt->mnt_sb->s_bdev) {
        return NULL;
    }
    *dev = path->mnt->mnt_sb->s_bdev->bd_dev;
    return dev;
}

int path_umount_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    dev_t dev;
    if (!p_dev(get_arg1(struct path*, regs), &dev)) {
        return -1;
    }
    dev_t *data = (dev_t*)kp->data;
    *data = dev;
    return 0;
}

int path_umount_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    if (get_rval(int, regs)) {
        return 0;
    }
    dev_t *dev = (dev_t*)kp->data;
    registry_session_put(*dev);
    return 0;
}