#include "loop_utils.h"
#include <linux/blk-mq.h>
#include <linux/blkdev.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/loop.h>
#include <linux/types.h>

struct loop_device {
	int                      lo_number;
	loff_t		             lo_offset;
	loff_t		             lo_sizelimit;
	int		                 lo_flags;
	char		             lo_file_name[LO_NAME_SIZE];

	struct file	            *lo_backing_file;
	unsigned int	         lo_min_dio_size;
	struct block_device     *lo_device;

	gfp_t                    old_gfp_mask;

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

char *backing_loop_device_file(struct block_device *bdev, char *buf) {
	struct loop_device *lo = (struct loop_device*)bdev->bd_disk->private_data;
    return d_path(&lo->lo_backing_file->f_path, buf, PATH_MAX);
}
