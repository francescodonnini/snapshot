#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "registry.h"
#include "update_session.h"
#include <linux/fs.h>
#include <linux/printk.h>

static bool fs_supported(struct super_block *sb) {
    if (!sb || !sb->s_type) {
        pr_err("super_block or super_block->s_type is NULL");
        return false;
    }
    if (strcmp(sb->s_type->name, "ext4") && strcmp(sb->s_type->name, "singlefilefs")) {
        pr_debug(pr_format("filesystem %s is not supported"), sb->s_type->name);
        return false;
    }
    return true;
}

static inline struct file* s_bdev_file(struct super_block *sb) {
    if (!sb) {
        pr_err("cannot get block device file from super_block");
        return NULL;
    }
    return sb->s_bdev_file;
}

int ext4_fill_super_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct super_block *sb = get_arg1(struct super_block*, regs);
    struct file *bd_file = s_bdev_file(sb);
    if (!bd_file) {
        return -1;
    }
    int err = ext4_update_session(bd_file);
    struct file **data = (struct file**)kp->data;
    *data = bd_file;
    return err;
}

int ext4_fill_super_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    int err = get_rval(int, regs);
    // The session has to be destroyed if the mount operation fails
    if (err) {
        struct file **data = (struct file**)kp->data;
        struct file *bd_file = *data;
        dev_t dev = bd_file->f_inode->i_rdev;
        registry_session_destroy(dev);
        pr_err("cannot fill super block of device %d:%d, got error %d", MAJOR(dev), MINOR(dev), err);
    }
    return 0;
}

int singlefilefs_fill_super_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct super_block *sb = get_arg1(struct super_block*, regs);
    struct file *bd_file = s_bdev_file(sb);
    if (!bd_file) {
        return -1;
    }
    struct file **data = (struct file**)kp->data;
    *data = bd_file;
    return 0;
}

int singlefilefs_fill_super_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    int err = get_rval(int, regs);
    if (err) {
        return 0;
    }
    struct file **data = (struct file**)kp->data;
    struct file *bd_file = *data;
    err = singlefilefs_update_session(bd_file);
    if (err) {
        dev_t dev = bd_file->f_inode->i_rdev;
        pr_err("cannot update session for device %d:%d, got error %d", MAJOR(dev), MINOR(dev), err);
    }
    return 0;
}

int kill_block_super_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct super_block *sb = (struct super_block *)get_arg1(void*, regs);
    if (!fs_supported(sb)) {
        return -1;
    }
    dev_t *data = (dev_t *)kp->data;
    *data = sb->s_bdev->bd_dev;
    return 0;    
}

int kill_block_super_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    dev_t *data = (dev_t*)kp->data;
    dev_t dev = *data;
    registry_session_put(dev);
    return 0;
}