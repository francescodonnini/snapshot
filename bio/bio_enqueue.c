#include "bio.h"
#include "pr_format.h"
#include "snapshot.h"
#include <linux/bio.h>
#include <linux/bvec.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

struct bio_work {
    struct work_struct work;
    struct bio *orig_bio;
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

static void dbg_dump_bio_content(struct bio *bio) {
    struct bio_vec *bv;
    int iter;
    bio_for_each_bvec_all(bv, bio, iter) {
        char *va = kmap_local_page(bv->bv_page);
        pr_debug(pr_format("processing data at address %p of size %d\n"), va, bv->bv_len);
        print_hex_dump_bytes("data: ", DUMP_PREFIX_OFFSET, va, 64);
        kunmap_local(va);
    }
}

static void dbg_dump_read_bio(struct bio *bio) {
    dbg_dump_bio("read_original_block_end_io:\n", bio);
    dbg_dump_bio_content(bio);
}

static void read_original_block_end_io(struct bio *bio) {
    dbg_dump_read_bio(bio);
    struct bio *orig_bio = (struct bio*)bio->bi_private;
    snapshot_save(bio->bi_bdev->bd_dev, bio);
    dbg_dump_bio("original bio\n", orig_bio);
    submit_bio(orig_bio);
}

static inline unsigned int bio_nr_pages(struct bio *bio) {
    return (bio->bi_iter.bi_size + PAGE_SIZE - 1) / PAGE_SIZE;
}

static int allocate_pages(struct bio *bio, unsigned int nr_pages) {
    dbg_dump_bio("allocate_pages: called with\n", bio);
    pr_debug(pr_format("%d\n"), nr_pages);
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

static struct bio* init_read_bio(struct bio *orig_bio) {
    unsigned int nr_pages = bio_nr_pages(orig_bio);
    struct bio *read_bio = bio_alloc(orig_bio->bi_bdev, nr_pages, REQ_OP_READ, GFP_KERNEL);
    if (!read_bio) {
        pr_debug(pr_format("cannot create read_bio: bio_alloc failed"));
        return NULL;
    }
    read_bio->bi_iter.bi_sector = orig_bio->bi_iter.bi_sector;
    if (allocate_pages(read_bio, nr_pages)) {
        bio_put(read_bio);
        return NULL;
    }
    read_bio->bi_end_io = read_original_block_end_io;
    read_bio->bi_private = orig_bio;
    dbg_dump_bio("read bio created successfully\n", read_bio);
    return read_bio;
}

static void process_bio(struct work_struct *work) {
    struct bio_work *w = container_of(work, struct bio_work, work);
    struct bio *orig_bio = w->orig_bio;
    dbg_dump_bio("work-queue is processing original (WRITE) bio\n", orig_bio);
    struct bio *rb = init_read_bio(orig_bio);
    if (!rb) {
        submit_bio(orig_bio);
    } else {
        submit_bio(rb);
    }
    kfree(w);
}

bool bio_enqueue(struct bio *bio) {
    struct bio_work *w;
    w = kzalloc(sizeof(*w), GFP_KERNEL);
    if (!w) {
        pr_debug(pr_format("cannot create bio_work struct for device (%d, %d)\n"), MAJOR(bio->bi_bdev->bd_dev), MINOR(bio->bi_bdev->bd_dev));
        return false;
    }
    w->orig_bio = bio;
    INIT_WORK(&w->work, process_bio);
    return queue_work(wq, &w->work);
}