#include "snapshot.h"
#include "bio_utils.h"
#include "fast_hash.h"
#include "path_utils.h"
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
#include <linux/workqueue.h>

#define OCTET_SZ (16)
#define ROOT_DIR "/snapshots"

struct save_work {
    struct work_struct  work;
    dev_t               devno;
    sector_t            sector;
    struct page_iter    iter;
};

static struct workqueue_struct *queue;

static int mkdir_snapshots(void) {
    struct path path;
    int err = kern_path(ROOT_DIR, LOOKUP_DIRECTORY, &path);
    if (!err) {
        pr_debug(pr_format("directory '%s' already exists"), ROOT_DIR);
        path_put(&path);
        return 0;
    }
    struct path parent;
    err = kern_path("/", LOOKUP_DIRECTORY, &parent);
    if (err) {
        pr_debug(pr_format("kern_path failed on '/', got error %d (%s)"), err, errtoa(err));
        return err;
    }
    struct inode *parent_ino = d_inode(parent.dentry);
    inode_lock(parent_ino);
    struct dentry *dentry = lookup_one_len("snapshots", parent.dentry, strlen("snapshots"));
    if (IS_ERR(dentry)) {
        err = PTR_ERR(dentry);
        pr_debug(pr_format("lookup_one_len failed on 'snapshots', got error %d (%s)"), err, errtoa(err));
        goto parent_put;
    }
    struct dentry *res = vfs_mkdir(mnt_idmap(parent.mnt), d_inode(parent.dentry), dentry, 0664);
    if (IS_ERR(res)) {
        err = PTR_ERR(res);
        pr_debug(pr_format("vfs_mkdir failed on '%s', got error %d (%s)"), ROOT_DIR, err, errtoa(err));
    }

    dput(dentry);
parent_put:
    inode_unlock(parent_ino);
    path_put(&parent);
    return err;
}

/**
 * snapshot_init -- creates the directory /snapshots if not exists and
 * initializes the necessary data structures used by this subsystem.
 * @returns 0 on success, <0 otherwise
 */
int snapshot_init(void) {
    queue = create_workqueue("save_files_wq");
    if (!queue) {
        pr_debug(pr_format("cannot create workqueue to save file(s)"));
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

static int mkdir_session(const char *session) {
    struct path parent;
    int err = kern_path(ROOT_DIR, LOOKUP_DIRECTORY, &parent);
    if (err) {
        pr_debug(pr_format("'%s' does not exists, got error %d (%s)"),
                 ROOT_DIR, err, errtoa(err));
        return err;
    }

    struct inode *parent_ino = d_inode(parent.dentry);
    inode_lock(parent_ino);
    struct dentry *dentry = lookup_one_len(session, parent.dentry, strlen(session));
    if (IS_ERR(dentry)) {
        err = PTR_ERR(dentry);
        pr_debug(pr_format("lookup_one_len failed on '%s', got error %d (%s)"),
                 session, err, errtoa(err));
        goto out_unlock_put;
    }
    struct dentry *res = vfs_mkdir(mnt_idmap(parent.mnt), d_inode(parent.dentry), dentry, 0664);
    if (IS_ERR(res)) {
        err = PTR_ERR(res);
        pr_debug(pr_format("vfs_mkdir failed on '%s/%s', got error %d (%s)"),
                 ROOT_DIR, session, err, errtoa(err));
    }
    dput(dentry);
out_unlock_put:
    inode_unlock(parent_ino);
    path_put(&parent);
    return err;
}

static char *create_path(const char *session, sector_t sector) {
    size_t n = strlen("/snapshots/") + strlen(session) + OCTET_SZ + 2;
    char *path = kzalloc(n, GFP_KERNEL);
    if (!path) {
        pr_debug(pr_format("out of memory"));
        return NULL;
    }
    sprintf(path, "/snapshots/%s/%lld", session, sector);
    return path;
}

static void save_page(struct work_struct *work) {
    char *session = kzalloc(UUID_STRING_LEN + 1, GFP_KERNEL);
    if (!session) {
        goto no_session;
    }
    bool has_dir;
    struct save_work *w = container_of(work, struct save_work, work);
    if (!registry_get_session_id(w->devno, session, &has_dir)) {
        goto no_session;
    }
    if (!has_dir) {
        if (mkdir_session(session)) {
            goto free_session;
        } else {
            registry_update_dir(w->devno, session);
        }
    }
    char *path = create_path(session, w->sector);
    if (!path) {
        goto free_session;
    }
    struct page_iter *it = &w->iter;
    struct file *fp = filp_open(path, O_CREAT | O_WRONLY, 0664);
    if (IS_ERR(fp)) {
        pr_debug(pr_format("cannot open file: '%s', got error %ld"), path, PTR_ERR(fp));
        goto no_file;
    }
    if (it->offset + it->len > PAGE_SIZE) {
        pr_debug(pr_format("out of page bound: [%d, %d)"), it->offset, it->len);
        goto no_file;
    }
    void *va = kmap_local_page(it->page);
    loff_t off = 0;
    ssize_t n = kernel_write(fp, va + it->offset, it->len, &off);
    kunmap_local(va);
    if (n != it->len) {
        pr_debug(pr_format("kernel_write failed to write whole page at '%s'"), path);
    }
    int err = filp_close(fp, NULL);
    if (err) {
        pr_debug(pr_format("filp_close failed to close file at '%s', got error %d"), path, err);
    }
no_file:
    kfree(path);
free_session:
    kfree(session);
no_session:
    __free_page(it->page);
    kfree(work);
}

static int add_work(dev_t devno, sector_t sector, struct page_iter *it) {
    struct save_work *w;
    w = kzalloc(sizeof(*w), GFP_KERNEL);
    if (!w) {
        pr_debug(pr_format("add_work failed for device=%d:%d, sector=%llu"), MAJOR(devno), MINOR(devno), sector);
        return -ENOMEM;
    }
    memcpy(&w->iter, it, sizeof(*it));
    w->devno = devno;
    w->sector = sector;
    INIT_WORK(&w->work, save_page);
    return queue_work(queue, &w->work);
}

void snapshot_save(struct bio *bio) {
    dev_t dev = bio_devno(bio);
    sector_t sector = bio_sector(bio);
    struct bio_private_data *p = bio->bi_private;
    for (int i = 0; i < p->nr_pages; ++i) {
        struct page_iter *it = &p->iter[i];
        int err = add_work(dev, sector, it);
        if (err == -ENOMEM) {
            __free_page(it->page);
        }
        sector += it->len;
    }
}