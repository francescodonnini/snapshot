#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "registry.h"
#include "update_session.h"
#include <linux/printk.h>

static inline int get_bdev_safe(struct fs_context *fc, struct block_device **bdev) {
    if (!fc) {
        pr_debug(pr_format("fs_context is NULL"));
        return -1;
    }
    if (!fc->root) {
        pr_debug(pr_format("root is NULL"));
        return -1;
    }
    if (!fc->root->d_sb) {
        pr_debug(pr_format("super block is NULL"));
        return -1;
    }
    if (!fc->root->d_sb->s_bdev) {
        pr_debug(pr_format("block device is NULL"));
        return -1;
    }
    *bdev = fc->root->d_sb->s_bdev;
    return 0;
}

int get_tree_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct fs_context *fc = get_arg1(struct fs_context*, regs);
    if (!fc->source) {
        pr_debug(pr_format("source is null"));
        return -1;
    }
    struct block_device *bdev;
    if (!get_bdev_safe(fc, &bdev)) {
        update_session(fc->source, bdev);
        pr_debug(pr_format("session for device %s has been created successfully"), fc->source);
    }
    struct get_tree_data *data = (struct get_tree_data*)kp->data;
    data->fc = fc;
    return 0;
}

int get_tree_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    int err = get_rval(int, regs);
    if (err) {
        struct get_tree_data *data = (struct get_tree_data*)kp->data;
        struct fs_context *fc = data->fc;
        struct block_device *bdev;
        if (!get_bdev_safe(fc, &bdev)) {
            registry_end_session(bdev->bd_dev);
            pr_debug(pr_format("session for device %s has been destroyed, got error %d"), fc->source, err);
        }
        return 0;
    }
    return 0;
}