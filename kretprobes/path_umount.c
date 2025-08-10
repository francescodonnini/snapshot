#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "registry_lookup.h"
#include <linux/blkdev.h>
#include <linux/path.h>
#include <linux/printk.h>

static inline int p_dev_safe(struct path *path, dev_t *dev) {
    if (!path || !path->mnt || !path->mnt->mnt_sb || !path->mnt->mnt_sb->s_bdev) {
        return -1;
    }
    *dev = path->mnt->mnt_sb->s_bdev->bd_dev;
    return 0;
}

int path_umount_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct path *path = get_arg1(struct path*, regs);
    dev_t dev;
    int err = p_dev_safe(path, &dev);
    if (err || !registry_lookup_active(dev)) {
        return -1;
    }
    struct umount_data *data = (struct umount_data*)kp->data;
    data->dev = dev;
    return 0;
}

int path_umount_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    if (get_rval(int, regs)) {
        return 0;
    }
    struct umount_data *data = (struct umount_data*)kp->data;
    registry_end_session(data->dev);
    return 0;
}