#include "iset.h"
#include "pr_format.h"
#include "rbitmap32.h"
#include "registry.h"
#include <linux/printk.h>

int iset_create(struct session *s) {
    int err = rbitmap32_init(&s->iset);
    if (err) {
        pr_err("cannot initialize block hashtable, got error %d", err);
    }
    return err;
}

void iset_destroy(struct session *s) {
    rbitmap32_destroy(&s->iset);
}

/**
 * iset_add puts a sector @param sector to the hashset associated to a certain device @param dev
 * Returns 0 on success, -ENOHASHSET if there is no hashset associated to @param dev, < 0 if some other
 * error occurred during the insertion of the sector in the hashtable (see rhashtable_lookup_insert_fast() for
 * further details).
 */
int iset_add(struct session *s, sector_t sector, bool *added) {
    return rbitmap32_add(&s->iset, lower_32_bits(sector), added);
}

bool iset_lookup(struct session *s, sector_t sector) {
    return rbitmap32_contains(&s->iset, lower_32_bits(sector));
}