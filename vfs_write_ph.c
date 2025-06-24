#include "include/kretprobe_handlers.h"
#include "include/pr_format.h"
#include "include/registry_lookup.h"
#include <linux/blk_types.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/string.h>

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

static char *get_mountpoint(struct file *file, char *buf, int buflen) {
    struct vfsmount *mnt = file->f_path.mnt;
    struct path mnt_path = {
        .mnt = mnt,
        .dentry = mnt->mnt_root,
    };
    return d_path(&mnt_path, buf, buflen);
}

static inline void log_if_match(struct file *fp, const char *s) {
    char buf[256];
    char *mntpoint = get_mountpoint(fp, buf, 256);
    struct vfsmount *mnt = fp->f_path.mnt;
    struct super_block *sb = mnt->mnt_sb;
    const char *m_path = fp->f_path.dentry->d_name.name;
    if (strstr(m_path, s)) {
        pr_debug(pr_format("vsf_write called on (%s:%s) %s\n"), sb->s_type->name, mntpoint, m_path);
    }
} 

int vfs_write_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct file *fp = get_file_pointer(regs);
    log_if_match(fp, "the-file");
    return 0;
}

int vfs_write_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    return 0;
}
