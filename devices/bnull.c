#include "bnull.h"
#include "bio.h"
#include "pr_format.h"
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <linux/sprintf.h>

#define DEV_NAME       "bnull"
#define BLOCK_MINORS   (1)
#define BNULL_CAPACITY (1024)

static struct bnull_dev {
    int                    major;
    int                    first_minor;
    struct block_device   *bdev;
    struct gendisk        *gd;
    struct blk_mq_tag_set  tag_set;
    struct request_queue  *queue;
} dev;

// WARNING: runs in atomic context, so no sleeping allowed
static blk_status_t null_queue_rq(struct blk_mq_hw_ctx *hctx,
                                  const struct blk_mq_queue_data *bd) {
    struct request *rq = bd->rq;
    blk_mq_start_request(rq);
    blk_mq_end_request(rq, BLK_STS_OK);
    return BLK_STS_OK;
}

static struct blk_mq_ops qops = {
    .queue_rq = null_queue_rq,
};

static const struct block_device_operations bops = {
    .owner = THIS_MODULE,
};

static void gendisk_delete(struct bnull_dev *dev) {
    del_gendisk(dev->gd);
    put_disk(dev->gd);
    blk_mq_free_tag_set(&dev->tag_set);
}

/**
 * disk_create - creates a disk and adds it to the system.
 */
static int disk_create(struct bnull_dev *blk_dev) {
    blk_dev->tag_set.ops = &qops;
    blk_dev->tag_set.nr_hw_queues = 1;
    blk_dev->tag_set.queue_depth = 128;
    blk_dev->tag_set.numa_node = NUMA_NO_NODE;
    blk_dev->tag_set.cmd_size = 0;
    blk_dev->tag_set.driver_data = blk_dev;
    int err = blk_mq_alloc_tag_set(&blk_dev->tag_set);
    if (err) {
        pr_err("cannot allocate tag set for device %s, got error %d", DEV_NAME, err);
        return err;
    }

    struct gendisk *gd = blk_mq_alloc_disk(&blk_dev->tag_set, NULL, blk_dev);
    if (IS_ERR(gd)) {
        err = PTR_ERR(gd);
        pr_err("cannot allocate gendisk for device %s, got error %d", DEV_NAME, err);
        goto out;
    }

    snprintf(gd->disk_name, 32, "%s", DEV_NAME);
    gd->major = blk_dev->major;
    gd->first_minor = 0;
    gd->minors = BLOCK_MINORS;
    gd->fops = &bops;
    gd->private_data = blk_dev;
    blk_dev->queue = gd->queue;
    blk_dev->queue->queuedata = blk_dev;
    set_capacity(gd, BNULL_CAPACITY);
    err = add_disk(gd);
    if (err) {
        pr_err("failed to add gendisk for device %s, got error %d", DEV_NAME, err);
        goto out2;
    }
    blk_dev->gd = gd;
    return 0;

out2:
    put_disk(gd);
out:
    blk_mq_free_tag_set(&blk_dev->tag_set);
    return err;
}

int bnull_init(void) {
    memset(&dev, 0, sizeof(dev));
    int major = register_blkdev(0, DEV_NAME);
    if (major < 0) {
        pr_err("unable to register %s block device, got error %d", DEV_NAME, major);
        return major;
    }
    dev.major = major;

    int err = disk_create(&dev);
    if (err) {
        goto unregister_bdev;
    }

    struct block_device *bdev = dev.gd->part0;
    if (!bdev) {
        pr_err("block device structure is NULL");
        err = -ENODEV;
        goto delete_disk;
    }
    dev.bdev = bdev;
    return 0;

delete_disk:
    gendisk_delete(&dev);
unregister_bdev:
    unregister_blkdev(dev.major, DEV_NAME);
    return err;
}

void bnull_cleanup() {
    gendisk_delete(&dev);
    unregister_blkdev(dev.major, DEV_NAME);
}

struct block_device *bnull_get_bdev(void) {
    return dev.bdev;
}