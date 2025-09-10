#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "registry.h"
#include <linux/blk_types.h>
#include <linux/fs_context.h>
#include <linux/printk.h>

int get_tree_bdev_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct fs_context *fc = get_arg1(struct fs_context*, regs);
    // ext4 is the only fs supported that uses this callback
    if (strcmp(fc->fs_type->name, "ext4")) {
        pr_debug(pr_format("filesystem %s is not supported"), fc->fs_type->name);
        return -1;
    }
    struct fs_context **data = (struct fs_context**)kp->data;
    *data = fc;
    return 0;
}

int get_tree_bdev_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    int err = get_rval(int, regs);
    struct fs_context **data = (struct fs_context**)kp->data;
    struct fs_context *fc = *data;
    if (!err) {
        registry_session_get(fc->source, fc->root->d_sb->s_bdev->bd_dev);
    }
    return 0;
}