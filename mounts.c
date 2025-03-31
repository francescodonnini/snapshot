#include "include/mounts.h"
#include "include/pr_format.h"
#include <linux/fs.h>
#include <linux/kernel_read_file.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kstrtox.h>

#define BUF_SIZE 4096L

struct mnts_info {
    const char *mnt_dev;
    const char *mnt_point;
    const char *fs_type;
    const char *mnt_opts;
    long       dump_freq;
    long       fsck_order;
};

static ssize_t fileread(struct file *fp, char *bufp, ssize_t n) {
    return fp->f_op->read(fp, bufp, n, &fp->f_pos);
}

static char* getline(char *bufp, ssize_t n, struct file *fp) {
    if (n <= 0) {
        return NULL;
    }
    n--;
    ssize_t br = fileread(fp, bufp, n);
    if (br > 0) {
        char *endp = strchr(bufp, 0);
        if (endp) {
            *endp = 0;
            return bufp;
        } else {
            bufp[br] = 0;
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

int list_mounts(void) {
    char *bufp = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (!bufp) {
        return 0;
    }    
    struct file *fp = filp_open("/proc/mounts", O_RDONLY, 0);
    if (IS_ERR(fp)) {
        pr_debug(pr_format("cannot open file %s\n"), "/proc/mounts");
        return 0;
    }
    while (!getline(bufp, BUF_SIZE, fp)) {
        struct mnts_info mi;
        if (parse_mnts_info(bufp, &mi)) {
            return -1;
        }
        pr_debug(pr_format("%s %s %s\n"), mi.mnt_dev, mi.mnt_point, mi.fs_type);
    }
    return 0;
}