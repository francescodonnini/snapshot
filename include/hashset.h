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

int hashset_pool_init(void);

void hashset_pool_cleanup(void);

int hashset_create(struct hashset *set);

void hashset_destroy(dev_t dev, struct hashset *set);

int hashset_add(dev_t dev, sector_t sector, bool *found);

int hashset_register(dev_t dev, struct hashset *session);

#endif