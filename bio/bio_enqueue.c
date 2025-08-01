#include "bio.h"
#include "pr_format.h"
#include <linux/bio.h>
#include <linux/bvec.h>
#include <linux/delay.h>
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

static void bio_save_pages(struct bio *bio) {
    dbg_dump_bio("bio_save_pages: called with\n", bio);

    struct bio_vec *bv;
    int iter;
    bio_for_each_bvec_all(bv, bio, iter) {
        char *va = kmap_local_page(bv->bv_page);
        pr_debug(pr_format("processing data at address %s of size %d\n"), va, bv->bv_len);
        print_hex_dump_bytes("data: ", DUMP_PREFIX_OFFSET,
                             va, bv->bv_len);
        kunmap_local(va);
    }
    bio_put(bio);
    pr_debug(pr_format("bio_save_pages ended!"));
}

static int allocate_pages(struct bio *bio, unsigned int nr_pages) {
    dbg_dump_bio("allocate_pages: called with\n", bio);
    pr_debug(pr_format("allocate_pages: called with %d\n"), nr_pages);

    int err = 0;
    while (nr_pages > 0) {
        struct page *page = alloc_page(GFP_KERNEL);
        if (!page) {
            err = -ENOMEM;
            goto free_bio_pages;
        }
        err = bio_add_page(bio, page, PAGE_SIZE, 0);
        if (err != PAGE_SIZE) {
            err = nr_pages;
            __free_page(page);
            goto free_bio_pages;
        }
        nr_pages--;
    }
    dbg_dump_bio("all pages have been added successfully\n", bio);
    return 0;

free_bio_pages:
    struct bio_vec bv;
    struct bvec_iter iter;
    bio_for_each_bvec(bv, bio, iter) {
        __free_page(bv.bv_page);
    }
    return err;
}

static struct bio* init_read_bio(struct bio *wb) {
    dbg_dump_bio("init_read_bio: called with\n", wb);
    
    unsigned int nr_pages = (wb->bi_iter.bi_size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    struct bio *rb = bio_alloc(wb->bi_bdev, nr_pages, REQ_OP_READ, GFP_KERNEL);
    if (!rb) {
        pr_debug(pr_format("cannot create read_bio: bio_alloc failed"));
        return NULL;
    }
    rb->bi_iter.bi_sector = wb->bi_iter.bi_sector;
    dbg_dump_bio("init_read_bio: bio_alloc returned\n", rb);
    if (allocate_pages(rb, nr_pages)) {
        bio_put(rb);
        return NULL;
    }
    return rb;
}

static void save_snapshot(struct bio *write_bio) {
    struct bio *rb = init_read_bio(write_bio);
    if (!rb) {
        return;
    }
    dbg_dump_bio("before submit_bio_wait\n", rb);
    if (submit_bio_wait(rb)) {
        pr_debug(pr_format("submit_bio_wait() failed"));
        return;
    }
    bio_save_pages(rb);
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
    save_snapshot(bio);
    pr_debug(pr_format("submitting original bio"));
    mdelay(3000);
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