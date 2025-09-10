#include "kretprobe_handlers.h"
#include "registry.h"
#include <linux/path.h>

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