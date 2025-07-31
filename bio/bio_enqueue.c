#include "bio.h"
#include "pr_format.h"
#include <linux/bio.h>
#include <linux/bvec.h>
#include <linux/workqueue.h>

struct bio_work {
    struct bio *bio;
    struct work_struct work;
};

static struct workqueue_struct *wq;

int bio_deferred_work_init(void) {
    wq = create_workqueue("bio_wq");
    if (!wq) {
        pr_debug(pr_format("cannot create wq for bio(s)\n"));
        return -1;
    }
    return 0;
}

void bio_deferred_work_cleanup(void) {
    flush_workqueue(wq);
    destroy_workqueue(wq);
}

static void process_bio(struct work_struct *work) {
    struct bio_work *w = container_of(work, struct bio_work, work);
    struct bio *bio = w->bio;
    pr_debug(pr_format("processing bio for device (%d, %d)\n"), MAJOR(bio->bi_bdev->bd_dev), MINOR(bio->bi_bdev->bd_dev));
    struct bio_vec bv;
    struct bvec_iter iter;
    unsigned long total = 0;
    bio_for_each_bvec(bv, bio, iter) {
        pr_debug(pr_format("write request to [%d, %d)\n"), bv.bv_offset, bv.bv_len);
        total += bv.bv_len;
    }
    pr_debug(pr_format("total bytes written %ld\n"), total);
    submit_bio(bio);
}

bool bio_enqueue(struct bio *bio) {
    struct bio_work *w;
    w = kzalloc(sizeof(*w), GFP_KERNEL);
    if (!w) {
        pr_debug(pr_format("cannot create bio_work struct for device (%d, %d)\n"), MAJOR(bio->bi_bdev->bd_dev), MINOR(bio->bi_bdev->bd_dev));
        return false;
    }
    INIT_WORK(&w->work, process_bio);
    w->bio = bio;
    return queue_work(wq, &w->work);
}