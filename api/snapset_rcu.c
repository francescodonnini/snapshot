#include "snapset.h"
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

struct session_obj {
    struct hlist_node  list;
    dev_t              dev;
    char              *session;
};

HLIST_HEAD(session_list);
DEFINE_SPINLOCK(lock);

int snapset_init(void) {
    return 0;
}

void snapset_cleanup(void) {}

static struct session_obj* get_session(dev_t dev) {
    rcu_read_lock();
    bool found = false;
    struct session_obj *it;
    hlist_for_each_entry(it, &session_list, list) {
        found = it->dev == dev;
        if (found) {
            break;
        }
    }
    rcu_read_unlock();
    if (found) {
        return it;
    }
    return NULL;
}

bool snapset_add_sector(dev_t dev, sector_t sector) {
    return true;
}

const char *snapset_get_session(dev_t dev) {
    struct session_obj *obj = get_session(dev);
    if (!obj) {
        return NULL;
    }
    return obj->session;
}

static struct session_obj *mk_session(dev_t dev, const char *session) {
    struct session_obj *o = kmalloc(sizeof(struct session_obj), GFP_KERNEL);
    if (!o) {
        return ERR_PTR(-ENOMEM);
    }
    o->dev = dev;
    o->session = session;
    INIT_HLIST_NODE(&o->list);
    return o;
}

static bool lookup_session(dev_t dev) {
    return get_session(dev) != NULL;
}

int snapset_register_session(dev_t dev, const char *session) {
    struct session_obj *o = mk_session(dev, session);
    if (IS_ERR(o)) {
        return PTR_ERR(session);
    }
    unsigned long flags;
    spin_lock_irqsave(&lock, flags);
    int err = 0;
    if (lookup_session(dev)) {
        err = -1;
        goto release_lock;
    }
    hlist_add_head(&o->list, &session_list);
release_lock:
    spin_unlock_irqrestore(&lock, flags);
    return err;
}