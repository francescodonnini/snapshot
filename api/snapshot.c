#include "bio.h"
#include "pr_format.h"
#include "registry.h"
#include "snapshot.h"
#include <linux/bio.h>
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
    struct bio_private_data *p_data;
};

static struct workqueue_struct *write_bio_wq;
static struct workqueue_struct *save_files_wq;

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
    int err = mkdir_snapshots();
    if (err) {
        return err;
    }
    write_bio_wq = alloc_ordered_workqueue("writebio-wq", 0);
    if (!write_bio_wq) {
        pr_err("out of memory");
        return -ENOMEM;
    }
    save_files_wq = alloc_workqueue("save-files-wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
    if (!save_files_wq) {
        pr_err("out of memory");
        destroy_workqueue(write_bio_wq);
        return -ENOMEM;
    }
    return 0;
}

void snapshot_cleanup(void) {
    if (write_bio_wq) {
        flush_workqueue(write_bio_wq);
        destroy_workqueue(write_bio_wq);
    }
    if (save_files_wq) {
        flush_workqueue(save_files_wq);
        destroy_workqueue(save_files_wq);
    }
}

static void file_write(const char *path, struct page_iter *it, unsigned long lo, unsigned long nbytes) {    
    struct file *fp = filp_open(path, O_CREAT | O_WRONLY, 0644);
    if (IS_ERR(fp)) {
        pr_err("cannot open file: %s, got error %ld", path, PTR_ERR(fp));
        return;
    }
    if (lo + nbytes > it->offset + it->len) {
        pr_err("write to %s failed: lo=%lu + #B=%lu > off=%u + len=%u", path, lo, nbytes, it->offset, it->len);
        goto out;
    }
    void *va = page_address(it->page);
    loff_t off = 0;
    ssize_t n = kernel_write(fp, va + it->offset + lo, nbytes, &off);
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

static void save_page(dev_t dev, sector_t sector, struct page_iter *iter) {
    char *session = kzalloc(UUID_STRING_LEN + 1, GFP_KERNEL);
    if (!session) {
        pr_err("out of memory");
        goto no_session;
    }
    if (!registry_session_id(dev, session)) {
        pr_err("no session associated to device %d:%d", MAJOR(dev), MINOR(dev));
        goto no_session;
    }
    int err = mkdir_session(session);
    if (err && err != -EEXIST) {
        goto free_session;
    }

    size_t n = strlen(ROOT_DIR) + strlen(session) + MAX_NAME_LEN + 3;
    if (n > PATH_MAX) {
        goto free_session;
    }
    char *path = kzalloc(n, GFP_KERNEL);
    if (!path) {
        goto free_session;
    }
    sector_t end = sector + (iter->len >> 9);
    sector_t lo_sector = sector;
    unsigned long lo = 0;
    unsigned long acc_len = 0;
    for (sector_t sector = sector; sector < end; sector++) {
        bool added;
        int err = registry_add_sector(dev, sector, &added);
        if (err) {
            pr_err("cannot add sector %llu to device %d:%d", sector, MAJOR(dev), MINOR(dev));
            continue;
        }
        if (added) {
            acc_len += 512;
        } else {
            if (acc_len > 0) {
                sprintf(path, "%s/%s/%llu", ROOT_DIR, session, lo_sector);
                file_write(path, iter, lo, acc_len);
                lo += acc_len;
                acc_len = 0;
            }
            lo += 512;
            lo_sector = sector + 1;
        }
    }
    if (acc_len > 0) {
        sprintf(path, "%s/%s/%llu", ROOT_DIR, session, lo_sector);
        file_write(path, iter, lo, acc_len);
    }
    kfree(path);
free_session:
    kfree(session);
no_session:
    __free_pages(iter->page, get_order(iter->len));
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
    // We completed successfully the read of the region to snapshot, so we
    // can add the whole range to the tree.
    struct bio_private_data *p_data = w->p_data;
    bool added;
    int err = registry_add_range(p_data->dev, p_data->sector, p_data->bytes, &added);
    if (err || !added) {
        bio_private_data_destroy(p_data);
        return;
    }
    sector_t sector = p_data->sector;
    struct page_iter *pos;
    page_iter_for_each(pos, p_data) {
        save_page(p_data->dev, sector, pos);
        sector += (pos->len >> 9);
    }
    kfree(p_data);
    kfree(w);
}

static void read_bio_enqueue(struct bio_private_data *p) {
    struct file_work *w;
    w = kzalloc(sizeof(*w), GFP_ATOMIC);
    if (!w) {
        pr_err("out of memory");
        return;
    }
    w->p_data = p;
    INIT_WORK(&w->work, snapshot_save);
    queue_work(save_files_wq, &w->work);
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
    } else {
        read_bio_enqueue(p_data);
    }
    submit_bio(orig_bio);
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

    struct page *page = alloc_pages(GFP_KERNEL, get_order(bvec->bv_len));
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