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
    struct rhash_head  linkage;
    dev_t              key;
    struct hashset    *set;
    struct rcu_head    rcu;
};

static const struct rhashtable_params sector_set_params = {
    .key_len     = sizeof(sector_t),
    .key_offset  = offsetof(struct sector_obj, key),
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
        pr_debug(pr_format("cannot allocate primary hashtable, got error %d (%s)"), err, errtoa(err));
    } else {
        rcu_assign_pointer(pool, &__pool);
    }
    return err;
}

static inline void session_free(struct session_obj *obj) {
    kfree(obj);
}

static void session_free_rhashtable(void *ptr, void *args) {
    struct session_obj *obj = (struct session_obj*)ptr;
    session_free(obj);
}

void hashset_pool_cleanup(void) {
    struct rhashtable *p = rcu_dereference(pool);
    if (!p) {
        return;
    }
    rcu_assign_pointer(pool, NULL);
    synchronize_rcu();
    rhashtable_free_and_destroy(p, session_free_rhashtable, NULL);
}

int hashset_create(struct hashset *set) {
    int err = rhashtable_init(&set->ht, &sector_set_params);
    if (err) {
        pr_debug(pr_format("cannot allocate secondary hashtable, got error %d"), err);
    }
    return err;
}

static void sector_free_rhashtable(void *ptr, void *args) {
    struct sector_obj *obj = (struct sector_obj*)ptr;
    kfree(obj);
}

static int hashset_remove(dev_t dev, struct session_obj *obj) {
    int err;
    rcu_read_lock();
    struct rhashtable *p = rcu_dereference(pool);
    if (!p) {
        err = -ENOHASHPOOL;
        goto hashset_destroy_out;
    }
    obj = rhashtable_lookup_fast(p, &dev, session_objects_params);
    if (IS_ERR_OR_NULL(obj)) {
        err = -EEXIST;
        goto hashset_destroy_out;
    }
    err = rhashtable_remove_fast(p, &obj->linkage, session_objects_params);
hashset_destroy_out:
    rcu_read_unlock();
    return err;
}

void hashset_destroy(dev_t dev, struct hashset *set) {
    struct session_obj *obj = NULL;
    int err = hashset_remove(dev, obj);
    synchronize_rcu();
    if (!err && obj) {
        kfree(obj);
    }
    rhashtable_free_and_destroy(&set->ht, sector_free_rhashtable, NULL);
}

static inline struct sector_obj *mk_sector_obj(sector_t sector) {
    struct sector_obj *obj = kmalloc(sizeof(struct sector_obj), GFP_KERNEL);
    if (!obj) {
        return ERR_PTR(-ENOMEM);
    }
    obj->key = sector;
    return obj;
}

/**
 * hashset_add puts a sector @param sector to the hashset associated to a certain device @param dev
 * Returns 0 on success, -ENOHASHSET if there is no hashset associated to @param dev, < 0 if some other
 * error occurred during the insertion of the sector in the hashtable (see rhashtable_lookup_insert_fast() for
 * further details).
 */
int hashset_add(dev_t dev, sector_t sector, bool *found) {
    struct sector_obj *obj = mk_sector_obj(sector);
    if (!obj) {
        return -ENOMEM;
    }
    int err = 0;
    *found = false;
    rcu_read_lock();
    struct rhashtable *p = rcu_dereference(pool);
    if (!p) {
        err = -ENOHASHPOOL;
        goto hashset_add_out;
    }
    struct session_obj *session = rhashtable_lookup_fast(p, &dev, session_objects_params);
    if (!session) {
        err = -ENOHASHSET;
        goto hashset_add_out;
    }
    struct hashset *set = session->set;
    err = rhashtable_lookup_insert_fast(&set->ht, &obj->linkage, sector_set_params);
    *found = err == -EEXIST;
    if (err == -EEXIST) {
        err = 0;
        *found = true;
    }
hashset_add_out:
    rcu_read_unlock();
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
    rcu_read_lock();
    int err = 0;
    struct rhashtable *p = rcu_dereference(pool);
    if (!p) {
        pr_debug(pr_format("no pool of hashtables"));
        err = -ENOHASHPOOL;
        goto hashset_register_out;
    }
    err = rhashtable_insert_fast(p, &session->linkage, session_objects_params);
    if (err) {
        pr_debug(pr_format("cannot register hashset for device (%d, %d), got error %d (%s)"), MAJOR(dev), MINOR(dev), err, errtoa(err));
    }
hashset_register_out:
    rcu_read_unlock();
    return err;
}

static void hashset_free_rcu(struct rcu_head *head) {
    struct hashset *set = container_of(head, struct hashset, rcu);
    rhashtable_free_and_destroy(&set->ht, session_free_rhashtable, NULL);
}

static void session_obj_free_rcu(struct rcu_head *head) {
    struct session_obj *obj = container_of(head, struct session_obj, rcu);
    session_free(obj);
}

int hashset_clear(dev_t dev, struct hashset *set) {
    struct session_obj *obj = NULL;
    int err = hashset_remove(dev, obj);
    if (obj) {
        call_rcu(&obj->rcu, session_obj_free_rcu);
    }
    call_rcu(&set->rcu, hashset_free_rcu);
    return err;
}
