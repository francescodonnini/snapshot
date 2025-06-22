#include "include/kretprobe_handlers.h"
#include "include/pr_format.h"
#include <linux/blk_types.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/mount.h>

static inline struct file* get_file_pointer(struct pt_regs *regs) {
// x86_64 cc: rdi, rsi, rdx, rcx, r8, r9, stack
#ifdef CONFIG_X86_64
    return (struct file*) regs->di;
// x86 cc:    eax, edx, ecx, stack
#elif CONFIG_X86
    return (struct file*) regs->ax;
#else
#error "unsupported architecture"
#endif
}

int vfs_write_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct file *fp = get_file_pointer(regs);
    struct vfsmount *mnt = fp->f_path.mnt;
    struct super_block *sb = mnt->mnt_sb;
    pr_debug(pr_format("vfs_write called on device %s (%s)\n"), sb->s_type->name, sb->s_root->d_name.name);
    pr_debug(pr_format("vfs_write called"));
    return 0;
}

int vfs_write_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    return 0;
}
