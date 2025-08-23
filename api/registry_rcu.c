#include "registry.h"
#include "fast_hash.h"
#include "hash.h"
#include "hashset.h"
#include "pr_format.h"
#include "session.h"
#include "snapshot.h"
#include <linux/blkdev.h>
#include <linux/list.h>
#include <linux/printk.h>
#include <linux/rculist.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/uuid.h>

#define SHA1_HASH_LEN (20)

struct session_current {
    struct session *ptr;
};

// All snapshot metadata are stored in a doubly-linked list
struct snapshot_metadata {
    struct list_head  list;
    // speed up searches by making string comparisons only on collisions or matches
    unsigned long           dev_name_hash; 
    char                   *dev_name;
    char                   *password;
    struct session_current  session;
    struct rcu_head         rcu;
};

DEFINE_SPINLOCK(write_lock);
LIST_HEAD(registry_db);

/**
 * registry_init initializes all necessary data structures to manage snapshots credentials
 * @return always 0
 */
int registry_init(void) {
    return 0;
}

/**
 * registry_cleanup deallocates all the heap-allocated data structures used by this subsystem
 */
void registry_cleanup(void) {
    LIST_HEAD(old_head);
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    list_splice_init(&registry_db, &old_head);
    spin_unlock_irqrestore(&write_lock, flags);
    synchronize_rcu();
    struct snapshot_metadata *it, *tmp;
    list_for_each_entry_safe(it, tmp, &old_head, list) {
        struct session *s = it->session.ptr;
        if (s) {
            session_destroy(s);
        }
        kfree(it);
    }
}

static inline bool registry_lookup_rcu(bool(*pred)(struct snapshot_metadata*, const void *args), const void *args) {
    struct snapshot_metadata *it;
    bool b = false;
    rcu_read_lock();
    list_for_each_entry_rcu(it, &registry_db, list) {
        b = pred(it, args);
        if (b) {
            break;
        }
    }
    rcu_read_unlock();
    return b;
}

static inline bool by_dev_name(struct snapshot_metadata *it, const void *args) {
    struct snapshot_metadata *node = (struct snapshot_metadata*)args;
    return it->dev_name_hash == node->dev_name_hash
           && !strcmp(it->dev_name, node->dev_name);
}

/**
 * try_add adds a snapshot_metadata inside the registry database if another entity
 * with the same name does not already exist. It assumes that the lock is held.
 * @param ep the entity to be added
 * @return 0 if the insertion was sucessfull, -EDUPNAME otherwise
 */
static int try_add(struct snapshot_metadata *ep) {
    if (registry_lookup_rcu(by_dev_name, ep)) {
        return -EDUPNAME;
    }
    list_add_rcu(&ep->list, &registry_db);
    return 0;
}

/**
 * helper function that initializes the pointer of password to the correct addresses.
 */
static inline struct snapshot_metadata *node_alloc_noname(gfp_t gfp) {
    struct snapshot_metadata *node;
    // size is the number of bytes needed by snapshot metadata + the number of bytes needed to hold the password hash
    size_t size = sizeof(struct snapshot_metadata)
                  + ALIGN(SHA1_HASH_LEN, sizeof(void*));
    node = kzalloc(size, gfp);
    if (!node) {
        return NULL;
    }
    node->password = (char*)node + sizeof(*node);
    return node;
}

static inline struct snapshot_metadata *node_alloc(const char *name, gfp_t gfp) {
    struct snapshot_metadata *node = node_alloc_noname(gfp);
    if (!node) {
        return NULL;
    }
    size_t n = strlen(name) + 1;
    node->dev_name = kzalloc(n, gfp);
    if (!node->dev_name) {
        kfree(node);
        return NULL;
    }
    strscpy(node->dev_name, name, n);
    node->dev_name_hash = fast_hash(name);
    return node;
}

