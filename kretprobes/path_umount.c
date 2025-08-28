#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "registry.h"
#include <linux/blkdev.h>
#include <linux/path.h>
#include <linux/printk.h>

static inline bool p_dev_safe(struct path *path, dev_t *dev) {
    if (!path || !path->mnt || !path->mnt->mnt_sb || !path->mnt->mnt_sb->s_bdev) {
        return false;
    }
    *dev = path->mnt->mnt_sb->s_bdev->bd_dev;
    return true;
}

int path_umount_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    dev_t dev;
    if (!p_dev_safe(get_arg1(struct path*, regs), &dev)) {
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