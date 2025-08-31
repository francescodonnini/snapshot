#ifndef AOS_SESSION_H
#define AOS_SESSION_H
#include "hashset.h"
#include <linux/types.h>

struct session {
    dev_t           dev;
    char           *id;
    struct hashset  hashset;
    bool            has_dir;
    int             mntpoints;
};

struct session *session_create(dev_t dev);

void session_destroy(struct session *s);

#endif