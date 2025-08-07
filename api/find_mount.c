#include "find_mount.h"
#include "pr_format.h"
#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/kstrtox.h>
#include <linux/printk.h>
#include <linux/sprintf.h>
#include <linux/string.h>
#include <linux/slab.h>

#define BUFIO_EOF  -1
#define LINE_BUFSZ 512
#define FILE_BUFSZ 1536

/**
 * Facility to hold the fields of a row read from /proc/mounts file
 */
struct mnts_info {
    const char *mnt_dev;
    const char *mnt_point;
    const char *fs_type;
    const char *mnt_opts;
    long        dump_freq;
    long        fsck_order;
};

/**
 * Facility to read the content inside a file in a buffered-way.
 * - fp       -- the pointer to the file to read from
 * - bufp     -- the internal buffer used to store chunks of the file
 * - rp       -- the read position inside the internal buffer (the next character to be read)
 * - size     -- the amount of valid bytes inside the internal buffer
 * - capacity -- the real size of the buffer, no more than this characters could be read from the file
 */
struct bufio {
    struct file *fp;
    char        *bufp;
    size_t       size;
    size_t       capacity;
    size_t       rp;
};

/**
 * Facility to iterate through the lines of /proc/mount. On each iteration, the line read is stored in linep.
 */
struct iter {
    char          *bufp;
    char          *linep;
    size_t         linesz;
    struct bufio   ff;
};

struct vfsmount *procfs_mnt;

int procfs_init(void) {
    struct file_system_type *fs_type = get_fs_type("proc");
    if (!fs_type) {
        pr_debug(pr_format("cannot find procfs\n"));
        return -1;
    }
    procfs_mnt = kern_mount(fs_type);
    if (IS_ERR(procfs_mnt)) {
        pr_debug(pr_format("cannot mount procfs\n"));
        return PTR_ERR(procfs_mnt);
    }
    return 0;
}

void procfs_cleanup(void) {
    kern_unmount(procfs_mnt);
}

/**
 * bufio_refill - refills the buffer reading new bytes from the file.
 * @param ff facility to read the bytes of a file
 * Returns:
 * 
 * - > 0 if some bytes are successfully read from the file;
 * 
 * - 0 if no bytes remaining;
 * 
 * - < 0 if some error occurred while reading from the file.
 */
static ssize_t bufio_refill(struct bufio *ff) {
    struct file *fp = ff->fp;
    ssize_t bytes_read = kernel_read(fp, ff->bufp, ff->capacity, &fp->f_pos);
    if (bytes_read < 0) {
        pr_debug(pr_format("refill() failed\n"));
        return bytes_read;
    }
    ff->rp = 0;
    ff->size = bytes_read;
    return bytes_read;
}

/**
 * bufio_getc - reads a character from the file. A good chunk of the file
 * is pre-saved in a buffer to reduce the number of times kernel_read gets called.
 * @param ff facility to read the bytes of a file
 * Returns the character read if successfull, a negative value otherwise.
 */
static int bufio_getc(struct bufio *ff) {
    if (ff->rp >= ff->size) {
        ssize_t err = bufio_refill(ff);
        if (err < 0) {
            return err;
        } else if (err == 0) {
            // err == 0 means there are no bytes left to read from the file
            return BUFIO_EOF;
        }
    }
    return ff->bufp[ff->rp++];
}

/**
 * bufio_getline - reads up to n bytes or until '\n' is met.
 * @param dst buffer where characters read from ff are copied to
 * @param n   max number of bytes expected in a line
 * @param ff  facility to read bytes from a file in a buffered way
 * Returns @param dst if an entire line has been read, otherwise NULL, which means
 * either n bytes are read and no '\n' was met or all the file was read and no '\n' was met.
 */
static char *bufio_getline(char *dst, size_t n, struct bufio *ff) {
    if (n <= 0) {
        return NULL;
    }
    --n; // make space for '\0' character
    char *linp = dst;
    while (n-- > 0) {
        int c = bufio_getc(ff);
        if (c <= 0) {
            // '\0' character is really unexpected in an ASCII file
            if (c == BUFIO_EOF || !c) {
                return NULL;
            }
            return ERR_PTR(c);
        } else if (c == '\n') {
            *linp = 0;
            return dst;
        } else {
            *linp++ = c;
        }
    }
    return NULL;
}

/**
 * parse_mnts_info - reads a line into a mnts_info record.
 * @param line line of text read from /proc/mounts
 * @param mi   record to hold the fields of a line of the file /proc/mounts
 * A line is a string of six words separated by by blanks (' ' or '\t'). 
 * Returns 0 if the parsing completed successfully, < 0 if some error occurred. Specifically:
 * -1 if some field is missing or one of -ERANGE or -EINVAL if the last two numeric fields are malformed -i.e.
 * kstrtol() failed.
 */
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