/**
 * mk_node allocates memory for a struct snapshot_metadata and initializes some of its fields:
 * it copies dev_name and the hash of password to the newly allocated memory area, and it stores the hash of dev_name.
 * It returns:
 * * -ETOOBIG if dev_name is too long to represent a valid file path;
 * * -ENOMEM if kmalloc failed to allocate enough memory to hold struct snapshot_metadata
 * *  other errors if it was not possibile to compute the cryptographic hash function of password (see 'hash' and 'hash_alloc' for details)
 * *  a pointer to the newly allocated memory area otherwise.
 */
static struct snapshot_metadata* mk_node(const char *dev_name, const char *password) {
    int err;
    size_t n = strnlen(dev_name, PATH_MAX);
    if (n == PATH_MAX) {
        err = -ETOOBIG;
        goto no_node;
    }
    struct snapshot_metadata *node = node_alloc(dev_name, GFP_KERNEL);
    if (node == NULL) {
        err = -ENOMEM;
        goto no_node;
    }
    err = hash("sha1", password, strlen(password), node->password, SHA1_HASH_LEN);
    if (err) {
        goto no_hash;
    }
    return node;

no_hash:
    kfree(node->dev_name);
    kfree(node);
no_node:
    return ERR_PTR(err);
}

/**
 * registry_insert tries to register a device/image file. It returns 0 on success, <0 otherwise.
 */
int registry_insert(const char *dev_name, const char *password) {
    struct snapshot_metadata *node = mk_node(dev_name, password);
    if (IS_ERR(node)) {
        return PTR_ERR(node);
    }
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    int err = try_add(node);
    spin_unlock_irqrestore(&write_lock, flags);
    if (err) {
        kfree(node);
    }
    return err;
}

/**
 * node_free_rcu release all the resources associated with a certain snapshot. It destroy a session and should be called only when a node
 * is removed from the list (not updated).
 */
static void node_free_rcu(struct rcu_head *head) {
    struct snapshot_metadata *node = container_of(head, struct snapshot_metadata, rcu);
    struct session *s = node->session.ptr;
    if (s) {
        session_destroy(s);
    }
    kfree(node->dev_name);
    kfree(node);
}

/**
 * get_by_name search a node whose device name is equal to name. It assumes it's called inside
 * a RCU critical section or a while a spinlock is held.
 */
static inline struct snapshot_metadata *get_by_name(const char *name) {
    unsigned long name_hash = fast_hash(name);
    struct snapshot_metadata *it;
    list_for_each_entry_rcu(it, &registry_db, list) {
        if (name_hash == it->dev_name_hash && !strcmp(name, it->dev_name)) {
            return it;
        }
    }
    return NULL;
}

/**
 * get_by_dev search a node whose device number is equal to dev. It assumes it's called inside
 * a RCU critical section or a while a spinlock is held.
 */
static inline struct snapshot_metadata *get_by_dev(dev_t dev) {
    struct snapshot_metadata *it;
    list_for_each_entry_rcu(it, &registry_db, list) {
        struct session *s = it->session.ptr;
        if (s && s->dev == dev) {
            return it;
        }
    }
    return NULL;
}


/**
 * registry_delete deletes the credentials associated with the block-device dev_name
 * @param dev_name the name of the block-device
 * @param password the password protecting the snapshot
 * @return -EWRONGCRED if the password or the device name are wrong, 0 otherwise 
 */
int registry_delete(const char *dev_name, const char *password) {
    char *buffer = hash_alloc("SHA1", password, strlen(password));
    if (IS_ERR(buffer)) {
        return PTR_ERR(buffer);
    }

    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    struct snapshot_metadata *it = get_by_name(dev_name);
    int err = -EWRONGCRED;
    if (it && !memcmp(buffer, it->password, SHA1_HASH_LEN)) {
        list_del_rcu(&it->list);
        err = 0;
    }
    spin_unlock_irqrestore(&write_lock, flags);
    
    kfree(buffer);
    if (!err) {
        call_rcu(&it->rcu, node_free_rcu);
    }
    return err;
}

static inline bool is_active(struct snapshot_metadata *it, const void *args) {
    dev_t *dev = (dev_t*)args;
    struct session *s = it->session.ptr;
    return s && s->dev == *dev;
}

