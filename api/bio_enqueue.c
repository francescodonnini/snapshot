#include "bio.h"
#include "bio_utils.h"
#include "pr_format.h"
#include "registry.h"
#include "snapshot.h"
#include <linux/bio.h>
#include <linux/bvec.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/workqueue.h>

struct bio_work {
    struct work_struct  work;
    struct bio         *orig_bio;
};

static struct workqueue_struct *wq;

int bio_deferred_work_init(void) {
    wq = alloc_ordered_workqueue("writebio-wq", 0);
    if (!wq) {
        pr_err("out of memory");
        return -ENOMEM;
    }
    return 0;
}

void bio_deferred_work_cleanup(void) {
    flush_workqueue(wq);
    destroy_workqueue(wq);
}

/**
 * read_original_block_end_io saves current bio to file and submit original write bio to
 * block IO layer
 */
static void read_original_block_end_io(struct bio *bio) {
    struct bio_private_data *p = bio->bi_private;
    if (bio->bi_status != BLK_STS_OK) {
        pr_err("bio completed with error %d", bio->bi_status);
    } else {
        snapshot_save(bio);
    }
    submit_bio(p->orig_bio);
    kfree(p);
    bio_put(bio);
}

/**
 * add_page adds pages to the read bio and its private data, the former needs the pages to write the data it reads from disk, the
 * latter save the page content in the /snapshots directory.
 */
static inline int add_page(struct bio_vec *bvec, struct bio *bio, int i) {
    struct bio_private_data *p = bio->bi_private;
    if (i >= p->iter_len) {
        pr_err("cannot add page to bio's private data: max number of page(s) is %d", p->iter_len);
        return 0;
    }

    int order = get_order(bvec->bv_len);
    struct page *page = alloc_pages(GFP_KERNEL, order);
    if (!page) {
        pr_err("out of memory");
        return -ENOMEM;
    }
    
    int err = bio_add_page(bio, page, bvec->bv_len, bvec->bv_offset);
    if (err != bvec->bv_len) {
        pr_err("bio_add_page failed");
        __free_pages(page, order);
        return 0;
    }

    p->iter[i].len = bvec->bv_len;
    p->iter[i].offset = bvec->bv_offset;
    p->iter[i].page = page;
    return bvec->bv_len;
}

static int allocate_pages(struct bio *bio, struct bio *orig_bio) {
    int count = 0;
    struct bio_vec bvec;
	struct bvec_iter it;
    bio_for_each_bvec(bvec, orig_bio, it) {
        if (add_page(&bvec, bio, count) != bvec.bv_len) {
            goto out;
        }
        ++count;
    }
    return 0;

out:
    struct bio_private_data *p = bio->bi_private;
    for (int i = 0; i < count; ++i) {
        __free_pages(p->iter[i].page, get_order(p->iter[i].len));
    }
    return -1;
}

/**
 * create_read_bio creates a read request to the block IO layer. This request has a callback that schedules the original
 * write bio after the targeted region has been read from the block device
 */
static struct bio* create_read_bio(struct bio *orig_bio) {
    struct bio_private_data *data;
    size_t size = sizeof(*data) + sizeof(struct page_iter) * orig_bio->bi_vcnt;
    data = kzalloc(size, GFP_KERNEL);
    if (!data) {
        pr_err("out of memory");
        return NULL;
    }
    data->orig_bio = orig_bio;
    data->iter_len = orig_bio->bi_vcnt;
    sector_t sector = bio_sector(orig_bio);
    data->sector = sector;
    struct bio *read_bio = bio_alloc(orig_bio->bi_bdev, orig_bio->bi_vcnt, REQ_OP_READ, GFP_KERNEL);
    if (!read_bio) {
        pr_err("bio_alloc failed");
        goto no_bio;
    }
    read_bio->bi_iter.bi_sector = sector;
    read_bio->bi_end_io = read_original_block_end_io;
    read_bio->bi_private = data;
    if (allocate_pages(read_bio, orig_bio)) {
        pr_err("cannot allocate pages for read bio");
        goto no_pages;
    }
    return read_bio;

no_pages:
    bio_put(read_bio);
no_bio:
    kfree(data);
    return NULL;
}

static void process_bio(struct work_struct *work) {
    struct bio_work *w = container_of(work, struct bio_work, work);
    struct bio *orig_bio = w->orig_bio;
    struct bio *rb = create_read_bio(orig_bio);
    if (!rb) {
        submit_bio(orig_bio);
    } else {
        submit_bio(rb);
    }
    kfree(w);
}

/**
 * bio_enqueue schedules a (write) bio for deferred work.
 */
int bio_enqueue(struct bio *bio) {
    struct bio_work *w;
    w = kzalloc(sizeof(*w), GFP_ATOMIC);
    if (!w) {
        pr_err("out of memory");
        return -ENOMEM;
    }
    w->orig_bio = bio;
    INIT_WORK(&w->work, process_bio);
    queue_work(wq, &w->work);
    return 0;
}