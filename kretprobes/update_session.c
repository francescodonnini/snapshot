#include "update_session.h"
#include "loop_utils.h"
#include "pr_format.h"
#include "registry.h"
#include <linux/printk.h>

typedef int(*updt_ssn_t)(const char*,struct block_device*);

/**
 * registry_update_loop_device gets the path of the file backing the loop device and
 * updates the registry with the device number associated to the image.
 * Returns 0 on success, < 0 otherwise. Possible errors are -ENOMEM if it is not possible to make
 * enough space to hold the path of the image file or if the update operation to the registry fails
 * (see registry_session_get for more details).
 */
static int registry_update_loop_device(struct block_device *bdev, updt_ssn_t updt_fn) {
    char *buf = kmalloc(PATH_MAX, GFP_ATOMIC);
    if (!buf) {
        return -ENOMEM;
    }
    int err;
    const char *ip = backing_loop_device_file(bdev, buf); 
    if (IS_ERR(ip)) {
        err = PTR_ERR(ip); 
        goto out;
    }
    err = updt_fn(ip, bdev);
out:
    kfree(buf);
    return err;
}

static int update_session(const char *dev_name, struct block_device *bdev, updt_ssn_t updt_fn) {
    int err;
    if (is_loop_device(bdev)) {
        err = registry_update_loop_device(bdev);
    } else {
        err = updt_fn(dev_name, bdev->bd_dev);
    }
    if (err) {
        pr_debug(pr_format("cannot update device major=%d,minor=%d, got error %d"), MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev), err);
    }
    return err;
}

/**
 * ext4_update_session create a session object associated to a block device named dev_name, it's called before the fill_super operation completes because some
 * filesystems like ext4 send bio requests before get_tree_bdev/mount_bdev completes. The session object is destroyed if the fill_super operation fails.
 */
int ext4_update_session(const char *dev_name, struct block_device *bdev) {
    return update_session(dev_name, bdev, registry_session_prealloc);
}

int singlefilefs_update_session(const char *dev_name, struct block_device *bdev) {
    return update_session(dev_name, bdev, registry_session_get_or_create);
}