#include "include/kprobe_handlers.h"
#include "include/pr_format.h"

int mount_bdev_pre_handler(struct kprobe *kp, struct pt_regs *regs) {
    pr_debug(pr_format("mount_bdev() called\n"));
    return -1;
}