#ifndef KPROBE_HANDLERS
#define KPROBE_HANDLERS
#include <linux/kprobes.h>

int vfs_write_pre_handler(struct kprobe *kp, struct pt_regs *regs);

int mount_bdev_pre_handler(struct kprobe *kp, struct pt_regs *regs);

#endif