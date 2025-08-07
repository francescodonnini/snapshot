#include "bnull.h"
#include "bio.h"
#include "pr_format.h"
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <linux/sprintf.h>

#define DEV_NAME       "bnull"
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
    struct bnull_dev *dev = rq->q->queuedata;
    blk_mq_start_request(rq);
    if (blk_rq_is_passthrough(rq)) {
        pr_debug(pr_format("skip non-fs request\n"));
        blk_mq_end_request(rq, BLK_STS_IOERR);
        return BLK_STS_OK;
    }
    if (dev->bdev) {
        pr_debug(
            pr_format("processing request for device '(%d, %d)'\n"),
            MAJOR(dev->bdev->bd_dev),
            MINOR(dev->bdev->bd_dev));
    }
    if (rq->bio) {
        dbg_dump_bio("processing bio:\n", rq->bio);
    }
    blk_mq_end_request(rq, BLK_STS_OK);
    return BLK_STS_OK;
}

static struct blk_mq_ops qops = {
    .queue_rq = null_queue_rq,
};

static const struct block_device_operations bops = {
    .owner = THIS_MODULE,
};

static void tag_set_init(struct bnull_dev *blk_dev) {
    memset(&blk_dev->tag_set, 0, sizeof(blk_dev->tag_set));
    blk_dev->tag_set.ops = &qops;
    blk_dev->tag_set.nr_hw_queues = 1;
    blk_dev->tag_set.queue_depth = 128;
    blk_dev->tag_set.numa_node = NUMA_NO_NODE;
    blk_dev->tag_set.cmd_size = 0;
    blk_dev->tag_set.driver_data = blk_dev;
}

static int alloc_tag_set(struct bnull_dev *blk_dev) {
    tag_set_init(blk_dev);
    int err = blk_mq_alloc_tag_set(&blk_dev->tag_set);
    if (err) {
        pr_debug(pr_format("cannot allocate tag set for '%s'\n"), DEV_NAME);
    }
    return err;
}

/**
 * gendisk_create - creates a disk and adds it to the system.
 */
static int gendisk_create(struct bnull_dev *blk_dev) {
    int nr_minors = 1;
    int err = alloc_tag_set(blk_dev);
    if (err) {
        return err;
    }
    struct gendisk *gd = blk_mq_alloc_disk(&blk_dev->tag_set, NULL, NULL);
    if (IS_ERR(gd)) {
        err = PTR_ERR(gd);
        pr_debug(pr_format("failed to allocate gendisk for '%s', got error %d\n"), DEV_NAME, err);
        goto free_tag_set;
    }
    snprintf(gd->disk_name, 32, "%s", DEV_NAME);
    gd->major = blk_dev->major;
    gd->first_minor = 0;
    gd->minors = nr_minors;
    gd->fops = &bops;
    gd->private_data = blk_dev;
    blk_dev->queue = gd->queue;
    blk_dev->queue->queuedata = blk_dev;
    set_capacity(gd, BNULL_CAPACITY);
    blk_dev->gd = gd;
    err = add_disk(blk_dev->gd);
    if (err) {
        pr_debug(pr_format("failed to add gendisk for '%s', got error %d\n"), DEV_NAME, err);
        goto add_disk_failed;
    }
    return 0;

add_disk_failed:
    put_disk(blk_dev->gd);
free_tag_set:
    blk_mq_free_tag_set(&blk_dev->tag_set);
    return err;
}

static void gendisk_delete(struct bnull_dev *dev) {
    if (dev->gd) {
        del_gendisk(dev->gd);
        put_disk(dev->gd);
    }
    blk_mq_free_tag_set(&dev->tag_set);
}

static struct block_device* bdev_get_by_dev(dev_t dev) {
    blk_mode_t mode = BLK_OPEN_WRITE;
    struct file *fp = bdev_file_open_by_dev(dev, mode, NULL, NULL);
    if (IS_ERR(fp)) {
        pr_debug(pr_format("failed to open block device '%s'\n"), DEV_NAME);
        return ERR_CAST(fp);
    }
    struct block_device *blk_dev = file_bdev(fp);
    filp_close(fp, NULL);
    return blk_dev;
}

int bnull_init(void) {
    int major = register_blkdev(0, DEV_NAME);
    if (major < 0) {
        pr_debug(pr_format("failed to register block device '%s': %d\n"), DEV_NAME, major);
        return major;
    }
    dev.major = major;
    int err = gendisk_create(&dev);
    if (err) {
        goto unregister_bdev;
    }
    struct block_device *bdev = bdev_get_by_dev(MKDEV(major, 0));
    if (IS_ERR(bdev) || !bdev) {
        err = PTR_ERR(bdev);
        pr_debug(pr_format("failed to get block device '%s', got error %d\n"), DEV_NAME, err);
        goto delete_disk;
    }
    dev.bdev = bdev;
    pr_debug(
        pr_format("instance of '%s' created successfully: maj,min=%d,%d"),
        DEV_NAME, MAJOR(dev.bdev->bd_dev), MINOR(dev.bdev->bd_dev));
    return 0;

delete_disk:
    gendisk_delete(&dev);
unregister_bdev:
    unregister_blkdev(dev.major, DEV_NAME);
    return err;
}

void bnull_cleanup(void) {
    gendisk_delete(&dev);
    unregister_blkdev(dev.major, DEV_NAME);
}

struct block_device *bnull_get_bdev(void) {
    return dev.bdev;
}