#include "include/kprobe_handlers.h"
#include "include/pr_format.h"
#include <linux/blk_types.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/mount.h>

static inline int get_file_pointer(struct pt_regs *regs, struct file **fp) {
// x86_64 cc: rdi, rsi, rdx, rcx, r8, r9, stack
#ifdef CONFIG_X86_64
    *fp = (struct file*) regs->di;
    return 0;
// x86 cc:    eax, edx, ecx, stack
#elif CONFIG_X86
    fp = (struct file*) regs->ax;
    return 0;
#else
    pr_debug(pr_format("unsupported architecture"));
    return -1;
#endif
}

int vfs_write_pre_handler(struct kprobe *kp, struct pt_regs *regs) {
    struct file *fp;
    if (get_file_pointer(regs, &fp)) {
        return 0;
    }
    struct vfsmount *mnt = fp->f_path.mnt;
    struct super_block *sb = mnt->mnt_sb;
    pr_debug(pr_format("vfs_write called on device %s (%s)\n"), sb->s_type->name, sb->s_root->d_name.name);
    pr_debug(pr_format("vfs_write called"));
    return 0;
}
