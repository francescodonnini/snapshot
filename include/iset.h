#ifndef AOS_ISET_H
#define AOS_ISET_H
#include "session.h"

#define ENOHASHPOOL 9000
#define ENOHASHSET  9001
#define EDUPHASHSET 9002

int iset_create(struct session *s);

void iset_destroy(struct session *s);

int iset_add(struct session *s, sector_t sector, bool *added);

bool iset_lookup(struct session *s, sector_t sector);

#endif