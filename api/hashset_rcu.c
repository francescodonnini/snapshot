#include "hashset.h"
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

int hashset_create(struct hashset *set) {
    int err = rhashtable_init(&set->ht, &sector_set_params);
    if (err) {
        pr_debug(pr_format("cannot initialize block hashtable, got error %d"), err);
    }
    return err;
}

static void sector_free_rhashtable(void *ptr, void *args) {
    struct sector_obj *obj = (struct sector_obj*)ptr;
    kfree(obj);
}

void hashset_destroy(struct hashset *set) {
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
int hashset_add(struct hashset *set, dev_t dev, sector_t sector, bool *added) {
    struct sector_obj *obj = mk_sector_obj(sector);
    if (!obj) {
        return -ENOMEM;
    }
    rcu_read_lock();
    pr_debug(pr_format("add item ([%d,%d],%llu) to block table"), MAJOR(dev), MINOR(dev), sector);
    int err = rhashtable_lookup_insert_fast(&set->ht, &obj->linkage, sector_set_params);
    *added = err == 0;
    if (err) {
        pr_debug(
            pr_format("cannot insert item ([%d,%d],%llu) to block table, got error %d (%s)"),
            MAJOR(dev), MINOR(dev), sector, err, errtoa(err));
        err = err == -EEXIST ? 0 : err;
        kfree(obj);
    }
    rcu_read_unlock();
    return err;
}