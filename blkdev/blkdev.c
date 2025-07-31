#include "bdget.h"
#include "blkdev.h"
#include "pr_format.h"
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <linux/sprintf.h>

#define DEV_NAME "dsnapshots"

static struct dsnapshots_dev {
    int                    major;
    struct block_device   *bdev;
    struct gendisk        *gd;
    struct blk_mq_tag_set  tag_set;
    struct request_queue  *queue;
} dev;

// WARNING: runs in atomic context, so no sleeping allowed
static blk_status_t process_blk_request(struct blk_mq_hw_ctx *hctx,
                                        const struct blk_mq_queue_data *bd) {
    struct request *rq = bd->rq;
    struct dsnapshots_dev *dev = rq->q->queuedata;
    blk_mq_start_request(rq);
    if (blk_rq_is_passthrough(rq)) {
        pr_debug(pr_format("skip non-fs request\n"));
        blk_mq_end_request(rq, BLK_STS_IOERR);
        return BLK_STS_OK;
    }
    pr_debug(pr_format("processing request %p for device '(%d, 0)'\n"), rq, dev->major);
    blk_mq_end_request(rq, BLK_STS_OK);
    return BLK_STS_OK;
}

static struct blk_mq_ops qops = {
    .queue_rq = process_blk_request,
};

static const struct block_device_operations bops = {
    .owner = THIS_MODULE,
};

static void tag_set_init(struct blk_mq_tag_set *tag_set, struct dsnapshots_dev *dev) {
    memset(&dev->tag_set, 0, sizeof(dev->tag_set));
    dev->tag_set.ops = &qops;
    dev->tag_set.nr_hw_queues = 1;
    dev->tag_set.queue_depth = 128;
    dev->tag_set.numa_node = NUMA_NO_NODE;
    dev->tag_set.cmd_size = 0;
    dev->tag_set.driver_data = dev;
}

/**
 * gendisk_create - creates a disk and adds it to the system.
 */
static int gendisk_create(struct dsnapshots_dev *dev) {
    int nr_minors = 1;
    tag_set_init(&dev->tag_set, dev);
    int err = blk_mq_alloc_tag_set(&dev->tag_set);
    if (err) {
        pr_debug(pr_format("cannot allocate tag set for '%s'\n"), DEV_NAME);
        return err;
    }

    struct gendisk *gd = blk_mq_alloc_disk(&dev->tag_set, NULL, NULL);
    if (IS_ERR(gd)) {
        err = PTR_ERR(gd);
        pr_debug(pr_format("failed to allocate gendisk for '%s', got error %d\n"), DEV_NAME, err);
        goto free_tag_set;
    }
    snprintf(gd->disk_name, 32, "%sa", DEV_NAME);
    gd->major = dev->major;
    gd->first_minor = 0;
    gd->minors = nr_minors;
    gd->fops = &bops;
    gd->private_data = dev;
    dev->queue = gd->queue;
    // set_capacity(gd, (1024 * 1024 * 1024) / 512);
    dev->gd = gd;
    err = add_disk(dev->gd);
    if (err) {
        pr_debug(pr_format("failed to add gendisk for '%s', got error %d\n"), DEV_NAME, err);
        goto add_disk_failed;
    }
    return 0;

add_disk_failed:
    put_disk(dev->gd);
free_tag_set:
    blk_mq_free_tag_set(&dev->tag_set);
    return err;
}

static void gendisk_delete(struct dsnapshots_dev *dev) {
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
    return file_bdev(fp);
}

int blkdev_init(void) {
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
    return 0;

delete_disk:
    gendisk_delete(&dev);
unregister_bdev:
    unregister_blkdev(major, DEV_NAME);
    return err;
}

struct block_device* bdget(void) {
    return dev.bdev;
}

void blkdev_cleanup(void) {
    gendisk_delete(&dev);
    unregister_blkdev(dev.major, DEV_NAME);
}