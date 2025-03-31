#include "include/mounts.h"
#include "include/pr_format.h"
#include <linux/fs.h>
#include <linux/kstrtox.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/slab.h>

#define BUF_SIZE 4096L

struct mnts_info {
    const char *mnt_dev;
    const char *mnt_point;
    const char *fs_type;
    const char *mnt_opts;
    long       dump_freq;
    long       fsck_order;
};


static int parse_mnts_info(char *bufp, struct mnts_info *mi) {
    bufp = strim(bufp);
    const char *sep = " \t";
    char *s = strsep(&bufp, sep);
    if (!s) {
        pr_debug(pr_format("cannot parse line %s\n"), bufp);
        return -1;
    }
    mi->mnt_dev = strsep(&bufp, sep);
    if (!mi->mnt_dev) {
        pr_debug(pr_format("cannot parse line %s\n"), bufp);
        return -1;
    }
    mi->mnt_point = strsep(&bufp, sep);
    if (!mi->mnt_point) {
        pr_debug(pr_format("cannot parse line %s\n"), bufp);
        return -1;
    }
    mi->fs_type = strsep(&bufp, sep);
    if (!mi->fs_type) {
        pr_debug(pr_format("cannot parse line %s\n"), bufp);
        return -1;
    }
    mi->mnt_opts = strsep(&bufp, sep);
    if (!mi->mnt_opts) {
        pr_debug(pr_format("cannot parse line %s\n"), bufp);
        return -1;
    }
    const char *s_dump_freq = strsep(&bufp, sep);
    int err = kstrtol(s_dump_freq, 10, &mi->dump_freq);
    if (err) {
        pr_debug(pr_format("cannot convert %s to long\n"), s_dump_freq);
        return err;
    }
    const char *s_fsck_order = strsep(&bufp, sep);
    err = kstrtol(s_fsck_order, 10, &mi->fsck_order);
    if (err) {
        pr_debug(pr_format("cannot convert %s to long\n"), s_fsck_order);
        return err;
    }
    return 0;
}

int init_procfs() {
    struct file_system_type *fs_type = get_fs_type("proc");
    if (!fs_type) {
        pr_debug(pr_format("cannot find procfs\n"));
        return -1;
    }
    struct vfsmount *mnt = kern_mount(fs_type);
    if (IS_ERR(mnt)) {
        pr_debug(pr_format("cannot mount procfs\n"));
        return -1;
    }
    struct file *fp = file_open_root_mnt(mnt, "mounts", O_RDONLY, 0);
    if (IS_ERR(fp)) {
        pr_debug(pr_format("cannot open /mounts\n"));
        return PTR_ERR(fp);
    }
    int err = filp_close(fp, NULL);
    if (err) {
        pr_debug(pr_format("cannot close file mounts"));
        return err;
    }
    char *bufp = kmalloc(4096L, GFP_KERNEL);
    if (!bufp) {
        pr_debug(pr_format("kmalloc failed\n"));
        return -ENOMEM;
    }
    ssize_t br = kernel_read(fp, bufp, 4096L, &fp->f_pos);
    if (br) {
        pr_debug(pr_format("read file %s\n"), bufp);
    }
    return 0;
}