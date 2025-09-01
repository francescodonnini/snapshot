#ifndef AOS_SESSION_H
#define AOS_SESSION_H
#include <linux/interval_tree.h>
#include <linux/rbtree.h>
#include <linux/rhashtable-types.h>
#include <linux/types.h>

struct session {
    dev_t                  dev;
    char                  *id;
    bool                   has_dir;
    int                    mntpoints;
    spinlock_t             rb_lock;
    struct rb_root_cached  root;
    struct rhashtable      iset;
};

struct session *session_create(dev_t dev);

void session_destroy(struct session *s);

#endif