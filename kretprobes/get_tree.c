#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "update_session.h"
#include <linux/printk.h>

int get_tree_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct fs_context *fc = get_arg1(struct fs_context*, regs);
    if (!fc->source) {
        pr_debug(pr_format("source is null"));
        return -1;
    }
    struct get_tree_data *data = (struct get_tree_data*)kp->data;
    data->fc = fc;
    pr_debug(pr_format("fs-type=%s,source=%s"), fc->fs_type->name, fc->source);
    return 0;
}

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

static inline void __update_session(struct fs_context *fc) {
    struct block_device *bdev;
    if (get_bdev_safe(fc, &bdev)) {
        return;
    }
    update_session(fc->source, bdev);
}

int get_tree_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    int err = get_rval(int, regs);
    if (err) {
        pr_debug(pr_format("completed with error %d"), err);
        return 0;
    }
    struct get_tree_data *data = (struct get_tree_data*)kp->data;
    struct fs_context *fc = data->fc;
    __update_session(fc);
    return 0;
}