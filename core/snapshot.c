#include "bio.h"
#include "itree.h"
#include "pr_format.h"
#include "registry.h"
#include "small_bitmap.h"
#include "snap_map.h"
#include "snapshot.h"
#include <linux/bio.h>
#include <linux/bitmap.h>
#include <linux/blkdev.h>
#include <linux/bvec.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/namei.h>
#include <linux/string.h>
#include <linux/time64.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#define MAX_NAME_LEN (20)
#define ROOT_DIR     "/snapshots"

struct write_bio_work {
    struct work_struct  work;
    struct bio         *orig_bio;
};

struct file_work {
    struct work_struct       work;
    struct bio              *orig_bio;
    struct bio_private_data *p_data;
    struct timespec64        read_completed_on;
};

struct block_work {
    struct work_struct work;
    dev_t              device;
    sector_t           sector;
    struct page_iter   data;
    struct timespec64  session_created_on;
    char               session_id[];
};

struct workqueue_struct *write_bio_wq;
struct workqueue_struct *read_bio_wq;
struct workqueue_struct *save_blocks_wq;

static int mkdir_snapshots(void) {
    struct path parent;
    int err = kern_path("/", LOOKUP_DIRECTORY, &parent);
    if (err) {
        pr_err("kern_path failed on '/', got error %d (%s)", err, errtoa(err));
        return err;
    }

    struct dentry *d_parent = parent.dentry;
    inode_lock(d_inode(d_parent));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 16, 0)
    struct dentry *dentry = lookup_one(mnt_idmap(parent.mnt), &QSTR("snapshots"), d_parent);
#else
    struct dentry *dentry = lookup_one_len("snapshots", d_parent, strlen("snapshots"));
#endif
    if (IS_ERR(dentry)) {
        err = PTR_ERR(dentry);
        pr_err("lookup_one_len failed on 'snapshots', got error %d (%s)", err, errtoa(err));
        goto out_unlock_put;
    }

    if (d_really_is_positive(dentry)) {
        pr_debug(pr_format("directory /snapshots already exists"));
        dput(dentry);
        goto out_unlock_put;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 15, 0)
    dentry = vfs_mkdir(mnt_idmap(parent.mnt), d_inode(d_parent), dentry, 0755);
    if (IS_ERR(dentry)) {
        err = PTR_ERR(dentry);
        pr_err("vfs_mkdir failed on '%s', got error %d (%s)", ROOT_DIR, err, errtoa(err));
    } else {
        dput(dentry);
    }
#else
    err = vfs_mkdir(mnt_idmap(parent.mnt), d_inode(d_parent), dentry, 0755);
    if (err) {
        pr_err("vfs_mkdir failed on '%s', got error %d (%s)", ROOT_DIR, err, errtoa(err));
    } else {
        dput(dentry);
    }
#endif
out_unlock_put:
    inode_unlock(d_inode(d_parent));
    path_put(&parent);
    return err;
}

