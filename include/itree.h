#ifndef AOS_ITREE_H
#define AOS_ITREE_H
#include "session.h"
#include <linux/types.h>

void itree_destroy(struct session *s);

int itree_add(struct session *s, sector_t start, unsigned long len, bool *added);

bool itree_subset_of(struct session *s, sector_t start, unsigned long len);

#endif