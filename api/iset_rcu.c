#include "iset.h"
#include "pr_format.h"
#include "registry.h"
#include <linux/printk.h>
#include <linux/rhashtable.h>
#include <linux/slab.h>

struct sector_obj {
    struct rhash_head linkage;
    sector_t          key;
};

static const struct rhashtable_params sector_set_params = {
    .key_len     = sizeof(sector_t),
    .key_offset  = offsetof(struct sector_obj, key),
    .head_offset = offsetof(struct sector_obj, linkage),
}; 

int iset_create(struct session *s) {
    int err = rhashtable_init(&s->iset, &sector_set_params);
    if (err) {
        pr_err("cannot initialize block hashtable, got error %d", err);
    }
    return err;
}

static void sector_free_rhashtable(void *ptr, void *args) {
    struct sector_obj *obj = (struct sector_obj*)ptr;
    kfree(obj);
}

void iset_destroy(struct session *s) {
    rhashtable_free_and_destroy(&s->iset, sector_free_rhashtable, NULL);
}

static inline struct sector_obj *mk_sector_obj(sector_t sector) {
    struct sector_obj *obj = kzalloc(sizeof(struct sector_obj), GFP_KERNEL);
    if (!obj) {
        return NULL;
    }
    obj->key = sector;
    return obj;
}

/**
 * iset_add puts a sector @param sector to the hashset associated to a certain device @param dev
 * Returns 0 on success, -ENOHASHSET if there is no hashset associated to @param dev, < 0 if some other
 * error occurred during the insertion of the sector in the hashtable (see rhashtable_lookup_insert_fast() for
 * further details).
 */
int iset_add(struct session *s, sector_t sector, bool *added) {
    struct sector_obj *obj = mk_sector_obj(sector);
    if (!obj) {
        return -ENOMEM;
    }
    rcu_read_lock();
    int err = rhashtable_lookup_insert_fast(&s->iset, &obj->linkage, sector_set_params);
    if (added) {
        *added = err == 0;
    }
    if (err) {
        err = err == -EEXIST ? 0 : err;
        kfree(obj);
    }
    rcu_read_unlock();
    return err;
}

bool iset_lookup(struct session *s, sector_t sector) {
    rcu_read_lock();
    bool found = rhashtable_lookup(&s->iset, &sector, sector_set_params) != NULL;
    rcu_read_unlock();
    return found;
}