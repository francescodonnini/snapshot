#include "kretprobe_handlers.h"
#include "registry.h"
#include "update_session.h"
#include <linux/blk_types.h>
#include <linux/fs_context.h>

static inline bool s_get_bdev_safe(struct super_block *sb, struct block_device **bdev) {
    if (!sb || !sb->s_bdev) {
        pr_debug(pr_format("cannot read struct block_device* from super_block"));
        return false;
    }
    *bdev = sb->s_bdev;
    return true;
}

int ext4_fill_super_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct super_block *sb = get_arg1(struct super_block*, regs);
    struct fs_context *fc = get_arg2(struct fs_context*, regs);
    struct block_device *bdev;
    if (!s_get_bdev_safe(sb, &bdev)) {
        return -1;
    }
    if (update_session(fc->source, bdev)) {
        return -1;
    }
    pr_debug(pr_format("session for device '%s' %d,%d has been created successfully"), fc->source, MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev));
    dev_t *data = (dev_t*)kp->data;
    *data = bdev->bd_dev;
    return 0;
}

int ext4_fill_super_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    int err = get_rval(int, regs);
    dev_t *dev = (dev_t*)kp->data;
    // The session has to be destroyed if the mount operation fails
    if (err) {
        registry_end_session(*dev);
        pr_debug(pr_format("cannot fill super block of device %d,%d, got error %d"), MAJOR(*dev), MINOR(*dev), err);
    }
    return 0;
}