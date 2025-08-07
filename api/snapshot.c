#include "snapshot.h"
#include "bio.h"
#include "fast_hash.h"
#include "path_utils.h"
#include "pr_format.h"
#include "snapset.h"
#include <linux/bio.h>
#include <linux/bvec.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/list.h>
#include <linux/path.h>
#include <linux/printk.h>

#define OCTET_SZ (16)
#define ROOT_DIR "/snapshots"

static int mkdir_snapshots(void) {
    struct path snapshots_path;
    int err = kern_path(ROOT_DIR, LOOKUP_DIRECTORY, &snapshots_path);
    if (!err) {
        pr_debug(pr_format("directory '%s' already exists"), ROOT_DIR);
        return 0;
    } else if (err) {
        pr_debug(pr_format("kern_path failed on '%s', got error %d"), ROOT_DIR, err);
    }
    struct path root_path;
    err = kern_path("/", LOOKUP_DIRECTORY, &root_path);
    if (err) {
        pr_debug(pr_format("kern_path failed on '/', got error %d"), err);
        return err;
    }
    struct dentry *dentry = lookup_one_len("snapshots", root_path.dentry, strlen("snapshots"));
    if (IS_ERR(dentry)) {
        err = PTR_ERR(dentry);
        pr_debug(pr_format("lookup_one_len failed on 'snapshots', got error %d"), err);
        goto root_path_put;
    }
    struct dentry *res = vfs_mkdir(mnt_idmap(root_path.mnt), d_inode(root_path.dentry), dentry, 0660);
    if (IS_ERR(res)) {
        err = PTR_ERR(res);
        pr_debug(pr_format("vfs_mkdir failed on '%s', got error %d"), ROOT_DIR, err);
    }

    dput(dentry);
root_path_put:
    path_put(&root_path);
    return err;
}

/**
 * snapshot_init -- creates the directory /snapshots if not exists and
 * initializes the necessary data structures used by this subsystem.
 * @returns 0 on success, <0 otherwise
 */
int snapshotfs_init(void) {
    pr_debug(pr_format("begin: mkdir_if_not_exists(%s)"), ROOT_DIR);
    int err = mkdir_snapshots();
    if (err) {
        return err;
    }
    pr_debug(pr_format("directory %s created successfully"), ROOT_DIR);
    return snapset_init();
}

void snapshotfs_cleanup(void) {
    snapset_cleanup();
}

static int try_filp_close(struct file *fp, int err, const char *path) {
    int err2 = filp_close(fp, NULL);
    if (err2) {
        pr_debug(pr_format("cannot close file %s, got error %d"), path, err2);
    }
    if (!err) {
        return err2;
    }
    return err;
}

static int mkdir_session(const char *session) {
    struct path parent_path;
    int err = kern_path(ROOT_DIR, LOOKUP_DIRECTORY, &parent_path);
    if (err) {
        pr_debug(pr_format("'%s' does not exists (error=%d)"), ROOT_DIR, err);
        return err;
    }
    struct dentry *dentry = lookup_one_len(session, parent_path.dentry, strlen(session));
    if (IS_ERR(dentry)) {
        err = PTR_ERR(dentry);
        pr_debug(pr_format("lookup_one_len failed on '%s', got error %d"), session, err);
        goto parent_path_put;
    }
    struct dentry *res = vfs_mkdir(mnt_idmap(parent_path.mnt), d_inode(parent_path.dentry), dentry, 0660);
    if (IS_ERR(res)) {
        err = PTR_ERR(res);
        pr_debug(pr_format("vfs_mkdir failed on '%s/%s', got error %d"), ROOT_DIR, session, err);
    }

    dput(dentry);
parent_path_put:
    path_put(&parent_path);
    return err;
}

/**
 * snapshot_create -- registers a new snapshot session.
 * @param snapshot -- unique identifier of the session
 * @param dev      -- device number of the block device registered by the activate snapshot
 *                    command
 * @returns 0 on success, <0 otherwise.
 */
int snapshot_create(dev_t dev, const char *snapshot) {
    int err = mkdir_session(snapshot);
    if (err) {
        return err;
    }
    return snapset_register_session(dev, snapshot);
}


static int __save_page(const char *path, struct page *page) {
    struct file *fp = filp_open(path, O_CREAT | O_WRONLY, S_IWUSR);
    if (IS_ERR_OR_NULL(fp)) {
        pr_debug(pr_format("cannot open file at %s"), path);
        return PTR_ERR(fp);
    }
    int err = 0;
    loff_t off;
    ssize_t n = kernel_write(fp, page, PAGE_SIZE, &off);
    if (n != PAGE_SIZE) {
        pr_debug(pr_format("kernel_write failed to write whole page at %s"), path);
        err = -1;
    }
    return try_filp_close(fp, err, path);
}

static char *h2a(sector_t sector, char *buffer) {
    sprintf(buffer, "%llx", sector);
    return buffer;
}

static void __save(char *path, struct bio_vec *bvl, struct bvec_iter *iter, const char *parent) {
    char octet[OCTET_SZ + 1] = {0};
    path_join(parent, h2a(iter->bi_sector, octet), path);
    pr_debug(
        pr_format("saving sector=%llu, off=%d, n=%d to %s"),
        iter->bi_sector, bvl->bv_offset, bvl->bv_len, path);
}

static void __save2(char *path, struct bio_vec *bvl, int i, const char *parent) {
    char octet[OCTET_SZ + 1] = {0};
    path_join(parent, h2a(iter->bi_sector, octet), path);
    pr_debug(
        pr_format("saving sector=%llu, off=%d, n=%d to %s"),
        iter->bi_sector, bvl->bv_offset, bvl->bv_len, path);
}

static void __save3(char *path, struct bio_vec *bvl, struct bvec_iter_all *iter, const char *parent) {
    char octet[OCTET_SZ + 1] = {0};
    path_join(parent, h2a(iter->bi_sector, octet), path);
    pr_debug(
        pr_format("saving sector=%llu, off=%d, n=%d to %s"),
        iter->bi_sector, bvl->bv_offset, bvl->bv_len, path);
}


int snapshot_save(dev_t dev, struct bio *bio) {
    const char *session = snapset_get_session(dev);
    if (!session) {
        pr_debug(
            pr_format("cannot find session associated with device %d,%d"),
            MAJOR(dev), MINOR(dev));
        return -1;
    }
    char *prefix = path_join_alloc(ROOT_DIR, session);
    if (!prefix) {
        return -ENOMEM;
    }
    int err = 0;
    char *path = kmalloc(strlen(prefix) + OCTET_SZ + 2, GFP_KERNEL);
    if (!path) {
        err = -ENOMEM;
        goto free_prefix;
    }
    dbg_dump_bio("snapshot_save\n", bio);
    struct bio_vec bvl;
    struct bvec_iter_all iter;
    bio_for_each_segment_all(bvl, bio, iter) {
        __save(path, &bvl, &iter, prefix);
    }
    kfree(path);
free_prefix:
    kfree(prefix);
    return err;
}