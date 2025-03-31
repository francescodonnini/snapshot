#include "include/mounts.h"
#include "include/pr_format.h"
#include <linux/fs.h>
#include <linux/kstrtox.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/types.h>

#define BUF_SIZE 4096L

struct mnts_info {
    const char *mnt_dev;
    const char *mnt_point;
    const char *fs_type;
    const char *mnt_opts;
    long       dump_freq;
    long       fsck_order;
};

struct vfsmount *mnt;

static char *getline(char *bufp, ssize_t n, struct file *fp) {
    --n;
    if (n <= 0) {
        return NULL;
    }
    ssize_t br = kernel_read(fp, bufp, n, &fp->f_pos);
    if (br) {
        char *t = strchr(bufp, '\n');
        if (t == NULL) {
            return ERR_PTR(-1);
        }
        *t = 0;
        loff_t line_len = t - bufp;
        if (br > line_len) {
            int err = vfs_llseek(fp, line_len - br, SEEK_CUR);
            if (err < 0) {
                return ERR_PTR(err);
            }
        }
        return bufp;
    } else if (!br) {
        return NULL;
    } else {
        return ERR_PTR(br);
    }
}

static int parse_mnts_info(char *bufp, struct mnts_info *mi) {
    bufp = strim(bufp);
    pr_debug(pr_format("start parsing %s\n"), bufp);
    const char *sep = " \t";
    mi->mnt_dev = strsep(&bufp, sep);
    pr_debug(pr_format("token=%s\n"), mi->mnt_dev);
    if (!mi->mnt_dev) {
        pr_debug(pr_format("cannot parse line %s\n"), bufp);
        return -1;
    }
    mi->mnt_point = strsep(&bufp, sep);
    pr_debug(pr_format("token=%s\n"), mi->mnt_point);
    if (!mi->mnt_point) {
        pr_debug(pr_format("cannot parse line %s\n"), bufp);
        return -1;
    }
    mi->fs_type = strsep(&bufp, sep);
    pr_debug(pr_format("token=%s\n"), mi->fs_type);
    if (!mi->fs_type) {
        pr_debug(pr_format("cannot parse line %s\n"), bufp);
        return -1;
    }
    mi->mnt_opts = strsep(&bufp, sep);
    pr_debug(pr_format("token=%s\n"), mi->mnt_opts);
    if (!mi->mnt_opts) {
        pr_debug(pr_format("cannot parse line %s\n"), bufp);
        return -1;
    }
    const char *s_dump_freq = strsep(&bufp, sep);
    pr_debug(pr_format("token=%s\n"), s_dump_freq);
    int err = kstrtol(s_dump_freq, 10, &mi->dump_freq);
    if (err) {
        pr_debug(pr_format("cannot convert %s to long\n"), s_dump_freq);
        return err;
    }
    const char *s_fsck_order = strsep(&bufp, sep);
    pr_debug(pr_format("token=%s\n"), s_fsck_order);
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
    mnt = kern_mount(fs_type);
    if (IS_ERR(mnt)) {
        pr_debug(pr_format("cannot mount procfs\n"));
        return PTR_ERR(mnt);
    }
    return 0;
}

int find_mount(const char *dev_name) {
    struct file *fp = file_open_root_mnt(mnt, "mounts", O_RDONLY, 0);
    if (IS_ERR(fp)) {
        pr_debug(pr_format("cannot open /mounts\n"));
        return PTR_ERR(fp);
    }
    char *bufp = kmalloc(4096L, GFP_KERNEL);
    if (!bufp) {
        pr_debug(pr_format("kmalloc failed\n"));
        goto kmalloc_error;
    }
    int escape = 0;
    char *line;
    while ((line = getline(bufp, 4096L, fp)) != NULL && !IS_ERR(line)) {
        if (++escape > 256) {
            break;
        }
        struct mnts_info mi;
        int err = parse_mnts_info(bufp, &mi);
        if (err) {
            pr_debug(pr_format("cannot parse %s got error %d\n"), bufp, err);
        } else {
            pr_debug(pr_format("%s %s %s\n"), mi.mnt_dev, mi.fs_type, mi.mnt_point);
        }
    }
    if (IS_ERR(line)) {
        pr_debug(pr_format("cannot complete parsing because of error %ld\n"), PTR_ERR(line));
    }
    pr_debug(pr_format("escape=%d\n"), escape);
    return 0;
kmalloc_error:
    int err = filp_close(fp, NULL);
    if (err) {
        pr_debug(pr_format("cannot close file mounts"));
        return err;
    }
    return -ENOMEM;
}