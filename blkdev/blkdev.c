#include "blkdev.h"
#include "pr_format.h"
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/printk.h>
#include <linux/sprintf.h>

#define DEV_NAME "dsnapshots"

static struct dsnapshots_dev {
    int                    major;
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

    blk_mq_end_request(rq, BLK_STS_OK);
    return BLK_STS_OK;
}

static struct blk_mq_ops qops = {
    .queue_rq = process_blk_request,
};

static int submit_bio_stack(struct bio *bio) {
    return 0;
}

static struct block_device_operations *bops = {
    .module = THIS_MODULE,
    .submit_bio = submit_bio_stack
};

static int mq_tag_set_create(struct blk_mq_tag_set *set) {
    set->ops = &qops;
    set->nr_hw_queues = 1;
    set->queue_depth = 128;
    set->cmd_size = sizeof(struct request);
    set->flags = BLK_MQ_F_SHOULD_MERGE;
    int err = blk_mq_alloc_tag_set(set);
    if (err) {
        pr_debug(pr_format("failed to allocate tag set for '%s': %d\n", DEV_NAME, err));
        return err;
    }
    return 0;
}

static int mq_init_queue(struct dsnapshots_dev *dev) {
    int err = mq_tag_set_create(&dev->tag_set);
    if (err) {
        return err;
    }
    // creates both hardware and software queues
    struct request_queue *queue = blk_mq_init_queue(dev->tag_set);
    if (IS_ERR(queue)) {
        pr_debug(pr_format("failed to initialize request queue for '%s'\n", DEV_NAME));
        blk_mq_free_tag_set(dev->tag_set);
        return PTR_ERR(queue);
    }
    // private date for the queue
    queue->queuedata = dev;
    dev->queue = queue;
    return 0;
}

static int gendisk_create(struct dsnapshots_dev *dev) {
    int err = mq_init_queue(dev);
    if (err) {
        return err;
    }
    struct gendisk *gd = blk_alloc_disk(1);
    if (IS_ERR(gd)) {
        pr_debug(pr_format("failed to allocate gendisk for '%s'\n", DEV_NAME));
        return PTR_ERR(gd);
    }
    gd->major = major;
    gd->first_minor = 0;
    gd->fops = &bops;
    gd->private_data = dev;
    snprintf(gd->disk_name, 32, "%sa", DEV_NAME);
    dev->gd = gd;
    add_disk(dev->gd);
    return 0;
}

int blkdev_init(void) {
    int major = register_blkdev(0, DEV_NAME);
    if (major < 0) {
        pr_debug(pr_format("failed to register block device '%s': %d\n", DEV_NAME, major));
        return major;
    }
    int err = gendisk_create(&dev);
    if (err) {
        unregister_blkdev(major, DEV_NAME);
        return err;
    }
    dev->major = major;
    return 0;
}

static void blkdev_delete(struct dsnapshots_dev *dev) {
    if (dev->gd) {
        del_gendisk(dev->gd);
    }
}

void blkdev_cleanup(void) {
    blkdev_delete(&dev);
    unregister_blkdev(0, DEV_NAME);
}