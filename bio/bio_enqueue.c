#include "bio.h"
#include "bio_utils.h"
#include "pr_format.h"
#include "snapshot.h"
#include <linux/bio.h>
#include <linux/bvec.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/workqueue.h>

struct bio_work {
    struct work_struct work;
    struct bio *orig_bio;
};

static struct workqueue_struct *wq;

int bio_deferred_work_init(void) {
    wq = create_workqueue("bio_wq");
    if (!wq) {
        pr_debug(pr_format("cannot create wq for bio(s)"));
        return -ENOMEM;
    }
    return 0;
}

void bio_deferred_work_cleanup(void) {
    flush_workqueue(wq);
    destroy_workqueue(wq);
}

static void dbg_dump_bio_content(struct bio *bio) {
    struct bio_private_data *p = bio->bi_private;
    for (int i = 0; i < p->nr_pages; ++i) {
        struct page_iter *it = &p->iter[i];
        char *va = kmap_local_page(it->page);
        pr_debug(pr_format("processing data at address %p of size %lu\n"), va, PAGE_SIZE);
        print_hex_dump_bytes("data: ", DUMP_PREFIX_OFFSET, va + it->offset, it->len);
        kunmap_local(va);
    }
}

static void dbg_dump_read_bio(const char *prefix, struct bio *bio) {
    dbg_dump_bio(prefix, bio);
    if (bio->bi_status == BLK_STS_OK) {
        dbg_dump_bio_content(bio);
    }
}

static inline void free_bio_pages(struct bio_private_data *p) {
    for (int i = 0; i < p->nr_pages; ++i) {
        __free_page(p->iter[i].page);
    }
}

/**
 * read_original_block_end_io saves current bio to file and submit original write bio to
 * block IO layer
 */
static void read_original_block_end_io(struct bio *bio) {
    if (bio->bi_status == BLK_STS_OK) {
        int err = -90000;
        if (err) {
            pr_debug(
                pr_format("cannot save snapshot %llu for device %d,%d, got error %d"),
                bio->bi_iter.bi_sector,
                MAJOR(bio->bi_bdev->bd_dev), MINOR(bio->bi_bdev->bd_dev),
                err);
            free_bio_pages((struct bio_private_data*)bio->bi_private);
        }
    } else {
        pr_debug(pr_format("bio completed with error %d"), bio->bi_status);
    }
    struct bio_private_data *p = bio->bi_private;
    submit_bio(p->orig_bio);
    kfree(p);
    bio_put(bio);
}

static inline int __add_page(struct bio *bio, int i, struct page *page, unsigned int len, unsigned int offset) {
    pr_debug(pr_format("i=%d,len=%d,offset=%d"), i, len, offset);
    struct bio_private_data *p = bio->bi_private;
    if (i >= p->nr_pages) {
        pr_debug(pr_format("trying to add too much pages to bio block private data"));
        return -1;
    }
    p->iter[i].page = page;
    p->iter[i].len = len;
    p->iter[i].offset = offset;
    return 0;
}

static inline int add_page(struct bio_vec *bvec, struct bio *bio, int i) {
    pr_debug(pr_format("bvec=(len=%d,off=%d,page=%p)"), bvec->bv_len, bvec->bv_offset, bvec->bv_page);
    struct page *page = alloc_page(GFP_KERNEL);
    if (!page) {
        return -ENOMEM;
    }
    int err = bio_add_page(bio, page, bvec->bv_len, bvec->bv_offset);
    if (err != bvec->bv_len) {
        __free_page(page);
        return -1;
    }
    return __add_page(bio, i, page, bvec->bv_len, bvec->bv_offset);
}

