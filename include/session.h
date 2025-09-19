#ifndef AOS_SESSION_H
#define AOS_SESSION_H
#include <linux/maple_tree.h>
#include <linux/spinlock.h>
#include <linux/time64.h>
#include <linux/types.h>
#define ENOSSN     5004

struct session {
    struct rcu_head    rcu;
    dev_t              dev;
    struct timespec64  created_on;
    struct maple_tree  tree;
};

int get_dirname_prefix_len(void);

int get_dirname_len(void);

struct session *session_create(dev_t dev);

void session_destroy(struct session *s);

#endif