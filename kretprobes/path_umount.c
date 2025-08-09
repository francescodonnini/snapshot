#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "registry_lookup.h"
#include <linux/blkdev.h>
#include <linux/path.h>
#include <linux/printk.h>

int path_umount_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct path *path = get_arg1(struct path*, regs);
    dev_t dev = path->mnt->mnt_sb->s_bdev->bd_dev;
    if (registry_lookup_mm(dev)) {
        return -1;
    }
    struct umount_data *data = (struct umount_data*)kp->data;
    data->dev = dev;
    return 0;
}

int path_umount_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    if (!get_rval(int, regs)) {
        return -1;
    }
    struct umount_data *data = (struct umount_data*)kp->data;
    pr_debug(pr_format("device (%d, %d) successfully unmounted!"), MAJOR(data->dev), MINOR(data->dev));
    return 0;
}