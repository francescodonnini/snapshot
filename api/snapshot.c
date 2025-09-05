#include "snapshot.h"
#include "bio_utils.h"
#include "fast_hash.h"
#include "iset.h"
#include "pr_format.h"
#include "registry.h"
#include <linux/bio.h>
#include <linux/bvec.h>
#include <linux/errname.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/list.h>
#include <linux/path.h>
#include <linux/printk.h>
#include <linux/sprintf.h>
#include <linux/version.h>
#include <linux/workqueue.h>

#define MAX_NAME_LEN (20)
#define ROOT_DIR     "/snapshots"

struct save_work {
    struct work_struct  work;
    dev_t               devno;
    sector_t            sector;
    struct page_iter    iter;
};

static struct workqueue_struct *queue;

static int mkdir_snapshots(void) {
    struct path parent;
    int err = kern_path("/", LOOKUP_DIRECTORY, &parent);
    if (err) {
        pr_err("kern_path failed on '/', got error %d (%s)", err, errtoa(err));
        return err;
    }

    struct dentry *d_parent = parent.dentry;
    inode_lock(d_inode(d_parent));
    struct dentry *dentry = lookup_one_len("snapshots", d_parent, strlen("snapshots"));
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

/**
 * snapshot_init creates the directory /snapshots if not exists and
 * initializes the necessary data structures used by this subsystem.
 * @returns 0 on success, <0 otherwise
 */
int snapshot_init(void) {
    queue = alloc_workqueue("blocks-wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
    if (!queue) {
        pr_err("cannot create workqueue to save file(s)");
        return -ENOMEM;
    }
    int err = mkdir_snapshots();
    if (err) {
        destroy_workqueue(queue);
    }
    return err;
}

void snapshot_cleanup(void) {
    flush_workqueue(queue);
    destroy_workqueue(queue);
}

static void write_to_file(const char *path, struct page_iter *it, unsigned long lo, unsigned long nbytes) {    
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
    struct dentry *dentry = lookup_one_len(session, d_parent, strlen(session));
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

static void save_page(struct work_struct *work) {
    struct save_work *w = container_of(work, struct save_work, work);
    char *session = kzalloc(UUID_STRING_LEN + 1, GFP_KERNEL);
    if (!session) {
        pr_err("out of memory");
        goto no_session;
    }
    if (!registry_session_id(w->devno, session)) {
        pr_err("no session associated to device %d:%d", MAJOR(w->devno), MINOR(w->devno));
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
    sector_t end = w->sector + (w->iter.len >> 9);
    sector_t lo_sector = w->sector;
    unsigned long lo = 0;
    unsigned long acc_len = 0;
    for (sector_t sector = w->sector; sector < end; sector++) {
        bool added;
        int err = registry_add_sector(w->devno, sector, &added);
        if (err) {
            break;
        }
        if (added) {
            acc_len += 512;
        } else {
            if (acc_len > 0) {
                sprintf(path, "%s/%s/%llu", ROOT_DIR, session, lo_sector);
                write_to_file(path, &w->iter, lo, acc_len);
                lo += acc_len;
                acc_len = 0;
            }
            lo += 512;
            lo_sector = sector + 1;
        }
    }
    if (acc_len > 0) {
        sprintf(path, "%s/%s/%llu", ROOT_DIR, session, lo_sector);
        write_to_file(path, &w->iter, lo, acc_len);
    }
    kfree(path);
free_session:
    kfree(session);
no_session:
    __free_pages(w->iter.page, get_order(w->iter.len));
    kfree(w);
}

static int add_work(dev_t devno, sector_t sector, struct page_iter *it) {
    struct save_work *w;
    w = kzalloc(sizeof(*w), GFP_KERNEL);
    if (!w) {
        pr_err("out of memory");
        return -ENOMEM;
    }
    memcpy(&w->iter, it, sizeof(*it));
    w->devno = devno;
    w->sector = sector;
    INIT_WORK(&w->work, save_page);
    return queue_work(queue, &w->work);
}

static inline void free_all_pages(struct bio_private_data *p) {
    for (int i = 0; i < p->iter_len; ++i) {
        __free_pages(p->iter->page, get_order(p->iter->len));
    }
}

/**
 * snapshot_save schedules each page or compound one of bio to the workqueue. Each page
 * will be saved to /snapshots/<session id>/<sector no>
 */
void snapshot_save(struct bio *bio) {
    // We completed successfully the read of the region to snapshot, so we
    // can add the whole range to the tree. 
    struct bio_private_data *p = bio->bi_private;
    dev_t dev = bio_devno(bio);
    sector_t sector = bio_sector(bio);
    unsigned long len = bio_len(bio);
    bool added;
    int err = registry_add_range(dev, sector, len, &added);
    if (err || !added) {
        free_all_pages(p);
        return;
    }
    for (int i = 0; i < p->iter_len; ++i) {
        struct page_iter *it = &p->iter[i];
        int err = add_work(dev, sector, it);
        if (err == -ENOMEM) {
            __free_pages(it->page, get_order(it->len));
        }
        sector += (it->len >> 9);
    }
}