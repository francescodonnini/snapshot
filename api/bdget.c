#include "bdget.h"
#include "pr_format.h"
#include <linux/blkdev.h>
#include <linux/printk.h>

/**
 * bdev_from_file -- returns the device number of a block device associated to a device file
 * in @param path.
 * @param path (input)  -- the path of the device file - e.g. /dev/sda1
 * @param dev  (output) -- device number associated to device file in @param path
 * @return 0 on success, < 0 otherwise
 */
int bdev_from_file(const char *path, dev_t *dev) {
    struct file *fp = bdev_file_open_by_path(path, BLK_OPEN_READ, NULL, NULL);
    if (IS_ERR(fp)) {
        pr_debug(pr_format("failed to open block device '%s', got error %ld\n"), path, PTR_ERR(fp));
        return PTR_ERR(fp);
    }
    *dev = file_bdev(fp)->bd_dev;
    pr_debug(pr_format("device number for '%s' is (%d, %d)"),
             path,
             MAJOR(*dev), MINOR(*dev));
    filp_close(fp, NULL);
    return 0;
}