#ifndef AOS_ITREE_H
#define AOS_ITREE_H
#include "b_range.h"
#include "session.h"
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/spinlock_types.h>
#include <linux/types.h>

int itree_create(struct session *s);

void itree_destroy(struct session *s);

int itree_add(struct session *s, struct b_range *range, bool *added);

bool itree_subset_of(struct session *s, sector_t start, unsigned long len);

#endif