#include "registry.h"
#include "fast_hash.h"
#include "hash.h"
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
#include <linux/time.h>
#include <linux/uuid.h>

// Little auxiliary struct used by "by_dev_and_time_ge" predicate
struct node_dev {
    struct timespec64 *time;
    dev_t              dev;
};

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
    size_t            dev_name_len;
    struct session   *session;
    struct rcu_head   rcu;
};

LIST_HEAD(registry_db);
DEFINE_SPINLOCK(write_lock);

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
    unsigned long flags;
    LIST_HEAD(list);

    spin_lock_irqsave(&write_lock, flags);
    list_splice_init(&registry_db, &list);
    spin_unlock_irqrestore(&write_lock, flags);

    synchronize_rcu();
    struct snapshot_metadata *it, *tmp;
    list_for_each_entry_safe(it, tmp, &list, list) {
        struct session *s = it->session;
        if (s) {
            session_destroy(s);
        }
        kfree(it);
    }
}

/**
 * registry_lookup_rcu returns true if a node in the list satisfies a certain predicate, it runs inside a RCU
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

static inline struct snapshot_metadata *get_by_name(const char *name) {
    struct node_name nn = {
        .hash = fast_hash(name),
        .name = name
    };
    return registry_get_by(by_name, &nn);
}

static inline struct snapshot_metadata *get_by_name_rcu(const char *name) {
    struct node_name nn = {
        .hash = fast_hash(name),
        .name = name
    };
    return registry_get_by_rcu(by_name, &nn);
}

static inline bool by_dev_and_time_ge(struct snapshot_metadata *node, const void *args) {
    struct node_dev *nd = (struct node_dev*)args;
    struct session *s = node->session;
    if (!s) {
        return false;
    }
    return s->dev == nd->dev && timespec64_compare(&s->created_on, nd->time) <= 0;
}

static inline struct snapshot_metadata *get_by_dev_and_time_ge_rcu(dev_t dev, struct timespec64 *time) {
    struct node_dev nd = {
        .dev = dev,
        .time = time,
    };
    return registry_get_by_rcu(by_dev_and_time_ge, &nd);
}

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
    node->dev_name_len = n - 1;
    node->dev_name_hash = fast_hash(name);
    return node;
}

/**
 * mk_node allocates memory for a struct snapshot_metadata and initializes some of its fields:
 * it copies dev_name to the newly allocated memory area, and it stores its hash.
 * It returns:
 * * -ETOOBIG if dev_name is too long to represent a valid file path;
 * * -ENOMEM if kmalloc failed to allocate enough memory to hold struct snapshot_metadata
 * * a pointer to the newly allocated memory area otherwise.
 */
static struct snapshot_metadata* mk_node(const char *dev_name) {
    size_t n = strnlen(dev_name, PATH_MAX);
    if (n == PATH_MAX) {
        return ERR_PTR(-ETOOBIG);
    }
    struct snapshot_metadata *node = node_alloc(dev_name, GFP_KERNEL);
    if (!node) {
        return ERR_PTR(-ENOMEM);
    }
    return node;
}

/**
 * registry_insert tries to register a device/image file. It returns 0 on success, <0 otherwise.
 */
