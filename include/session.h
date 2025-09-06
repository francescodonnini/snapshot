#ifndef AOS_SESSION_H
#define AOS_SESSION_H
#include "../rbitmap/rbitmap32.h"
#include <linux/maple_tree.h>
#include <linux/rhashtable.h>
#include <linux/spinlock.h>
#include <linux/time64.h>
#include <linux/types.h>

struct session {
    dev_t                  dev;
    char                  *id;
    struct timespec64      created_on;
    bool                   pending;
    int                    mntpoints;
    struct rhashtable      iset;
    struct maple_tree      tree;
};

struct session *session_create(dev_t dev);

void session_destroy(struct session *s);

#endif