bool registry_lookup_active(dev_t dev) {
    return registry_lookup_rcu(is_active, (void*)&dev);
}

static void create_session_rcu(struct rcu_head *head) {
    struct snapshot_metadata *node = container_of(head, struct snapshot_metadata, rcu);
    kfree(node);
}

/**
 * registry_create_session updates a previously registered image/device with the associated device number
 * @param dev_name - the path of the image associated with the device
 * @param dev      - the device number associated to the device
 * @returns 0 on success, <0 otherwise.
 */
int registry_create_session(const char *dev_name, dev_t dev) {
    struct snapshot_metadata *new_node = node_alloc_noname(GFP_KERNEL);
    if (!new_node) {
        return -ENOMEM;
    }
    int err;
    struct session *session = session_create(dev);
    if (!session) {
        err = -ENOMEM;
        goto no_session;
    }
    new_node->session.ptr = session;
    
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    struct snapshot_metadata *old_node = get_by_name(dev_name);
    if (!old_node) {
        pr_debug(pr_format("cannot find device=%s"), dev_name);
        err = -EWRONGCRED;
        goto wrong_credentials;
    }

    new_node->dev_name = old_node->dev_name;
    new_node->dev_name_hash = old_node->dev_name_hash;
    memcpy(new_node->password, old_node->password, SHA1_HASH_LEN);

    pr_debug(pr_format("device=%s;dev=%d,%d;uuid=%s"), dev_name, MAJOR(session->dev), MINOR(session->dev), session->id);
    
    list_replace_rcu(&old_node->list, &new_node->list);
    spin_unlock_irqrestore(&write_lock, flags);
    
    call_rcu(&old_node->rcu, create_session_rcu);
    return err;

wrong_credentials:
    spin_unlock_irqrestore(&write_lock, flags);
no_session:
    kfree(new_node->dev_name);
    kfree(new_node);
    return err;
}

/**
 * registry_add_sector adds a sector to a session associate to a device number dev. The parameter added is optional
 * and if it's not NULL, then registry_add_sector writes false to it if the sector wasn't previously registered in the
 * corresponding hashset.
 */
int registry_add_sector(dev_t dev, sector_t sector, bool *added) {
    rcu_read_lock();
    bool registered = false;
    struct snapshot_metadata *it;
    struct session *s = NULL;
    list_for_each_entry_rcu(it, &registry_db, list) {
        s = it->session.ptr;
        registered = s && s->dev == dev;
        if (registered) {
            break;
        }
    }
    int err;
    if (!registered) {
        err = -ENOSSN;
        goto out;
    }
    if (added) {
        *added = false;
    }
    err = hashset_add(&s->hashset, sector, added);
out:
    rcu_read_unlock();
    return err;
}

int registry_lookup_sector(dev_t dev, sector_t sector, bool *present) {
    rcu_read_lock();
    bool registered = false;
    struct snapshot_metadata *it;
    struct session *s = NULL;
    list_for_each_entry_rcu(it, &registry_db, list) {
        s = it->session.ptr;
        registered = s && s->dev == dev;
        if (registered) {
            break;
        }
    }
    *present = false;
    int err;
    if (!registered) {
        err = -ENOSSN;
    } else {
        err = 0;
        *present = hashset_lookup(&s->hashset, sector);
    }
    rcu_read_unlock();
    return err;
}

bool registry_get_session_id(dev_t dev, char *id, bool *has_dir) {
    rcu_read_lock();
    bool found = false;
    struct snapshot_metadata *it;
    list_for_each_entry_rcu(it, &registry_db, list) {
        struct session *s = it->session.ptr;
        found = s && s->dev == dev;
        if (found) {
            strncpy(id, s->id, UUID_STRING_LEN + 1);
            *has_dir = s->has_dir;
            break;
        }
    }
    rcu_read_unlock();
    return found;
}

static void end_session_rcu(struct rcu_head *head) {
    struct snapshot_metadata *node = container_of(head, struct snapshot_metadata, rcu);
    struct session *s = node->session.ptr;
    if (s) {
        session_destroy(s);
    }
    kfree(node);
}

