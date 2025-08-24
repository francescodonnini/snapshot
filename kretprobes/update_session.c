#include "update_session.h"
#include "loop_utils.h"
#include "pr_format.h"
#include "registry.h"
#include <linux/printk.h>

static inline int try_update_loop_dev(struct block_device *bdev, char *buf) {
    const char *ip = backing_loop_device_file(bdev, buf);    
    if (IS_ERR(ip)) {
        return PTR_ERR(ip);
    } else {
        return registry_create_session(ip, bdev->bd_dev);
    }
}

/**
 * registry_update_loop_device gets the path of the file backing the loop device and
 * updates the registry with the device number associated to the image.
 * Returns 0 on success, < 0 otherwise. Possible errors are -ENOMEM if it is not possible to make
 * enough space to hold the path of the image file or if the update operation to the registry fails
 * (see registry_create_session for more details).
 */
static int registry_update_loop_device(struct block_device *bdev) {
    char *buf = kmalloc(PATH_MAX, GFP_ATOMIC);
    if (!buf) {
        return -ENOMEM;
    }
    int err = try_update_loop_dev(bdev, buf);
    kfree(buf);
    return err;
}

void update_session(const char *dev_name, struct block_device *bdev) {
    pr_debug(pr_format("update_session(%s, %p)"), dev_name, bdev);
    int err;
    if (is_loop_device(bdev)) {
        err = registry_update_loop_device(bdev);
    } else {
        err = registry_create_session(dev_name, bdev->bd_dev);
    }
    if (err) {
        pr_debug(pr_format("cannot update device major=%d,minor=%d, got error %d"), MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev), err);
    } else {
        pr_debug(pr_format("device '%s' updated successfully with major=%d,minor=%d"), dev_name, MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev));
    }
}