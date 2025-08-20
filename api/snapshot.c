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
#include <linux/workqueue.h>

#define OCTET_SZ (16)
#define ROOT_DIR "/snapshots"

struct save_work {
    struct work_struct  work;
    const char         *path;
    struct page_iter   *iter;
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
        pr_debug(pr_format("'%s' does not exists, got error %d (%s)"), ROOT_DIR, err, errtoa(err));
        return err;
    }
    struct inode *parent_ino = d_inode(parent.dentry);
    inode_lock(parent_ino);
    struct dentry *dentry = lookup_one_len(session, parent.dentry, strlen(session));
    if (IS_ERR(dentry)) {
        err = PTR_ERR(dentry);
        pr_debug(pr_format("lookup_one_len failed on '%s', got error %d (%s)"), session, err, errtoa(err));
        goto snapshots_path_put;
    }
    struct dentry *res = vfs_mkdir(mnt_idmap(parent.mnt), d_inode(parent.dentry), dentry, 0660);
    if (IS_ERR(res)) {
        err = PTR_ERR(res);
        pr_debug(pr_format("vfs_mkdir failed on '%s/%s', got error %d (%s)"), ROOT_DIR, session, err, errtoa(err));
    }
    dput(dentry);
snapshots_path_put:
    inode_unlock(parent_ino);
    path_put(&parent);
    return err;
}

/**
 * snapshot_create -- registers a new snapshot session.
 * @param snapshot -- unique identifier of the session
 * @param dev      -- device number of the block device registered by the activate snapshot
 *                    command
 * @returns 0 on success, <0 otherwise.
 */
int snapshot_create(const char *session) {
    return mkdir_session(session);
}

static void save_page(struct work_struct *work) {
    struct save_work *w = container_of(work, struct save_work, work);
    const char *path = w->path;
    struct page_iter *it = w->iter;
    struct file *fp = filp_open(path, O_CREAT | O_WRONLY, 0600);
    if (IS_ERR(fp)) {
        pr_debug(pr_format("cannot open file: '%s', got error %ld"), path, PTR_ERR(fp));
        goto save_page_out;
    }
    loff_t off;
    void *va = kmap_local_page(it->page);
    ssize_t n = kernel_write(fp, va + it->offset, it->len, &off);
    if (n != PAGE_SIZE) {
        pr_debug(pr_format("kernel_write failed to write whole page at '%s'"), path);
    }
    kunmap_local(va);
    int err = filp_close(fp, NULL);
    if (err) {
        pr_debug(pr_format("filp_close failed to close file at '%s', got error %d"), path, err);
    }
save_page_out:
    kfree(work);
    kfree(path);
    __free_page(it->page);
}

static int add_work(const char *path, struct page_iter *it) {
    struct save_work *w;
    w = kzalloc(sizeof(*w), GFP_KERNEL);
    if (!w) {
        return -ENOMEM;
    }
    w->path = path;
    w->iter = it;
    INIT_WORK(&w->work, save_page);
    return queue_work(queue, &w->work);
}

static inline char *ltoa(sector_t sector, char *buffer) {
    sprintf(buffer, "%lld", sector);
    return buffer;
}

static void save_bio(struct bio_private_data *p, const char *parent) {
    char octet[OCTET_SZ + 1] = {0};
    sector_t sector = p->sector;
    for (int i = 0; i < p->nr_pages; ++i, sector += PAGE_SIZE) {
        struct page_iter *it = &p->iter[i];
        char *path = kzalloc(strlen(parent) + OCTET_SZ + 2, GFP_KERNEL);
        if (!path) {
            pr_debug(pr_format("cannot allocate enough space for path"));
            __free_page(it->page);
            continue;
        }
        path_join(parent, ltoa(sector, octet), path);
        int err = add_work(path, it);
        if (err == -ENOMEM) {
            pr_debug(pr_format("add_work failed for file='%s', sector=%llu"), path, sector);
            __free_page(it->page);
            kfree(path);
        }
    }
}

int snapshot_save(struct bio *bio) {
    char *session = kzalloc(UUID_STRING_LEN + 1, GFP_KERNEL);
    if (!session) {
        pr_debug(pr_format("cannot make enough space to hold session id"));
        return -ENOMEM;
    }
    int err = 0;
    bool found = registry_get_session_id(bio_devno(bio), session);
    if (!found) {
        pr_debug(pr_format("cannot find session associated with device %d,%d"), MAJOR(bio_devno(bio)), MINOR(bio_devno(bio)));
        err = -EWRONGCRED;
        goto snapshot_save_free_session;
    }
    char *parent = path_join_alloc(ROOT_DIR, session);
    if (!parent) {
        err = -ENOMEM;
        goto snapshot_save_free_session;
    }
    save_bio(bio->bi_private, parent);
    kfree(parent);
snapshot_save_free_session:
    kfree(session);
    return err;
}