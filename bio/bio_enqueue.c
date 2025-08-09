#include "bio.h"
#include "bio_utils.h"
#include "pr_format.h"
#include "snapshot.h"
#include <linux/bio.h>
#include <linux/bvec.h>
#include <linux/delay.h>
#include <linux/list.h>
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
    snapshot_save(bio);
    struct bio_private_data *priv = bio->bi_private;
    submit_bio(priv->orig_bio);
}

static inline int bio_block_add_page(struct bio *bio, int i, struct page *page) {
    struct bio_private_data *priv = bio->bi_private;
    if (i >= priv->block.nr_pages) {
        pr_debug(pr_format("trying to add too much pages to bio block private data"));
        return -1;
    }
    priv->block.pages[i] = page;
    return 0;
}

static inline int add_page(struct bio *bio, int i, struct page *page) {
    int err = bio_add_page(bio, page, PAGE_SIZE, 0);
    if (err < PAGE_SIZE) {
        return -1;
    }
    return bio_block_add_page(bio, i, page);
}

static inline void free_bio_pages(struct bio *bio) {
    struct bio_vec bv;
    struct bvec_iter iter;
    bio_for_each_bvec(bv, bio, iter) {
        __free_page(bv.bv_page);
    }
}

static int allocate_pages(struct bio *bio, int nr_pages) {
    int err = 0;
    for (int i = 0; i < nr_pages; ++i) {
        struct page *page = alloc_page(GFP_KERNEL);
        if (!page) {
            err = -ENOMEM;
            goto allocate_pages_out;
        }
        pr_debug(pr_format("add_page(bio, %d, page)"), i);
        err = add_page(bio, i, page);
        if (err) {
            __free_page(page);
            goto allocate_pages_out;
        }
    }
    return err;

allocate_pages_out:
    free_bio_pages(bio);
    return err;
}

static inline unsigned int bio_nr_pages(struct bio *bio) {
    return (bio->bi_iter.bi_size + PAGE_SIZE - 1) / PAGE_SIZE;
}

static struct bio_private_data *mk_private_data(sector_t sector, int nr_pages) {
    size_t size = sizeof(struct bio_private_data) + sizeof(struct page*) * nr_pages;
    struct bio_private_data *pd = kmalloc(size, GFP_KERNEL);
    if (!pd) {
        pr_debug(pr_format("cannot allocate enough memory for struct bio_private_data(%d)"), nr_pages);
        return NULL;
    }
    pd->block.nr_pages = nr_pages;
    pd->block.sector = sector;
    pd->block.pages = (struct page**)pd + sizeof(struct bio_private_data);
    return pd;
}

static struct bio* allocate_read_bio(struct bio *orig_bio) {
    struct bio_private_data *private_data = mk_private_data(bio_sector(orig_bio), bio_nr_pages(orig_bio));
    if (!private_data) {
        return NULL;
    }
    struct bio *read_bio = bio_alloc(orig_bio->bi_bdev, bio_nr_pages(orig_bio), REQ_OP_READ, GFP_KERNEL);
    if (!read_bio) {
        pr_debug(pr_format("bio_alloc failed"));
        kfree(private_data);
        return NULL;
    }
    read_bio->bi_iter.bi_sector = bio_sector(orig_bio);
    read_bio->bi_end_io = read_original_block_end_io;
    private_data->orig_bio = orig_bio;
    read_bio->bi_private = private_data;
    return read_bio;
}

static struct bio* create_read_bio(struct bio *orig_bio) {
    struct bio *read_bio = allocate_read_bio(orig_bio);
    if (!read_bio) {
        return NULL;
    }    
    if (allocate_pages(read_bio, bio_nr_pages(orig_bio))) {
        pr_debug(pr_format("allocate_pages failed"));
        kfree(read_bio->bi_private);
        bio_put(read_bio);
        return NULL;
    }
    return read_bio;
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
bool bio_enqueue(struct bio *bio) {
    struct bio_work *w;
    w = kzalloc(sizeof(*w), GFP_KERNEL);
    if (!w) {
        pr_debug(pr_format("cannot create bio_work struct for device (%d, %d)"), MAJOR(bio_devnum(bio)), MINOR(bio_devnum(bio)));
        return false;
    }
    w->orig_bio = bio;
    INIT_WORK(&w->work, process_bio);
    return queue_work(wq, &w->work);
}