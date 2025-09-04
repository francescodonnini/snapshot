#include "registry.h"
#include "fast_hash.h"
#include "hash.h"
#include "iset.h"
#include "itree.h"
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

#define SHA256_LEN (32)

// Little auxiliary struct used by "by_name" predicate
struct node_name {
    const char    *name;
    unsigned long  hash;
};

// All snapshot metadata are stored in a doubly-linked list
struct snapshot_metadata {
    struct list_head  list;
    // speed up searches by making string comparisons only on collisions or matches
    unsigned long     dev_name_hash; 
    char             *dev_name;
    char              password[SHA256_LEN];
    struct session   *session;
    struct rcu_head   rcu;
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
        struct session *s = it->session;
        if (s) {
            session_destroy(s);
        }
        kfree(it);
    }
}

/**
 * registry_lookup_rcu returns true if a node in the list satisfies a certain predicate, it runs inside an RCU
 * critical section
 */
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

/**
 * registry_get_by looks up for a node that satisfies a certain predicate: pred. It must be called while the spinlock is held!
 */
static inline struct snapshot_metadata *registry_get_by(bool (*pred)(struct snapshot_metadata*, const void*), const void *args) {
    struct snapshot_metadata *it;
    list_for_each_entry(it, &registry_db, list) {
        if (pred(it, args)) {
            return it;
        }
    }
    return NULL;
}

/**
 * registry_get_by looks up for a node that satisfies a certain predicate: pred. It must be called inside a RCU critical section!
 */
static inline struct snapshot_metadata *registry_get_by_rcu(bool (*pred)(struct snapshot_metadata*, const void*), const void *args) {
    struct snapshot_metadata *it;
    list_for_each_entry_rcu(it, &registry_db, list) {
        if (pred(it, args)) {
            return it;
        }
    }
    return NULL;
}

static inline bool by_name(struct snapshot_metadata *node, const void *args) {
    struct node_name *name = (struct node_name*)args;
    return node->dev_name_hash == name->hash && !strcmp(node->dev_name, name->name);
}

static inline bool by_dev(struct snapshot_metadata *node, const void *args) {
    dev_t *dev = (dev_t*)args;
    struct session *s = node->session;
    return s && s->dev == *dev;
}

/**
 * get_by_name search a node whose device name is equal to name. It assumes it's called while a spinlock is held.
 */
static inline struct snapshot_metadata *get_by_name(const char *name) {
    struct node_name nn = {
        .hash = fast_hash(name),
        .name = name
    };
    return registry_get_by(by_name, &nn);
}

/**
 * get_by_name search a node whose device name is equal to name. It assumes it's called while a spinlock is held.
 */
static inline struct snapshot_metadata *get_by_name_rcu(const char *name) {
    struct node_name nn = {
        .hash = fast_hash(name),
        .name = name
    };
    return registry_get_by_rcu(by_name, &nn);
}

/**
 * helper function that initializes the pointer of password to the correct addresses.
 */
static inline struct snapshot_metadata *node_alloc_noname(gfp_t gfp) {
    struct snapshot_metadata *node;
    node = kzalloc(sizeof(*node), gfp);
    if (!node) {
        return NULL;
    }
    INIT_LIST_HEAD(&node->list);
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
    err = hash("sha256", password, strlen(password), node->password, SHA256_LEN);
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
    int err;
    struct node_name name = { 
        .hash = node->dev_name_hash,
        .name = node->dev_name
    };
    if (registry_lookup_rcu(by_name, &name)) {
        err = -EDUPNAME;
    } else {
        list_add_rcu(&node->list, &registry_db);
        err = 0;
    }
    spin_unlock_irqrestore(&write_lock, flags);
    if (err) {
        kfree(node->dev_name);
        kfree(node);
    }
    return err;
}

/**
 * registry_delete_rcu release all the resources associated with a certain snapshot. It destroy a session and should be called only when a node
 * is removed from the list (not updated).
 */
static void registry_delete_rcu(struct rcu_head *head) {
    struct snapshot_metadata *node = container_of(head, struct snapshot_metadata, rcu);
    struct session *s = node->session;
    if (s) {
        session_destroy(s);
    }
    kfree(node->dev_name);
    kfree(node);
}

