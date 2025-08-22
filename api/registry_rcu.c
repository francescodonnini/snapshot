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
 * total_size returns the size (in number of bytes) required by snapshot metadata. The space required is the number of bytes required by
 * a struct snapshot_metadata and the space required to store the device name and password hash. Those informations are stored in a contiguos
 * memory regione to minimize kmalloc invocations.
 */
static size_t total_size(size_t n) {
    return sizeof(struct snapshot_metadata)
        + ALIGN(n + 1, sizeof(void*))
        + ALIGN(SHA1_HASH_LEN, sizeof(void*));
}

/**
 * helper function that initializes the pointers dev_name and password to the correct addresses.
 */
static struct snapshot_metadata *node_alloc(size_t n) {
    struct snapshot_metadata *node;
    node = kzalloc(total_size(n), GFP_KERNEL);
    if (!node) {
        return NULL;
    }
    node->dev_name = (char*)node + sizeof(*node);
    node->password = node->dev_name + ALIGN(n + 1, sizeof(void*));
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
    struct snapshot_metadata *node = node_alloc(n);
    if (node == NULL) {
        err = -ENOMEM;
        goto no_node;
    }
    err = hash("sha1", password, strlen(password), node->password, SHA1_HASH_LEN);
    if (err) {
        goto free_node;
    }
    node->dev_name_hash = fast_hash(dev_name);
    strscpy(node->dev_name, dev_name, n + 1);
    return node;

free_node:
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

static void node_free_rcu(struct rcu_head *head) {
    struct snapshot_metadata *node = container_of(head, struct snapshot_metadata, rcu);
    struct session *s = node->session.ptr;
    if (s) {
        session_destroy(s);
    }
    kfree(node);
}

/**
 * check_password computes the hash of password and compares it to pw_hash, that is an already hashed password.
 * It returns true if the hashes match, false if they don't match or some error occurred while computing the
 * cryptographic hash of password (see hash/hash2 for further details).
 */
static bool check_password(const char *pw_hash, const char *password, char *buffer) {
    int err = hash("sha1", password, strlen(password), buffer, SHA1_HASH_LEN);
    if (err) {
        return false;
    }
    return memcmp(pw_hash, buffer, SHA1_HASH_LEN) == 0;
}

static inline struct snapshot_metadata *get_by_name(const char *name) {
    unsigned long name_hash = fast_hash(name);
    struct snapshot_metadata *it;
    list_for_each_entry_rcu(it, &registry_db, list) {
        bool b = name_hash == it->dev_name_hash && !strcmp(name, it->dev_name);
        if (b) {
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
    char *buffer = kmalloc(SHA1_HASH_LEN, GFP_KERNEL);
    if (!buffer) {
        return -ENOMEM;
    }
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    struct snapshot_metadata *it = get_by_name(dev_name);
    int err = -EWRONGCRED;
    if (it && check_password(it->password, password, buffer)) {
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

static inline void node_copy(struct snapshot_metadata *dst, struct snapshot_metadata *src) {
    memcpy(dst->dev_name, src->dev_name, strlen(src->dev_name) + 1);
    dst->dev_name_hash = src->dev_name_hash;
    memcpy(dst->password, src->password, SHA1_HASH_LEN);
}

/**
 * registry_update updates a previously registered image/device with the associated device number
 * @param dev_name - the path of the image associated with the device
 * @param dev      - the device number associated to the device
 * @returns 0 on success, <0 otherwise.
 */
int registry_update(const char *dev_name, dev_t dev) {
    struct snapshot_metadata *new_node = node_alloc(strlen(dev_name));
    if (!new_node) {
        return -ENOMEM;
    }
    int err;
    struct session *session = session_create(dev);
    if (!session) {
        err = -ENOMEM;
        goto no_session;
    }
    err = snapshot_create(session->id);
    if (err) {
        goto no_directory;
    }
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    struct snapshot_metadata *old_node = get_by_name(dev_name);
    if (!old_node) {
        pr_debug(pr_format("cannot find device=%s"), dev_name);
        err = -EWRONGCRED;
        goto wrong_credentials;
    }
    node_copy(new_node, old_node);
    new_node->session.ptr = session;
    pr_debug(pr_format("device=%s;dev=%d,%d;uuid=%s"),
    dev_name, MAJOR(session->dev), MINOR(session->dev), session->id);
    list_replace_rcu(&old_node->list, &new_node->list);
    spin_unlock_irqrestore(&write_lock, flags);
    call_rcu(&old_node->rcu, node_free_rcu);
    return err;

wrong_credentials:
    spin_unlock_irqrestore(&write_lock, flags);
no_directory:
    session_destroy(session);
no_session:
    kfree(new_node);
    return err;
}

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
    if (added) {
        *added = false;
    }
    int err;
    if (!registered) {
        err = -ENOSSN;
        goto out;
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

bool registry_get_session_id(dev_t dev, char *id) {
    rcu_read_lock();
    bool found = false;
    struct snapshot_metadata *it;
    list_for_each_entry_rcu(it, &registry_db, list) {
        struct session *s = it->session.ptr;
        found = s && s->dev == dev;
        if (found) {
            strncpy(id, s->id, UUID_STRING_LEN + 1);
            break;
        }
    }
    rcu_read_unlock();
    return found;
}

void registry_end_session(dev_t dev) {
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
    struct snapshot_metadata *new_node = node_alloc(strlen(it->dev_name));
    if (!new_node) {
        err = -ENOMEM;
        goto no_session;
    }
    node_copy(new_node, it);
    list_replace_rcu(&it->list, &new_node->list);
no_session:
    spin_unlock_irqrestore(&write_lock, flags);
    if (err) {
        kfree(new_node);
    } else {
        call_rcu(&it->rcu, node_free_rcu);
    }
}