static void init_bufio(struct bufio *ff, char *buf, size_t bufsz, struct file *fp) {
    ff->bufp = buf;
    ff->capacity = bufsz;
    ff->rp = 0;
    ff->size = 0;
    ff->fp = fp;
}

static int __iter_begin(struct iter *it, size_t linesz, size_t file_bufsz, struct file *fp) {
    // common memory pool to buffer both file content and the line read
    char *bufp = kmalloc(LINE_BUFSZ + FILE_BUFSZ, GFP_KERNEL);
    if (!bufp) {
        pr_debug(pr_format("cannot make enough space to parse /proc/mounts file\n"));
        return -ENOMEM;
    }
    it->bufp = bufp;
    it->linep = &bufp[file_bufsz];
    it->linesz = linesz;
    init_bufio(&it->ff, it->bufp, file_bufsz, fp);
    return 0;
}

static int close_file(struct file *fp, int err) {
    int err2 = filp_close(fp, NULL);
    if (err2 < 0) {
        pr_debug(pr_format("cannot close file /proc/mounts because of error %d\n"), err);
    }
    if (!err) {
        return err2;
    }
    return err;
}

static int iter_begin(struct iter *it) {
    struct file *fp = file_open_root_mnt(procfs_mnt, "mounts", O_RDONLY, 0);
    if (IS_ERR(fp)) {
        pr_debug(pr_format("cannot open file /proc/mounts because of error %ld\n"), PTR_ERR(fp));
        return PTR_ERR(fp);
    }
    int err = __iter_begin(it, LINE_BUFSZ, FILE_BUFSZ, fp);
    if (err) {
        return close_file(fp, err);
    }
    return err;
}

static int iter_end(struct iter *it) {
    int err = filp_close(it->ff.fp, NULL);
    if (err < 0) {
        pr_debug(pr_format("cannot close file /proc/mounts because of error %d\n"), err);
    }
    kfree(it->bufp);
    return err;
}

static struct mnts_info* iter_next(struct iter *it, struct mnts_info *mi) {
    int err = 0;
    char *line = bufio_getline(it->linep, it->linesz, &it->ff);
    if (line != NULL && !IS_ERR(line)) {
        err = parse_mnts_info(line, mi);
        if (err) {
            pr_debug(pr_format("cannot parse %s got error %d\n"), line, err);
            return ERR_PTR(err);
        }
    }
    if (!line) {
        return NULL;
    }
    if (IS_ERR(line)) {
        pr_debug(pr_format("cannot complete parsing because of error %ld\n"), PTR_ERR(line));
        return ERR_CAST(line);
    }
    return mi;
}

static bool by_dev_name(struct mnts_info *mip, const char *arg) {
    char *dev_name = (char*)arg;
    return (strstr(mip->mnt_dev, "loop") && !strcmp(mip->mnt_point, dev_name))
           || !strcmp(mip->mnt_dev, dev_name);
}

static bool by_mountpoint(struct mnts_info *mip, const char *arg) {
    char *path = (char*)arg;
    return !strcmp(mip->mnt_point, path);
}

/**
 * bdev_from_file returns the device number of a block device associated to a device file in @param path.
 * @param path (input) the path of the device file - e.g. /dev/sda1
 * @param dev (output) device number associated to device file in @param path
 * @return 0 on success, < 0 otherwise
 */
static int bdev_from_file(const char *path, dev_t *dev) {
    struct file *fp = bdev_file_open_by_path(path, BLK_OPEN_READ, NULL, NULL);
    if (IS_ERR(fp)) {
        pr_debug(pr_format("failed to open block device '%s', got error %ld\n"), path, PTR_ERR(fp));
        return PTR_ERR(fp);
    }
    *dev = file_bdev(fp)->bd_dev;
    filp_close(fp, NULL);
    return 0;
}

int get_fdev(const char *mountpoint, dev_t *dev) {
    struct iter it;
    int err = iter_begin(&it);
    if (err) {
        return err;
    }
    struct mnts_info mi;
    struct mnts_info *mip;
    bool found = false;
    while ((mip = iter_next(&it, &mi)) != NULL) {
        if (IS_ERR(mip)) {
            err = PTR_ERR(mip);
            break;
        }
        found = by_mountpoint(mip, mountpoint);
        if (found) {
            break;
        }
    }
    if (found) {
        err = bdev_from_file(mip->mnt_dev, dev);
    }
    iter_end(&it);
    return err;
}