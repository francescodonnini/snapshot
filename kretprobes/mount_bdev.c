#include "kretprobe_handlers.h"
#include "pr_format.h"
#include <linux/dcache.h>
#include <linux/fs.h>

static inline struct file_system_type* get_filesystem_type(struct pt_regs *regs) {
// x86_64 cc: rdi, rsi, rdx, rcx, r8, r9, stack
#ifdef CONFIG_X86_64
    return (struct file_system_type*) regs->di;
// x86 cc:    eax, edx, ecx, stack
#elif CONFIG_X86
    return (struct file_system_type*) regs->ax;
#else
#error "unsupported architecture"
#endif
}

static inline const char* get_dev_name(struct pt_regs *regs) {
// x86_64 cc: rdi, rsi, rdx, rcx, r8, r9, stack
#ifdef CONFIG_X86_64
    return (const char*) regs->si;
// x86 cc:    eax, edx, ecx, stack
#elif CONFIG_X86
    return (const char*) regs->ax;
#else
#error "unsupported architecture"
#endif
}

int mount_bdev_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct file_system_type *fs_type = get_filesystem_type(regs);
    pr_debug(pr_format("mount_bdev(%s, %s) called\n"), fs_type->name, get_dev_name(regs));
    return 0;
}

int mount_bdev_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    return 0;
}