int registry_insert(const char *dev_name) {
    struct snapshot_metadata *node = mk_node(dev_name);
    if (IS_ERR(node)) {
        return PTR_ERR(node);
    }
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    int err;
    if (get_by_name(dev_name)) {
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
        snap_map_destroy(s->dev, &s->created_on);
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
int registry_delete(const char *dev_name) {
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    struct snapshot_metadata *it = get_by_name(dev_name);
    int err;
    if (it) {
        list_del_rcu(&it->list);
        err = 0;
    } else {
        err = -EWRONGCRED;
    }
    spin_unlock_irqrestore(&write_lock, flags);
    
    if (!err) {
        call_rcu(&it->rcu, registry_delete_rcu);
    }
    return err;
}

static void free_session_rcu(struct rcu_head *head) {
    struct snapshot_metadata *node = container_of(head, struct snapshot_metadata, rcu);
    struct session *s = node->session;
    if (s) {
        snap_map_destroy(s->dev, &s->created_on);
        session_destroy(s);
    }
    kfree(node);
}

int registry_session_prealloc(const char *dev_name, dev_t dev) {
    struct snapshot_metadata *new_node = node_alloc_noname(GFP_ATOMIC);
    if (!new_node) {
        pr_err("out of memory");
        return -ENOMEM;
    }
    struct session *new_ssn = session_create(dev);
    if (!new_ssn) {
        pr_err("out of memory");
        kfree(new_node);
        return -ENOMEM;
    }

    int err = 0;
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    struct snapshot_metadata *current_node = get_by_name(dev_name);
    if (!current_node) {
        pr_debug(pr_format("no device associated to device=%s,%d:%d"), dev_name, MAJOR(dev), MINOR(dev));
        err = -EWRONGCRED;
        goto release_lock; // no device
    }
    bool free_old_session = false; // if true, then the old session should be scheduled for deferred deallocation
    struct session *current_ssn = current_node->session;
    if (current_ssn) {       
        // the old session could be deallocated because a new device has been mounted
        free_old_session = true;
    }
    new_node->session = new_ssn;
    new_node->dev_name = current_node->dev_name;
    new_node->dev_name_hash = current_node->dev_name_hash;
    new_node->dev_name_len = current_node->dev_name_len;
    list_replace_rcu(&current_node->list, &new_node->list);
    spin_unlock_irqrestore(&write_lock, flags);

    if (free_old_session) {
        call_rcu(&current_node->rcu, free_session_rcu);
    }
    return err;

release_lock:
    spin_unlock_irqrestore(&write_lock, flags);
    kfree(new_node);
    session_destroy(new_ssn);
    return err;
}

/**
 * tail copies to out the last n character of s or the last k < n characters until '/' is met.
 * @s string to copy the characters from
 * @s_len the length of s
 * @out the buffer to write the character to
 * @out_len the length of buffer (including NUL)
 * 
 * It returns the number of characters copied if successfull, <0 otherwise.
 */
static int tail(const char *s, size_t s_len, char *out, size_t out_len, size_t n) {
    char *occ = strrchr(s, '/');
    if (occ && !occ[1]) {
        pr_warn("malformed device name %s", s);
        return -1;
    }
    // tail should get at least n characters but s can be shorter than that
    n = min_t(size_t, n, s_len);
    if (occ) {
        // '/' is not included in the directory name
        ++occ;
        size_t k = &s[s_len] - occ;
        if (k < n) {
            return strscpy(out, occ, out_len);
        }
    } else {
        pr_warn("strange device name %s", s);
    }
    return strscpy(out, &s[s_len - n], out_len);
}

static int get_dirname(const char *dev_name, size_t dev_name_len, struct timespec64 *created_on, char *out, size_t n) {
    int tail_n = tail(dev_name, dev_name_len, out, n, get_dirname_prefix_len());
    if (tail_n <= 0) {
        pr_err("cannot write tail of %s to buffer", dev_name);
        return -1;
    }
    struct tm tm;
    time64_to_tm(created_on->tv_sec, 0, &tm);
    scnprintf(&out[tail_n], n - tail_n,
              "%04ld-%02d-%02dT%02d:%02d:%02d.%09ld",
              tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
              tm.tm_hour, tm.tm_min, tm.tm_sec,
              created_on->tv_nsec);
    return 0;
}

/**
 * registry_session_id returns true if there exists a session associated to device number dev and read_completed_on >= session creation date,
 * false otherwise.
 */
bool registry_session_id(dev_t dev, struct timespec64 *read_completed_on, char *dirname, size_t n, struct timespec64 *created_on) {
    rcu_read_lock();
    bool found = false;
    struct snapshot_metadata *it = get_by_dev_and_time_ge_rcu(dev, read_completed_on);
    found = it != NULL;
    if (found) {
        struct session *s = it->session;
        if (get_dirname(it->dev_name, it->dev_name_len, &s->created_on, dirname, n)) {
            found = false;
            goto out;
        }
        memcpy(created_on, &s->created_on, sizeof(*created_on));
    }
out:
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
    struct snapshot_metadata *new_node = node_alloc_noname(GFP_ATOMIC);
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
    new_node->dev_name_len = it->dev_name_len;
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
int registry_add_range(dev_t dev, struct timespec64 *created_on, struct b_range *range) {
    rcu_read_lock();
    struct snapshot_metadata *it = get_by_dev_and_time_ge_rcu(dev, created_on);
    int err;
    if (!it) {
        err = -ENOSSN;
        pr_debug(pr_format("registry_add_range: no session associated to device %d:%d"), MAJOR(dev), MINOR(dev));
        goto out;
    }
    err = itree_add(it->session, range);
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
int registry_lookup_range(dev_t dev, unsigned long start, unsigned long end_excl) {
    rcu_read_lock();
    struct snapshot_metadata *it = registry_get_by_rcu(by_dev, &dev);
    int err;
    if (!it) {
        err = -ENOSSN;
    } else {
        err = itree_subset_of(it->session, start, end_excl) ? -EEXIST : 0;
    }
    rcu_read_unlock();
    return err;
}

static inline ssize_t length(struct snapshot_metadata *it) {
    size_t n = strlen(it->dev_name) + 1; // + length of " "
    struct session *s = it->session;
    if (s) {
        n += get_dirname_len() + 1; 
    } else {
        n += strlen("-\n");
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
    const int dirname_len = get_dirname_len();
    char *dirname = kzalloc(dirname_len + 1, GFP_KERNEL);
    if (!dirname) {
        return -ENOMEM;
    }
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
        if (s) {
            if (!get_dirname(it->dev_name, it->dev_name_len, &s->created_on, dirname, dirname_len + 1)) {
                br += sprintf(&buf[br], "%s\n", dirname);
            }
        } else {
            br += sprintf(&buf[br], "-\n");
        }
    }
    rcu_read_unlock();
    if (err && br + strlen("EOF") < size) {
        br += sprintf(&buf[br], "EOF");
    } else if (!br) {
        br += sprintf(buf, "(no devices)\n");
    }
    kfree(dirname);
    return br;
}