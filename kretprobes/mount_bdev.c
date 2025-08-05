#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "registry.h"
#include <linux/blk-mq.h>
#include <linux/blk_types.h>
#include <linux/blkdev.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/loop.h>
#include <linux/major.h>
#include <linux/printk.h>


struct loop_device {
	int                      lo_number;
	loff_t		             lo_offset;
	loff_t		             lo_sizelimit;
	int		                 lo_flags;
	char		             lo_file_name[LO_NAME_SIZE];

	struct file	            *lo_backing_file;
	unsigned int	         lo_min_dio_size;
	struct block_device     *lo_device;

	gfp_t old_gfp_mask;

	spinlock_t               lo_lock;
	int                      lo_state;
	spinlock_t               lo_work_lock;
	struct workqueue_struct *workqueue;
	struct work_struct       rootcg_work;
	struct list_head         rootcg_cmd_list;
	struct list_head         idle_worker_list;
	struct rb_root           worker_tree;
	struct timer_list        timer;
	bool                     sysfs_inited;

	struct request_queue	*lo_queue;
	struct blk_mq_tag_set	 tag_set;
	struct gendisk		    *lo_disk;
	struct mutex		     lo_mutex;
	bool			         idr_visible;
};

static inline struct file_system_type* get_fstype(struct pt_regs *regs) {
    return get_arg1(struct file_system_type*, regs);
}

static inline char* get_dev_name(struct pt_regs *regs) {
    return get_arg3(char*, regs);
}

int mount_bdev_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    pr_debug(
        pr_format("mount_bdev(%s, %s) called\n"),
        get_fstype(regs)->name,
        get_dev_name(regs));
    // store the device name in the kretprobe instance data
    // so that it can be used in the return handler to check if the device has been successfully mounted
    struct mount_bdev_data *data = (struct mount_bdev_data*)kp->data;
    data->dev_name = get_dev_name(regs);
    return 0;
}

static inline bool is_loop_device(struct block_device *bdev) {
    return MAJOR(bdev->bd_dev) == LOOP_MAJOR;
}

static int registry_update_loop_device(struct block_device *bdev) {
    char *buf = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!buf) {
        return -ENOMEM;
    }
    struct loop_device *lo = (struct loop_device*)bdev->bd_disk->private_data;
    char *ip = d_path(&lo->lo_backing_file->f_path, buf, PATH_MAX);
    int err = 0;
    if (IS_ERR(ip)) {
        err = PTR_ERR(ip);
    } else {
        err = registry_update(ip, bdev->bd_dev);
    }
    kfree(buf);
    return err;
}

int mount_bdev_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct dentry *dentry = get_rval(struct dentry*, regs);
    if (IS_ERR(dentry)) {
        pr_debug(pr_format("mount_bdev failed with error: %ld\n"), PTR_ERR(dentry));
        return 0;
    }

    struct mount_bdev_data *data = (struct mount_bdev_data*)kp->data;
    struct block_device *bdev = dentry->d_sb->s_bdev;
    pr_debug(pr_format("mounted block device (%d, %d) with bdev %s\n"),
             MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev), data->dev_name);

    if (registry_lookup_mm(bdev->bd_dev)) {
        pr_debug(pr_format("device %s associated to device number (%d, %d) is already registered"),
                 data->dev_name, MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev));
        return 0;
    }

    if (is_loop_device(bdev)) {
        registry_update_loop_device(bdev);
    } else {
        registry_update(data->dev_name, bdev->bd_dev);
    }
    return 0;
}