/**
 * registry_end_session detaches the current session from the node associated
 * to the device number dev.
 */
void registry_end_session(dev_t dev) {
    struct snapshot_metadata *new_node = node_alloc_noname(GFP_KERNEL);
    if (!new_node) {
        pr_debug(pr_format("out of memory"));
        return;
    }

    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    bool found = false;
    struct snapshot_metadata *it;
    list_for_each_entry_rcu(it, &registry_db, list) {
        struct session *s = it->session.ptr;
        found = s && s->dev == dev;
        if (found) {
            break;
        }
    }

    int err = 0;
    if (!found) {
        err = -ENOSSN;
        goto no_session;
    }
    
    new_node->dev_name = it->dev_name;
    new_node->dev_name_hash = it->dev_name_hash;
    memcpy(new_node->password, it->password, SHA1_HASH_LEN);
    new_node->session.ptr = NULL;

    list_replace_rcu(&it->list, &new_node->list);

no_session:
    spin_unlock_irqrestore(&write_lock, flags);
    if (err) {
        kfree(new_node);
    } else {
        call_rcu(&it->rcu, end_session_rcu);
    }
}

static inline ssize_t length(struct snapshot_metadata *it) {
    size_t n = strlen(it->dev_name);
    struct session *s = it->session.ptr;
    if (s) {
        n += strlen(s->id);
    }
    return n;
}

/**
 * registry_show_session prints into buf the active sessions in the format:
 * <device name>: /snapshots/<session id>
 *
 * Currently the buffer is guaranteed to be a page (e.g. of 4K), so it might be not possible to 
 * print all the currently active sessions to the buffer, in this case the buffer can be terminated with
 * EOF (if there is enough space to hold "EOF").
 */
ssize_t registry_show_session(char *buf, size_t size) {
    rcu_read_lock();
    int err = 0;
    ssize_t br = 0;
    struct snapshot_metadata *it;
    list_for_each_entry_rcu(it, &registry_db, list) {
        ssize_t n = length(it);
        if (br + n >= size) {
            err = -1;
            break;
        }
        br += sprintf(&buf[br], "%s: ", it->dev_name);
        struct session *s = it->session.ptr;
        if (s) {
            br += sprintf(&buf[br], "/snapshots/%s\n", s->id);
        } else {
            br += sprintf(&buf[br], "-\n");
        }
    }
    rcu_read_unlock();
    if (err) {
        if (br + strlen("EOF") < size) {
            br += sprintf(&buf[br], "EOF");
        }
    }
    return br;
}

static void node_update_rcu(struct rcu_head *head) {
    struct snapshot_metadata *node = container_of(head, struct snapshot_metadata, rcu);
    kfree(node);
}

/**
 * registry_update_dir updates the current session associated to the device number dev writing that
 * the current active session has finally a directory where to write the blocks
 */
void registry_update_dir(dev_t dev, const char *session) {
    // we cannot update the node without make another allocation
    struct snapshot_metadata *new_node = node_alloc_noname(GFP_KERNEL);
    if (!new_node) {
        pr_debug(pr_format("out of memory"));
        return;
    }

    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    struct snapshot_metadata *old_node = get_by_dev(dev);
    int err = 0;
    if (!old_node) {
        err = -ENOSSN;
        goto release_lock;
    }

    new_node->dev_name = old_node->dev_name;
    new_node->dev_name_hash = old_node->dev_name_hash;
    memcpy(new_node->password, old_node->password, SHA1_HASH_LEN);
    struct session *s = old_node->session.ptr;
    if (!s) {
        pr_debug(pr_format("no session associated with device %s"), old_node->dev_name);
        goto release_lock;
    }
    s->has_dir = true;
    new_node->session.ptr = s;

    list_replace_rcu(&old_node->list, &new_node->list);

release_lock:
    spin_unlock_irqrestore(&write_lock, flags);
    call_rcu(&old_node->rcu, node_update_rcu);
}