int snapshot_init(void) {
    int err = snap_map_init();
    if (err) {
        return err;
    }
    err = mkdir_snapshots();
    if (err) {
        return err;
    }
    write_bio_wq = alloc_ordered_workqueue("write-bio-wq", 0);
    if (!write_bio_wq) {
        pr_err("out of memory");
        return -ENOMEM;
    }
    read_bio_wq = alloc_workqueue("save-files-wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
    if (!read_bio_wq) {
        pr_err("out of memory");
        err = -ENOMEM;
        goto out;
    }
    save_blocks_wq = alloc_workqueue("save-blocks-wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
    if (!save_blocks_wq) {
        pr_err("out of memory");
        err = -ENOMEM;
        goto out2;
    }
    return 0;

out2:
    destroy_workqueue(read_bio_wq);
out:
    destroy_workqueue(write_bio_wq);
    return err;
}

void snapshot_cleanup(void) {
    if (write_bio_wq) {
        destroy_workqueue(write_bio_wq);
    }
    if (read_bio_wq) {
        destroy_workqueue(read_bio_wq);
    }
    if (save_blocks_wq) {
        destroy_workqueue(save_blocks_wq);
    }
    snap_map_cleanup();
}

static void file_write(const char *path, struct page_iter *it, unsigned long offset, unsigned long nbytes) {    
    struct file *fp = filp_open(path, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (IS_ERR(fp)) {
        pr_err("cannot open file: %s, got error %ld", path, PTR_ERR(fp));
        return;
    }
    if (offset + nbytes > it->offset + it->len) {
        pr_err("write to %s failed: lo=%lu + #B=%lu > off=%u + len=%u", path, offset, nbytes, it->offset, it->len);
        goto out;
    }
    void *va = page_address(it->page);
    loff_t off = 0;
    ssize_t n = kernel_write(fp, va + offset, nbytes, &off);
    if (n != nbytes) {
        pr_err("kernel_write failed to write whole page at %s", path);
    }
out:
    int err = filp_close(fp, NULL);
    if (err) {
        pr_err("filp_close failed to close file at %s, got error %d", path, err);
    }
}

static int mkdir_session(const char *session) {
    struct path parent;
    int err = kern_path(ROOT_DIR, LOOKUP_DIRECTORY, &parent);
    if (err) {
        pr_err("%s does not exist, got error %d (%s)", ROOT_DIR, err, errtoa(err));
        return err;
    }

    struct dentry *d_parent = parent.dentry;
    inode_lock(d_inode(d_parent));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 16, 0)
    struct dentry *dentry = lookup_one(mnt_idmap(parent.mnt), &QSTR(session), d_parent);
#else
    struct dentry *dentry = lookup_one_len(session, d_parent, strlen(session));
#endif
    if (IS_ERR(dentry)) {
        err = PTR_ERR(dentry);
        pr_err("lookup_one_len failed on '%s', got error %d (%s)", session, err, errtoa(err));
        goto out_unlock_put;
    }
    
    if (d_really_is_positive(dentry)) {
        pr_debug(pr_format("%s/%s already exists"), ROOT_DIR, session);
        dput(dentry);
        goto out_unlock_put;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 15, 0)
    dentry = vfs_mkdir(mnt_idmap(parent.mnt), d_inode(d_parent), dentry, 0755);
    if (IS_ERR(dentry)) {
        err = PTR_ERR(dentry);
        pr_err("vfs_mkdir failed on '%s/%s', got error %d (%s)", ROOT_DIR, session, err, errtoa(err));
    } else {
        dput(dentry);
    }
#else
    err = vfs_mkdir(mnt_idmap(parent.mnt), d_inode(d_parent), dentry, 0755);
    if (err) {
        pr_err("vfs_mkdir failed on %s/%s, got error %d (%s)", ROOT_DIR, session, err, errtoa(err));
    } else {
        dput(dentry);
    }
#endif
out_unlock_put:
    inode_unlock(d_inode(d_parent));
    path_put(&parent);
    return err;
}

static void save_block(struct work_struct *work) {
    struct block_work *w = container_of(work, struct block_work, work);
    size_t path_len = strlen(ROOT_DIR) + strlen(w->session_id) + MAX_NAME_LEN + 3;
    if (path_len > PATH_MAX) {
        goto out;
    }

    char *path = kzalloc(path_len, GFP_KERNEL);
    if (!path) {
        pr_err("out of memory");
        goto out;
    }
    
    unsigned long sectors_num = DIV_ROUND_UP(w->data.len, 512);
    struct small_bitmap map;
    unsigned long *added = small_bitmap_zeros(&map, sectors_num);
    if (!added) {
        goto free_data;
    }
    
    int err = snap_map_add_range(w->device, &w->session_created_on, w->sector, w->sector + sectors_num, added);
    if (err) {
        pr_err("cannot add range [%llu, %llu) to bitmap, got error %d", w->sector, w->sector + sectors_num, err);
        goto free_data;
    }
    
    unsigned long lo = 0, hi;
    while (small_bitmap_next_set_region(&map, &lo, &hi)) {
        sprintf(path, "%s/%s/%llu", ROOT_DIR, w->session_id, w->sector + lo);
        file_write(path, &w->data, w->data.offset + lo, (hi - lo) * 512);
        lo = hi;
    }
    
    small_bitmap_free(&map);
free_data:
    __free_pages(w->data.page, get_order(w->data.len));
    kfree(path);
out:
    kfree(w);
}

static inline void free_all_pages(struct bio_private_data *p_data) {
    struct page_iter *pos;
    page_iter_for_each(pos, p_data) {
        __free_pages(pos->page, get_order(pos->len));
    }
}

static void bio_private_data_destroy(struct bio_private_data *p_data) {
    free_all_pages(p_data);
    kfree(p_data);
}

/**
 * snapshot_save schedules each page or compound one of bio to the workqueue. Each page
 * will be saved to /snapshots/<session id>/<sector no>
 */
static void snapshot_save(struct work_struct *work) {
    struct file_work *w = container_of(work, struct file_work, work);
    submit_bio(w->orig_bio);
    if (!w->p_data) {
        return;
    }
    char *session = kzalloc(get_session_id_len() + 1, GFP_KERNEL);
    if (!session) {
        pr_err("out of memory");
        goto out;
    }
    struct bio_private_data *p_data = w->p_data;
    struct timespec64 session_created_on;
    if (!registry_session_id(p_data->dev, &w->read_completed_on, session, &session_created_on)) {
        pr_err("snapshot_save: no session associated to device %d:%d", MAJOR(p_data->dev), MINOR(p_data->dev));
        goto free_session;
    }

    // We completed successfully the read of the region to snapshot, so we
    // can add the whole range to the tree.
    struct b_range *range = b_range_alloc(p_data->sector, p_data->sector + DIV_ROUND_UP(p_data->bytes, 512));
    if (!range) {
        pr_err("out of memory");
        goto free_session;
    }
    int err = registry_add_range(p_data->dev, &session_created_on, range);
    if (err) {
        kfree(range);
    }

    err = mkdir_session(session);
    if (err && err != -EEXIST) {
        goto free_session;
    }

    err = snap_map_create(p_data->dev, &session_created_on);
    if (err && err != -EEXIST) {
        pr_err("cannot create bitmap, got error %d", err);
        goto free_session;
    }

    sector_t sector = p_data->sector;
    struct page_iter *pos;
    page_iter_for_each(pos, p_data) {
        struct block_work *b;
        b = kzalloc(sizeof(*b) + get_session_id_len() + 1, GFP_KERNEL);
        if (!b) {
            pr_err("out of memory");
            break;
        }
        b->device = p_data->dev;
        b->sector = sector;
        memcpy(b->session_id, session, get_session_id_len());
        memcpy(&b->session_created_on, &session_created_on, sizeof(session_created_on));
        memcpy(&b->data, pos, sizeof(*pos));
        INIT_WORK(&b->work, save_block);
        queue_work(save_blocks_wq, &b->work);
        sector += DIV_ROUND_UP(pos->len, 512);
    }
    kfree(session);
    kfree(w);
    return;

free_session:
    kfree(session);
out:
    bio_private_data_destroy(p_data);
    kfree(w);
}

static void read_bio_enqueue(struct bio *orig_bio, struct bio_private_data *p_data) {
    struct file_work *w;
    w = kzalloc(sizeof(*w), GFP_ATOMIC);
    if (!w) {
        pr_err("out of memory");
        return;
    }
    w->orig_bio = orig_bio;
    w->p_data = p_data;
    ktime_get_ts64(&w->read_completed_on);
    INIT_WORK(&w->work, snapshot_save);
    queue_work(read_bio_wq, &w->work);
}

/**
 * read_original_block_end_io saves current bio to file and submit original write bio to
 * block IO layer
 */
static void read_original_block_end_io(struct bio *bio) {
    struct bio_private_data *p_data = (struct bio_private_data*)bio->bi_private;
    struct bio *orig_bio = p_data->orig_bio;
    if (bio->bi_status != BLK_STS_OK) {
        pr_err("bio completed with error %d", bio->bi_status);
        bio_private_data_destroy(p_data);
        p_data = NULL;
    }
    read_bio_enqueue(orig_bio, p_data);
    bio_put(bio);
}

/**
 * add_page adds pages to the read bio and its private data, the former needs the pages to write the data it reads from disk, the
 * latter save the page content in the /snapshots directory.
 */
static inline int add_page(struct bio_vec *bvec, struct bio *bio) {
    struct bio_private_data *p = bio->bi_private;
    if (p->iter_len > p->iter_capacity) {
        pr_err("cannot add page to bio's private data: max number of page(s) is %lu", p->iter_capacity);
        return 0;
    }

    struct page *page = alloc_pages(GFP_KERNEL | __GFP_ZERO, get_order(bvec->bv_len));
    if (!page) {
        pr_err("out of memory");
        return -ENOMEM;
    }
    
    int err = bio_add_page(bio, page, bvec->bv_len, bvec->bv_offset);
    if (err != bvec->bv_len) {
        pr_err("bio_add_page failed");
        __free_pages(page, get_order(bvec->bv_len));
        return 0;
    }
    p->iter[p->iter_len].len = bvec->bv_len;
    p->iter[p->iter_len].offset = bvec->bv_offset;
    p->iter[p->iter_len++].page = page;
    p->bytes += bvec->bv_len;
    return bvec->bv_len;
}

static int allocate_pages(struct bio *bio, struct bio *orig_bio) {
    struct bio_vec bvec;
	struct bvec_iter it;
    bio_for_each_bvec(bvec, orig_bio, it) {
        if (add_page(&bvec, bio) != bvec.bv_len) {
            goto out;
        }
    }
    return 0;

out:
    free_all_pages((struct bio_private_data*)bio->bi_private);
    return -1;
}

/**
 * create_read_bio creates a read request to the block IO layer. This request has a callback that schedules the original
 * write bio after the targeted region has been read from the block device
 */
static struct bio* create_read_bio(struct bio *orig_bio) {
    struct bio_private_data *p_data;
    p_data = kzalloc(sizeof(*p_data) + sizeof(struct page_iter) * orig_bio->bi_vcnt, GFP_KERNEL);
    if (!p_data) {
        pr_err("out of memory");
        return NULL;
    }
    p_data->orig_bio = orig_bio;
    p_data->dev = orig_bio->bi_bdev->bd_dev;
    p_data->iter_capacity = orig_bio->bi_vcnt;
    sector_t sector = orig_bio->bi_iter.bi_sector;
    p_data->sector = sector;
    struct bio *read_bio = bio_alloc(orig_bio->bi_bdev, orig_bio->bi_vcnt, REQ_OP_READ, GFP_KERNEL);
    if (!read_bio) {
        pr_err("bio_alloc failed");
        goto no_bio;
    }
    read_bio->bi_end_io = read_original_block_end_io;
    read_bio->bi_iter.bi_sector = sector;
    read_bio->bi_private = p_data;
    if (allocate_pages(read_bio, orig_bio)) {
        pr_err("cannot allocate pages for read bio");
        goto no_pages;
    }
    return read_bio;

no_pages:
    bio_put(read_bio);
no_bio:
    kfree(p_data);
    return NULL;
}

/**
 * process_bio creates a read bio that targets the area involved in the write request (orig_bio).
 */
static void process_bio(struct work_struct *work) {
    struct write_bio_work *w = container_of(work, struct write_bio_work, work);
    struct bio *orig_bio = w->orig_bio;
    struct bio *read_bio = create_read_bio(orig_bio);
    if (read_bio) {
        submit_bio(read_bio);
    } else {
        submit_bio(orig_bio);
    }
    kfree(w);
}

/**
 * write_bio_enqueue schedules a (write) bio for deferred work.
 */
int write_bio_enqueue(struct bio *bio) {
    struct write_bio_work *w;
    w = kzalloc(sizeof(*w), GFP_ATOMIC);
    if (!w) {
        pr_err("out of memory");
        return -ENOMEM;
    }
    w->orig_bio = bio;
    INIT_WORK(&w->work, process_bio);
    queue_work(write_bio_wq, &w->work);
    return 0;
}