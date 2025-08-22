#ifndef AOS_HASHSET_H
#define AOS_HASHSET_H
#include <linux/rhashtable-types.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#define ENOHASHPOOL 9000
#define ENOHASHSET  9001
#define EDUPHASHSET 9002

struct hashset {
    struct rhashtable ht;
};

int hashset_create(struct hashset *set);

void hashset_destroy(struct hashset *set);

int hashset_add(struct hashset *set, sector_t sector, bool *added);

bool hashset_lookup(struct hashset *set, sector_t sector);

#endif