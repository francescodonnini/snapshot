#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "registry.h"
#include "update_session.h"
#include <linux/printk.h>

static inline bool fs_bdev_safe(struct fs_context *fc, struct block_device **bdev) {
    if (!fc) {
        pr_debug(pr_format("fs_context is NULL"));
        return false;
    }
    if (!fc->root) {
        pr_debug(pr_format("root is NULL"));
        return false;
    }
    if (!fc->root->d_sb) {
        pr_debug(pr_format("super block is NULL"));
        return false;
    }
    if (!fc->root->d_sb->s_bdev) {
        pr_debug(pr_format("block device is NULL"));
        return false;
    }
    *bdev = fc->root->d_sb->s_bdev;
    return true;
}

int get_tree_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct fs_context *fc = get_arg1(struct fs_context*, regs);
    if (!fc->source) {
        pr_debug(pr_format("source is null"));
        return -1;
    }
    struct block_device *bdev;
    if (!fs_bdev_safe(fc, &bdev)) {
        return -1;
    }
    struct fs_context **data = (struct fs_context**)kp->data;
    *data = fc;
    return 0;
}

int get_tree_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    int err = get_rval(int, regs);
    if (err) {
        struct fs_context **data = (struct fs_context**)kp->data;
        struct fs_context *fc = (struct fs_context*)*data;
        pr_debug(pr_format("cannot mount device '%s', got error %d"), fc->source, err);
    }
    return 0;
}