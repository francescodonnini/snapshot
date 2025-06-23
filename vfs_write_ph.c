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

static inline void log_if_match(const char *m_path) {
    if (registry_lookup(m_path)) {
        pr_debug(pr_format("vsf_write called on %s\n"), m_path);
    }
} 

int vfs_write_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct file *fp = get_file_pointer(regs);
    char buffer[256];
    char *m_path = get_mountpoint(fp, buffer, 256);
    log_at_most(m_path);
    return 0;
}

int vfs_write_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    return 0;
}