/**
 * registry_delete deletes the credentials associated with the block-device dev_name
 * @param dev_name the name of the block-device
 * @param password the password protecting the snapshot
 * @return -EWRONGCRED if the password or the device name are wrong, 0 otherwise 
 */
int registry_delete(const char *dev_name, const char *password) {
    char *buffer = hash_alloc("sha256", password, strlen(password));
    if (IS_ERR(buffer)) {
        return PTR_ERR(buffer);
    }

    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    struct snapshot_metadata *it = get_by_name(dev_name);
    int err = -EWRONGCRED;
    if (it && !memcmp(buffer, it->password, SHA256_LEN)) {
        list_del_rcu(&it->list);
        err = 0;
    }
    spin_unlock_irqrestore(&write_lock, flags);
    
    kfree(buffer);
    if (!err) {
        call_rcu(&it->rcu, registry_delete_rcu);
    }
    return err;
}

static void free_session_rcu(struct rcu_head *head) {
    struct snapshot_metadata *node = container_of(head, struct snapshot_metadata, rcu);
    if (node->session) {
        session_destroy(node->session);
    }
    kfree(node);
}

int registry_session_prealloc(const char *dev_name, dev_t dev) {
    struct snapshot_metadata *node = node_alloc_noname(GFP_ATOMIC);
    if (!node) {
        pr_err("out of memory");
        return -ENOMEM;
    }
    struct session *session = session_create(dev);
    if (!session) {
        pr_debug(pr_format("out of memory"));
        kfree(node);
        return -ENOMEM;
    }

    int err = 0;
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    struct snapshot_metadata *old = get_by_name(dev_name);
    if (!old) {
        pr_debug(pr_format("no device associated to device=%s,%d:%d"), dev_name, MAJOR(dev), MINOR(dev));
        err = -EWRONGCRED;
        goto release_lock; // no device
    }
    bool free_old_session = false; // if true, then the old session should be scheduled for deferred deallocation
    struct session *s = old->session;
    if (s) {
        if (s->mntpoints > 0) { // current session is still active
            pr_err("usage counter of session %s is greater than 0", s->id);
            err = -1;
            goto release_lock;
        } else {
            // the old session could be deallocated because a new device has been mounted
            free_old_session = true;
        }
    }
    pr_debug(pr_format("session %s %d:%d #M=%d,P=%d"), session->id, MAJOR(dev), MINOR(dev), session->mntpoints, session->pending);
    session->pending = true;
    node->session = session;
    node->dev_name = old->dev_name;
    node->dev_name_hash = old->dev_name_hash;
    memcpy(node->password, old->password, SHA256_LEN);
    list_replace_rcu(&old->list, &node->list);
    spin_unlock_irqrestore(&write_lock, flags);
    if (free_old_session) {
        call_rcu(&old->rcu, free_session_rcu);
    }
    return err;

release_lock:
    spin_unlock_irqrestore(&write_lock, flags);
    kfree(node);
    session_destroy(session);
    return err;
}

static void free_node_only_rcu(struct rcu_head *head) {
    struct snapshot_metadata *node = container_of(head, struct snapshot_metadata, rcu);
    kfree(node);
}

/**
 * registry_session_get updates a previously registered image/device with the associated device number, if a session has been already registered,
 * then it increments it usage counter.
 * @param dev_name - the path of the image associated with the device
 * @param dev      - the device number associated to the device
 * @returns 0 on success, <0 otherwise.
 * 
 * It's called in kretprobe context.
 */
int registry_session_get(const char *dev_name, dev_t dev) {
    pr_debug(pr_format("registry_session_get(%s, %d:%d)"), dev_name, MAJOR(dev), MINOR(dev));
    int err = 0;
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    struct snapshot_metadata *node = registry_get_by(by_dev, &dev);
    if (!node) {
        pr_debug(pr_format("no device associated to device=%s,%d:%d"), dev_name, MAJOR(dev), MINOR(dev));
        err = -EWRONGCRED;
        goto release_lock; // no device
    }
    struct session *s = node->session;
    if (s) {
        if (s->mntpoints > 0) { // current session is still active
            s->mntpoints++;
        } else if (!s->mntpoints && s->pending) {
            s->mntpoints = 1;
            s->pending = false;
            pr_debug(pr_format("session %s has been created successfully for device %d:%d"), s->id, MAJOR(s->dev), MINOR(s->dev));
        } else {
            pr_debug(pr_format("trying to increment expired usage counter"));
        }
    }
release_lock:
    spin_unlock_irqrestore(&write_lock, flags);
    return err;
}