static int allocate_pages(struct bio *bio, struct bio *orig_bio) {
    int count = 0;
    int err = 0;
    struct bvec_iter old;
    pr_debug(pr_format("before: (done=%d,idx=%d,sector=%llu,size=%d)"), orig_bio->bi_iter.bi_bvec_done, orig_bio->bi_iter.bi_idx, orig_bio->bi_iter.bi_sector, orig_bio->bi_iter.bi_size);
    memcpy(&old, &orig_bio->bi_iter, sizeof(struct bvec_iter));
    struct bio_vec bvec;
	struct bvec_iter it;
    bio_for_each_bvec(bvec, orig_bio, it) {
        pr_debug(pr_format("iter=(done=%d,idx=%d,sector=%llu,size=%d)"), it.bi_bvec_done, it.bi_idx, it.bi_sector, it.bi_size);
        pr_debug(pr_format("bvec=(len=%d,off=%d,page=%p)"), bvec.bv_len, bvec.bv_offset, bvec.bv_page);
        int err = add_page(&bvec, bio, count);
        if (err) {
            goto allocate_pages_out;
        }
        ++count;
    }
    pr_debug(pr_format("after: (done=%d,idx=%d,sector=%llu,size=%d)"), orig_bio->bi_iter.bi_bvec_done, orig_bio->bi_iter.bi_idx, orig_bio->bi_iter.bi_sector, orig_bio->bi_iter.bi_size);
    memcpy(&orig_bio->bi_iter, &old, sizeof(struct bvec_iter));
    pr_debug(pr_format("restored: (done=%d,idx=%d,sector=%llu,size=%d)"), orig_bio->bi_iter.bi_bvec_done, orig_bio->bi_iter.bi_idx, orig_bio->bi_iter.bi_sector, orig_bio->bi_iter.bi_size);
    return err;

allocate_pages_out:
    struct bio_private_data *p = bio->bi_private;
    for (int i = 0; i < count; ++i) {
        __free_page(p->iter[i].page);
    }
    return err;
}

/**
 * create_read_bio creates a read request to the block IO layer. This request has a callback that schedules the original
 * write bio after the targeted region has been read from the block device
 */
static struct bio* create_read_bio(struct bio *orig_bio) {
    struct bio_private_data *data;
    size_t size = sizeof(*data) + sizeof(struct page_iter) * orig_bio->bi_vcnt;
    data = kmalloc(size, GFP_KERNEL);
    if (!data) {
        pr_debug(pr_format("cannot allocate enough memory for struct bio_private_data(%d)"), orig_bio->bi_vcnt);
        return NULL;
    }
    data->orig_bio = orig_bio;
    data->nr_pages = orig_bio->bi_vcnt;
    data->sector = bio_sector(orig_bio);
    if (!data) {
        return NULL;
    }
    struct bio *read_bio = bio_alloc(orig_bio->bi_bdev, orig_bio->bi_vcnt, REQ_OP_READ, GFP_KERNEL);
    if (!read_bio) {
        pr_debug(pr_format("bio_alloc failed"));
        kfree(data);
        return NULL;
    }
    read_bio->bi_iter.bi_sector = bio_sector(orig_bio);
    read_bio->bi_end_io = read_original_block_end_io;
    read_bio->bi_private = data;
    if (allocate_pages(read_bio, orig_bio)) {
        pr_debug(pr_format("allocate_pages failed"));
        kfree(read_bio->bi_private);
        bio_put(read_bio);
        return NULL;
    }
    dbg_dump_bio("read bio created successfully:\n", read_bio);
    return read_bio;
}

static void process_bio(struct work_struct *work) {
    struct bio_work *w = container_of(work, struct bio_work, work);
    struct bio *orig_bio = w->orig_bio;
    dbg_dump_bio("processing write bio:\n", orig_bio);
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
bool bio_enqueue(struct bio *bio) {
    struct bio_work *w;
    w = kzalloc(sizeof(*w), GFP_KERNEL);
    if (!w) {
        pr_debug(pr_format("cannot create bio_work struct for device (%d, %d)"), MAJOR(bio_devno(bio)), MINOR(bio_devno(bio)));
        return false;
    }
    w->orig_bio = bio;
    INIT_WORK(&w->work, process_bio);
    return queue_work(wq, &w->work);
}