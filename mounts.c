#include "include/mounts.h"
#include "include/pr_format.h"
#include <linux/fs.h>
#include <linux/kstrtox.h>
#include <linux/printk.h>
#include <linux/sprintf.h>
#include <linux/string.h>
#include <linux/slab.h>

struct mnts_info {
    const char *mnt_dev;
    const char *mnt_point;
    const char *fs_type;
    const char *mnt_opts;
    long       dump_freq;
    long       fsck_order;
};

struct vfsmount *mnt;

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

static int parse_mnts_info(char *line, struct mnts_info *mi) {
    line = strim(line);
    const char *sep = " \t";
    mi->mnt_dev = strsep(&line, sep);
    if (!mi->mnt_dev) {
        pr_debug(pr_format("cannot parse line %s\n"), line);
        return -1;
    }
    mi->mnt_point = strsep(&line, sep);
    if (!mi->mnt_point) {
        pr_debug(pr_format("cannot parse line %s\n"), line);
        return -1;
    }
    mi->fs_type = strsep(&line, sep);
    if (!mi->fs_type) {
        pr_debug(pr_format("cannot parse line %s\n"), line);
        return -1;
    }
    mi->mnt_opts = strsep(&line, sep);
    if (!mi->mnt_opts) {
        pr_debug(pr_format("cannot parse line %s\n"), line);
        return -1;
    }
    const char *s_dump_freq = strsep(&line, sep);
    int err = kstrtol(s_dump_freq, 10, &mi->dump_freq);
    if (err) {
        pr_debug(pr_format("cannot convert %s to long\n"), s_dump_freq);
        return err;
    }
    const char *s_fsck_order = strsep(&line, sep);
    err = kstrtol(s_fsck_order, 10, &mi->fsck_order);
    if (err) {
        pr_debug(pr_format("cannot convert %s to long\n"), s_fsck_order);
        return err;
    }
    return 0;
}

/**
 * getline - read a string until either new line '\n' or the NULL '\0' are met. It updates rp
 * to the position of the character following '\n' or the end of the string.
 * @param rp - the position to start the reading. If '\n' is met, rp gets updated to the position
 * following it, if no '\n' is met, then rp is updated to the position of the NULL character.
 * @returns the old value of rp (before the update) if '\n' is met, NULL otherwise.
 */
static char *getline(char **rp) {
    char *cp = *rp;
    if (!cp) {
        return NULL;
    }
    char *t = strchr(cp, '\n');
    if (t == NULL) {
        return ERR_PTR(-1); // EOF met too early
    }
    *t = 0;
    // '\n' met so there is at least one character to read ('\0')
    *rp = t+1;
    return cp;
}

static int get_file_size(struct file *fp, size_t *size) {
    loff_t off = vfs_llseek(fp, 0, SEEK_END);
    if (off < 0) {
        return off;
    }
    *size = off;
    off = vfs_llseek(fp, 0, SEEK_SET);
    if (off < 0) {
        return off;
    }
    return 0;
}

/**
 * read_mounts_file - returns a buffer containing the content of /proc/mounts.
 * @returns NULL if the file is empty, a non-NULL pointer otherwise. If the pointer is not NULL
 * then it is necessary to check with IS_ERR(). The pointer embeds an error code if it was not possible
 * to allocate enough memory to contain the entire file or if some error occurs while reading the file.
 */
static char* read_mounts_file() {
    struct file *fp = file_open_root_mnt(mnt, "mounts", O_RDONLY, 0);
    if (IS_ERR(fp)) {
        pr_debug(pr_format("cannot open /proc/mounts\n"));
        return PTR_ERR(fp);
    }
    size_t buf_size;
    int err = get_file_size(fp, &buf_size);
    if (err) {
        pr_debug(pr_format("cannot get size of file /proc/mounts because of error %d\n"), err);
        goto close_file;
    }
    if (!buf_size) {
        pr_debug(pr_format("file /proc/mounts is empty\n"));
        return NULL;
    }
    // buf_size gets incremented because the buffer should contain the NUL-character
    char *bufp = kmalloc(++buf_size, GFP_KERNEL);
    if (!bufp) {
        pr_debug(pr_format("kmalloc failed\n"));
        return ERR_PTR(-ENOMEM);
    }
    ssize_t n = kernel_read(fp, bufp, buf_size, &fp->f_pos);
    if (n <= 0) {
        pr_debug(pr_format("cannot read file /proc/mounts because of error %ld\n"), n);
        err = n;
        goto close_file;
    } else if (n < buf_size - 1) {
        pr_debug(pr_format("cannot read entire file /proc/mounts\n"));
        err = -1;
        goto close_file;
    }
    bufp[n] = 0;
    return bufp;
close_file:
    kfree(bufp);
    int err2 = filp_close(fp, NULL);
    if (err2) {
        pr_debug(pr_format("cannot close file /proc/mounts"));
        return ERR_PTR(err2);
    }
    return ERR_PTR(err);
}

/**
 * find_mount - look if a device is already mounted at dev_name path
 * @param dev_name
 * @param found
 * @returns 0 if the search was successfull - i.e. the function was able to find the device or to look for all
 * of them. < 0 is some error occurred.
 */
int find_mount(const char *dev_name, bool *found) {
    *found = false;
    char *bufp = read_mounts_file();
    if (!bufp) {
        return 0;
    } else if (IS_ERR(bufp)) {
        return PTR_ERR(bufp);
    }
    int err;
    char *line;
    char *rp = bufp;
    while ((line = getline(&rp)) != NULL && !IS_ERR(line)) {
        struct mnts_info mi;
        err = parse_mnts_info(bufp, &mi);
        if (err) {
            pr_debug(pr_format("cannot parse %s got error %d\n"), line, err);
            break;
        }
        if (strstr(mi.mnt_dev, "loop") && !strcmp(mi.mnt_point, dev_name)) {
            *found = true;
            break;
        }
        if (!strcmp(mi.mnt_dev, dev_name)) {
            *found = true;
            break;
        }
    }
    if (IS_ERR(line)) {
        pr_debug(pr_format("cannot complete parsing because of error %ld\n"), PTR_ERR(line));
        err = PTR_ERR(line);
    }
    kfree(bufp);
    return err;
}