/**
 * registry_session_get updates a previously registered image/device with the associated device number, if a session has been already registered,
 * then it increments it usage counter.
 * @param dev_name - the path of the image associated with the device
 * @param dev      - the device number associated to the device
 * @returns 0 on success, <0 otherwise.
 * 
 * It's called in kretprobe context.
 */
int registry_session_get_or_create(const char *dev_name, dev_t dev) {
    struct snapshot_metadata *node = node_alloc_noname(GFP_ATOMIC);
    if (!node) {
        pr_err("out of memory");
        return -ENOMEM;
    }
    struct session *session = session_create(dev);
    if (!session) {
        pr_debug(pr_format("out of memory"));
        kfree(node);
        return -ENOMEM;
    }
    
    int err = 0;
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    struct snapshot_metadata *old = get_by_name(dev_name);
    if (!old) {
        pr_debug(pr_format("no device associated to device=%s,%d:%d"), dev_name, MAJOR(dev), MINOR(dev));
        err = -EWRONGCRED;
        goto release_lock; // no device
    }
    bool inplace_update = false; // if true, then only the usage counter has been incremented
    bool free_old_session = false; // if true, then the old session should be scheduled for deferred deallocation
    struct session *s = old->session;
    if (s) {
        if (s->mntpoints > 0) { // current session is still active
            s->mntpoints++;
            inplace_update = true;
            // no need to replace the old node with the new one!
        } else {
            session->mntpoints = 1;
            // the old session could be deallocated because a new device has been mounted
            node->session = session;
            free_old_session = true;
        }
    } else {
        node->session = session;
    }

    if (!inplace_update) {
        node->dev_name = old->dev_name;
        node->dev_name_hash = old->dev_name_hash;
        memcpy(node->password, old->password, SHA256_LEN);
        list_replace_rcu(&old->list, &node->list);
    }
    spin_unlock_irqrestore(&write_lock, flags);

    if (free_old_session) {
        call_rcu(&old->rcu, free_session_rcu);
    } else if (!inplace_update) {
        call_rcu(&old->rcu, free_node_only_rcu);
    } else {
        kfree(session);
        kfree(node);
    }
    return err;

release_lock:
    spin_unlock_irqrestore(&write_lock, flags);
    kfree(node);
    session_destroy(session);
    return err;
}

/**
 * registry_put_session attempts to decrement the usage counter of a session associated
 * to device number dev. Once the counter drops to zero, the corresponding session isn't free
 * immediately but it can be replaced by a new session if an appropriate device is mounted in the system.
 */
int registry_session_put(dev_t dev) {
    // it indicates that there is no need for an update so we can deallocate
    // the memory previously allocated
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    int err;
    struct snapshot_metadata *node = registry_get_by(by_dev, &dev);
    if (!node) {
        pr_debug(pr_format("no device associated to %d:%d"), MAJOR(dev), MINOR(dev));
        err = -ENODEV;
        goto unlock;
    }
    struct session *s = node->session;
    if (WARN(!s, "no session associated to %d:%d", MAJOR(dev), MINOR(dev))) {
        err = -ENOSSN;
        goto unlock;
    }
    if (WARN(!s->mntpoints, "usage count for session %s is already 0", s->id)) {
        err = -ENOSSN;
        goto unlock;
    }

    s->mntpoints--;
    if (s->pending) {
        s->pending = false;
    }
unlock:
    spin_unlock_irqrestore(&write_lock, flags);
    return 0;
}

