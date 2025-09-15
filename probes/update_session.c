#include "update_session.h"
#include "loop_utils.h"
#include "pr_format.h"
#include "registry.h"
#include <linux/printk.h>

typedef int(*updt_ssn_t)(const char*,dev_t);

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
    err = updt_fn(ip, bdev->bd_dev);
out:
    kfree(buf);
    return err;
}



static int update_session(struct file *bd_file, updt_ssn_t updt_fn) {
    struct block_device *bdev = I_BDEV(bd_file->f_inode);
    int err;
    if (is_loop_device(bdev)) {
        err = registry_update_loop_device(bdev, updt_fn);
    } else {
        char *buf = kzalloc(PATH_MAX, GFP_ATOMIC);
        if (!buf) {
            return -ENOMEM;
        }
        char *dev_name = d_path(&bd_file->f_path, buf, PATH_MAX);
        err = updt_fn(dev_name, bdev->bd_dev);
        kfree(buf);
    }
    if (err) {
        pr_err("cannot update device %d:%d, got error %d", MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev), err);
    }
    return err;
}

/**
 * ext4_update_session create a session object associated to a block device named dev_name, it's called before the fill_super operation completes because some
 * filesystems like ext4 send bio requests before get_tree_bdev/mount_bdev completes. The session object is destroyed if the fill_super operation fails.
 */
int ext4_update_session(struct file *bd_file) {
    return update_session(bd_file, registry_session_prealloc);
}

int singlefilefs_update_session(struct file *bd_file) {
    return update_session(bd_file, registry_session_prealloc);
}