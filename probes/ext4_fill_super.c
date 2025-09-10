#include "kretprobe_handlers.h"
#include "registry.h"
#include "update_session.h"
#include <linux/fs.h>
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
        pr_err("cannot fill super block of device %d:%d, got error %d", MAJOR(*dev), MINOR(*dev), err);
    }
    return 0;
}
