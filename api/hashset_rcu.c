#include "hashset.h"
#include "pr_format.h"
#include <linux/printk.h>
#include <linux/rhashtable.h>
#include <linux/slab.h>

struct sector_obj {
    struct rhash_head linkage;
    sector_t          key;
};

struct session_obj {
    struct rhash_head linkage;
    dev_t             key;
    struct hashset    *set;
};

static const struct rhashtable_params sector_set_params = {
    .key_len    = sizeof(sector_t),
    .key_offset = offsetof(struct sector_obj, key),
    .head_offset = offsetof(struct sector_obj, linkage),
}; 

static const struct rhashtable_params session_objects_params = {
    .key_len     = sizeof(dev_t),
    .key_offset  = offsetof(struct session_obj, key),
    .head_offset = offsetof(struct session_obj, linkage),
};

struct rhashtable  __pool;
struct rhashtable *pool = NULL;

int hashset_pool_init(void) {
    int err = rhashtable_init(&__pool, &session_objects_params);
    if (err) {
        pr_debug(pr_format("cannot allocate primary hashtable, got error %d"), err);
    } else {
        rcu_assign_pointer(pool, &__pool);
    }
    return err;
}

static void hashset_free(void *ptr, void *args) {
    struct sector_obj *obj = (struct sector_obj*)ptr;
    pr_debug(pr_format("hashset_free called on %llu"), obj->key);
    kfree(obj);    
}

static void session_free(void *ptr, void *args) {
    struct session_obj *obj = (struct session_obj*)ptr;
    struct hashset *set = obj->set;
    rcu_assign_pointer(obj->set, NULL);
    synchronize_rcu();
    rhashtable_free_and_destroy(&set->ht, hashset_free, NULL);
}

void hashset_pool_cleanup(void) {
    struct rhashtable *p = pool;
    if (!p) {
        return;
    }
    rcu_assign_pointer(pool, NULL);
    synchronize_rcu();
    rhashtable_free_and_destroy(p, session_free, NULL);    
}

struct hashset *hashset_create(void) {
    struct hashset *set = kmalloc(sizeof(struct hashset), GFP_KERNEL);
    if (!set) {
        pr_debug(pr_format("cannot make enough space for struct hashset"));
        return ERR_PTR(-ENOMEM);
    }
    int err = rhashtable_init(&set->ht, &sector_set_params);
    if (err) {
        pr_debug(pr_format("cannot allocate secondary hashtable, got error %d"), err);
        kfree(set);
        return ERR_PTR(err);
    }
    return set;
}

void hashset_destroy(dev_t dev) {
    rcu_read_lock();
    struct rhashtable *htp = pool;
    struct session_obj *obj = rhashtable_lookup_fast(htp, &dev, session_objects_params);
    if (IS_ERR_OR_NULL(obj)) {
        goto hashset_destroy_out;
    }
    struct hashset *set = obj->set;
    int err = rhashtable_remove_fast(&set->ht, &obj->linkage, sector_set_params);
    if (err) {
        goto hashset_destroy_out;
    }
    synchronize_rcu();
    kfree(obj);
hashset_destroy_out:
    rcu_read_unlock();
}

static inline struct sector_obj *mk_sector_obj(sector_t sector) {
    struct sector_obj *obj = kmalloc(sizeof(struct sector_obj), GFP_KERNEL);
    if (!obj) {
        return ERR_PTR(-ENOMEM);
    }
    obj->key = sector;
    return obj;
}

static inline int hashset_lookup(struct rhashtable *tbl, sector_t sector, bool *found) {
    int err = 0;
    struct sector_obj *obj = rhashtable_lookup_fast(tbl, &sector, sector_set_params);
    if (IS_ERR(obj)) {
        err = PTR_ERR(obj);
        goto lookup_error;
    }
    *found = obj != NULL;
lookup_error:
    return err;
}

/**
 * hashset_add puts a sector @param sector to the hashset associated to a certain device @param dev
 * Returns 0 on success, -ENOHASHSET if there is no hashset associated to @param dev, < 0 if some other
 * error occurred during the insertion of the sector in the hashtable (see rhashtable_lookup_insert_fast() for
 * further details).
 */
int hashset_add(dev_t dev, sector_t sector, bool *found) {
    pr_debug(pr_format("hashset_add(%d, %llu)"), dev, sector);
    int err = 0;
    *found = false;
    rcu_read_lock();
    struct rhashtable *htp = pool;
    if (!htp) {
        err = -ENOHASHPOOL;
        goto out;
    }
    struct session_obj *session = rhashtable_lookup_fast(htp, &dev, session_objects_params);
    if (!session) {
        err = -ENOHASHSET;
        goto out;
    }
    struct sector_obj *obj = mk_sector_obj(sector);
    if (!obj) {
        err = -ENOMEM;
        goto out;
    }
    struct rhashtable *set = &(session->set->ht);
    err = rhashtable_lookup_insert_fast(set, &obj->linkage, sector_set_params);
    *found = err == -EEXIST;
    if (err == -EEXIST) {
        err = 0;
        *found = true;
    }
out:
    rcu_read_unlock();
    pr_debug(pr_format("hashset_add => err=%d,found=%d"), err, *found);
    return err;
}

static inline struct session_obj *mk_session_obj(dev_t dev, struct hashset *set) {
    struct session_obj *obj = kmalloc(sizeof(struct session_obj), GFP_KERNEL);
    if (!obj) {
        pr_debug(pr_format("cannot allocate enough space for session(%d)"), dev);
        return ERR_PTR(-ENOMEM);
    }
    obj->key = dev;
    obj->set = set;
    pr_debug(pr_format("mk_session(%d)"), dev);
    return obj;
}

/**
 * hashset_register inserts an hashset in the common hashtable pool.
 * Returns 0 on success, -1 if an hashset associated with the device number @param dev has been already registered,
 * another number < 0 if some other kind of error occurs (see rhashtable_insert_fast for further details).
 */
int hashset_register(dev_t dev, struct hashset *set) {
    struct session_obj *session = mk_session_obj(dev, set);
    if (IS_ERR(session)) {
        return PTR_ERR(session);
    }    
    struct rhashtable *htp = pool;
    if (!htp) {
        pr_debug(pr_format("no pool of hashtables"));
        return -1;
    }
    int err = rhashtable_insert_fast(htp, &session->linkage, session_objects_params);
    if (err == -EEXIST) {
        pr_debug(pr_format("hashset associated to device %d has been already registered"), dev);
    } else if (err) {
        pr_debug(pr_format("cannot register hashset for device %d, got error %ld"), dev, PTR_ERR(session));
    }
    return err;
}