bool registry_session_id(dev_t dev, char *id) {
    rcu_read_lock();
    bool found = false;
    struct snapshot_metadata *it = registry_get_by_rcu(by_dev, &dev);
    found = it != NULL;
    if (found) {
        struct session *s = it->session;
        strncpy(id, s->id, UUID_STRING_LEN + 1);
    }
    rcu_read_unlock();
    return found;
}

/**
 * registry_session_destroy detaches the current session from the node associated
 * to the device number dev. This function is used only when a function responsible to
 * fill a super block fails, so it is safe to assume that the session is not used by other
 * processes.
 */
void registry_session_destroy(dev_t dev) {
    struct snapshot_metadata *new_node = node_alloc_noname(GFP_KERNEL);
    if (!new_node) {
        pr_err("out of memory");
        return;
    }

    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    struct snapshot_metadata *it = registry_get_by(by_dev, &dev);
    int err = 0;
    if (!it) {
        err = -ENOSSN;
        goto no_session;
    }
    
    new_node->dev_name = it->dev_name;
    new_node->dev_name_hash = it->dev_name_hash;
    memcpy(new_node->password, it->password, SHA256_LEN);
    
    struct session *s = it->session;
    WARN(s->mntpoints != 1, "usage counter of session '%s' is %d, expected 1\n", s->id, s->mntpoints);
    
    new_node->session = NULL;

    list_replace_rcu(&it->list, &new_node->list);

no_session:
    spin_unlock_irqrestore(&write_lock, flags);
    if (err) {
        kfree(new_node);
    } else {
        call_rcu(&it->rcu, free_session_rcu);
    }
}

/**
 * registry_add_range adds a range [sector, sector + len] to a session associated to a device number dev.
 */
int registry_add_range(dev_t dev, sector_t sector, unsigned long len, bool *added) {
    rcu_read_lock();
    struct snapshot_metadata *it = registry_get_by_rcu(by_dev, &dev);
    int err;
    if (!it) {
        err = -ENOSSN;
        pr_err("no session associated to device %d:%d", MAJOR(dev), MINOR(dev));
        goto out;
    }
    struct session *s = it->session;
    err = itree_add(s, sector, len, added);
out:
    rcu_read_unlock();
    return err;
}

int registry_add_sector(dev_t dev, sector_t sector, bool *added) {
    rcu_read_lock();
    struct snapshot_metadata *it = registry_get_by_rcu(by_dev, &dev);
    int err;
    if (!it) {
        err = -ENOSSN;
        pr_err("no session associated to device %d:%d", MAJOR(dev), MINOR(dev));
        goto out;
    }
    struct session *s = it->session;
    err = iset_add(s, sector, added);
out:
    rcu_read_unlock();
    return err;
}

/**
 * registry_lookup_range checks if a certain device associated to device number dev has
 * already received a write request that targeted a certain sector. It returns zero if
 * there exists a device with device number dev that is currently mounted in the system, -ENOSSN otherwise.
 * present is an output parameter, after the function returns, it's equal to true if the sector has been
 * already registered by a previous write request, false otherwise.
 */
int registry_lookup_range(dev_t dev, sector_t sector, unsigned long len, bool *present) {
    rcu_read_lock();
    struct snapshot_metadata *it = registry_get_by_rcu(by_dev, &dev);
    *present = false;
    int err;
    if (!it) {
        err = -ENOSSN;
    } else {
        struct session *s = it->session;
        *present = itree_subset_of(s, sector, len);
        err = 0;
    }
    rcu_read_unlock();
    return err;
}

static inline ssize_t length(struct snapshot_metadata *it) {
    size_t n = strlen(it->dev_name) + 1; // + length of " "
    struct session *s = it->session;
    if (s) {
        n += strlen(s->id) + 1; // + length of "\n"
    } else {
        n += 2; // length of "-\n"
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
        br += sprintf(&buf[br], "%s ", it->dev_name);
        struct session *s = it->session;
        if (s && s->mntpoints > 0) {
            br += sprintf(&buf[br], "%s\n", s->id);
        } else {
            br += sprintf(&buf[br], "-\n");
        }
    }
    rcu_read_unlock();
    if (err && br + strlen("EOF") < size) {
        br += sprintf(&buf[br], "EOF");
    }
    return br;
}