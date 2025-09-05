#ifndef AOS_SESSION_H
#define AOS_SESSION_H
#include "../rbitmap/rbitmap32.h"
#include <linux/maple_tree.h>
#include <linux/spinlock.h>
#include <linux/types.h>

struct session {
    dev_t                  dev;
    char                  *id;
    bool                   pending;
    int                    mntpoints;
    struct rbitmap32       iset;
    struct maple_tree      tree;
};

struct session *session_create(dev_t dev);

void session_destroy(struct session *s);

#endif