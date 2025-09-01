#ifndef AOS_SESSION_H
#define AOS_SESSION_H
#include <linux/rhashtable-types.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/types.h>

struct session {
    dev_t                  dev;
    char                  *id;
    bool                   has_dir;
    int                    mntpoints;
    struct rhashtable      iset;
    struct rb_root_cached  rb_root;
    spinlock_t             rb_lock;
};

struct session *session_create(dev_t dev);

void session_destroy(struct session *s);

#endif