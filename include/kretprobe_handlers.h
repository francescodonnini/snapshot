#ifndef KPROBE_HANDLERS
#define KPROBE_HANDLERS
#include <linux/fs_context.h>
#include <linux/kprobes.h>

static inline long __get_arg1(struct pt_regs *regs) {
#ifdef CONFIG_X86_64
    return regs->di;
#else
#error "unsupported architecture"
#endif
}

static inline long __get_arg2(struct pt_regs *regs) {
#ifdef CONFIG_X86_64
    return regs->si;
#else
#error "unsupported architecture"
#endif
}

static inline long __get_arg3(struct pt_regs *regs) {
#ifdef CONFIG_X86_64
    return regs->dx;
#else
#error "unsupported architecture"
#endif
}

static inline long __get_arg4(struct pt_regs *regs) {
#ifdef CONFIG_X86_64
    return regs->cx;
#else
#error "unsupported architecture"
#endif
}

static inline long __get_arg5(struct pt_regs *regs) {
#ifdef CONFIG_X86_64
    return regs->r8;
#else
#error "unsupported architecture"
#endif
}

static inline long __get_arg6(struct pt_regs *regs) {
#ifdef CONFIG_X86_64
    return regs->r9;
#else
#error "unsupported architecture"
#endif
}

#define get_arg(_n, _type, _regs) ((_type)__get_arg##_n(_regs))

#define get_arg1(_type, _regs) get_arg(1, _type, _regs)

#define get_arg2(_type, _regs) get_arg(2, _type, _regs)

#define get_arg3(_type, _regs) get_arg(3, _type, _regs)

#define get_arg4(_type, _regs) get_arg(4, _type, _regs)

#define get_arg5(_type, _regs) get_arg(5, _type, _regs)

#define get_arg6(_type, _regs) get_arg(6, _type, _regs)

static inline long __get_rval(struct pt_regs *regs) {
#ifdef CONFIG_X86_64
    return regs->ax;
#else
#error "unsupported architecture"
#endif
}

#define get_rval(_type, _regs) ((_type)__get_rval(_regs))

int ext4_fill_super_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs);

int ext4_fill_super_handler(struct kretprobe_instance *kp, struct pt_regs *regs);

int kill_block_super_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs);

int kill_block_super_handler(struct kretprobe_instance *kp, struct pt_regs *regs);

int mount_bdev_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs);

int mount_bdev_handler(struct kretprobe_instance *kp, struct pt_regs *regs);

int singlefilefs_fill_super_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs);

int singlefilefs_fill_super_handler(struct kretprobe_instance *kp, struct pt_regs *regs);

int submit_bio_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs);

#endif