#ifndef AOS_SESSION_H
#define AOS_SESSION_H
#include <linux/maple_tree.h>
#include <linux/rhashtable.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#define ENOSSN     5004

struct session {
    dev_t                  dev;
    char                  *id;
    struct timespec64      created_on;
    bool                   pending;
    int                    mntpoints;
    struct maple_tree      tree;
};

int get_session_id_len(void);

struct session *session_create(dev_t dev);

void session_destroy(struct session *s);

#endif