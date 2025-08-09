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

#define OCTET_SZ (16)
#define ROOT_DIR "/snapshots"

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
    struct dentry *res = vfs_mkdir(mnt_idmap(parent.mnt), d_inode(parent.dentry), dentry, 0660);
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
    int err = mkdir_snapshots();
    if (err) {
        return err;
    }
    return hashset_pool_init();
}

void snapshot_cleanup(void) {
    hashset_pool_cleanup();
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
int snapshot_create(dev_t dev, const char *session, struct hashset *set) {
    int err = mkdir_session(session);
    if (err) {
        return err;
    }
    return hashset_register(dev, set);
}

static inline char *h2a(sector_t sector, char *buffer) {
    sprintf(buffer, "%llx", sector);
    return buffer;
}

static void save_page(const char *path, struct page *page) {
    struct file *fp = filp_open(path, O_CREAT | O_WRONLY, 0600);
    if (IS_ERR(fp)) {
        pr_debug(pr_format("cannot open file: '%s', got error %ld"), path, PTR_ERR(fp));
        return;
    }
    loff_t off;
    void *va = kmap_local_page(page);
    ssize_t n = kernel_write(fp, va, PAGE_SIZE, &off);
    if (n != PAGE_SIZE) {
        pr_debug(pr_format("kernel_write failed to write whole page at '%s'"), path);
    }
    kunmap_local(va);
    int err = filp_close(fp, NULL);
    if (err) {
        pr_debug(pr_format("filp_close failed to close file at '%s', got error %d"), path, err);
    }
}

static void save_bio(char *path, struct bio *bio, const char *parent) {
    dbg_dump_bio("save_bio\n", bio);
    struct bio_private_data *priv = bio->bi_private;
    char octet[OCTET_SZ + 1] = {0};
    sector_t sector = priv->block.sector;
    for (int i = 0; i < priv->block.nr_pages; ++i) {
        path_join(parent, h2a(sector, octet), path);
        save_page(path, priv->block.pages[i]);
        sector += PAGE_SIZE;
    }
}

int snapshot_save(struct bio *bio) {
    char *session = kmalloc(UUID_STRING_LEN + 1, GFP_KERNEL);
    if (!session) {
        pr_debug(pr_format("cannot make enough space to hold session id"));
        return -ENOMEM;
    }
    int err = 0;
    bool found = registry_get_session(bio_devnum(bio), session);
    if (!found) {
        pr_debug(pr_format("cannot find session associated with device %d,%d"), MAJOR(bio_devnum(bio)), MINOR(bio_devnum(bio)));
        err = -EWRONGCRED;
        goto snapshot_save_free_session;
    }
    char *parent = path_join_alloc(ROOT_DIR, session);
    if (!parent) {
        err = -ENOMEM;
        goto snapshot_save_free_session;
    }
    char *path = kmalloc(strlen(parent) + OCTET_SZ + 2, GFP_KERNEL);
    if (!path) {
        err = -ENOMEM;
        goto snapshot_save_free_parent;
    }
    save_bio(path, bio, parent);
    kfree(path);
snapshot_save_free_parent:
    kfree(parent);
snapshot_save_free_session:
    kfree(session);
    return err;
}