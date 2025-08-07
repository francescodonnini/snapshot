#ifndef AOS_SNAPSET_H
#define AOS_SNAPSET_H
#include <linux/types.h>

int snapset_init(void);

void snapset_cleanup(void);

bool snapset_add_sector(dev_t dev, sector_t sector);

const char *snapset_get_session(dev_t dev);

int snapset_register_session(dev_t dev, const char *session);
#endif