#ifndef KPROBE_HANDLERS
#define KPROBE_HANDLERS
#include <linux/kprobes.h>

int vfs_write_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs);

int vfs_write_handler(struct kretprobe_instance *kp, struct pt_regs *regs);

int mount_bdev_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs);

int mount_bdev_handler(struct kretprobe_instance *kp, struct pt_regs *regs);

#endif