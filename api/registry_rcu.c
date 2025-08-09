#include "registry.h"
#include "fast_hash.h"
#include "hash.h"
#include "hashset.h"
#include "pr_format.h"
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

struct registry_entity {
    struct list_head     list;
    // speed up searches by making string comparisons only on collisions or matches
    unsigned long        dev_name_hash; 
    char                *dev_name;
    char                *password;
    char                *session_id;
    dev_t                dev;
    struct hashset       set;
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
    struct registry_entity *it, *tmp;
    list_for_each_entry_safe(it, tmp, &old_head, list) {
        hashset_destroy(it->dev, &it->set);
        kfree(it);
    }
}

static inline bool registry_lookup_rcu(bool(*pred)(struct registry_entity*, void *args), void *args) {
    struct registry_entity *it;
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

static inline bool by_dev_name(struct registry_entity *it, void *args) {
    struct registry_entity *node = (struct registry_entity*)args;
    return it->dev_name_hash == node->dev_name_hash
           && !strcmp(it->dev_name, node->dev_name);
}

/**
 * try_add adds a registry_entity inside the registry database if another entity
 * with the same name does not already exist. It assumes that the lock is held.
 * @param ep the entity to be added
 * @return 0 if the insertion was sucessfull, -EDUPNAME otherwise
 */
static int try_add(struct registry_entity *ep) {
    if (registry_lookup_rcu(by_dev_name, ep)) {
        return -EDUPNAME;
    }
    list_add_rcu(&ep->list, &registry_db);
    return 0;
}

/**
 * mk_node allocates memory for a struct registry_entity and initializes some of its fields:
 * it copies dev_name and the hash of password to the newly allocated memory area, and it stores the hash of dev_name.
 * It returns:
 * * -ETOOBIG if dev_name is too long to represent a valid file path;
 * * -ENOMEM if kmalloc failed to allocate enough memory to hold struct registry_entity
 * *  other errors if it was not possibile to compute the cryptographic hash function of password (see 'hash' and 'hash_alloc' for details)
 * *  a pointer to the newly allocated memory area otherwise.
 */
static struct registry_entity* mk_node(const char *dev_name, const char *password) {
    int err;
    size_t n = strnlen(dev_name, PATH_MAX);
    if (n == PATH_MAX) {
        err = -ETOOBIG;
        goto no_node;
    }
    struct registry_entity *node;
    size_t size = sizeof(*node);
    size += ALIGN(n + 1, sizeof(void*));
    size += ALIGN(SHA1_HASH_LEN, sizeof(void*));
    size += ALIGN(UUID_STRING_LEN + 1, sizeof(void*));
    node = kzalloc(size, GFP_KERNEL);
    if (node == NULL) {
        err = -ENOMEM;
        goto no_node;
    }
    node->dev_name = (char*)node + sizeof(*node);
    node->password = node->dev_name + ALIGN(n + 1, sizeof(void*));
    node->session_id = node->password + ALIGN(SHA1_HASH_LEN, sizeof(void*));
    err = hash("sha1", password, strlen(password), node->password, SHA1_HASH_LEN);
    if (err) {
        goto free_node;
    }
    node->dev_name_hash = fast_hash(dev_name);
    strscpy(node->dev_name, dev_name, n + 1);
    err = hashset_create(&node->set);
    if (err) {
        goto free_node;
    }
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
    struct registry_entity *node = mk_node(dev_name, password);
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

static inline struct registry_entity *get_by_name(const char *name) {
    unsigned long name_hash = fast_hash(name);
    struct registry_entity *it;
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
    char *other_pw_buffer = kmalloc(SHA1_HASH_LEN, GFP_KERNEL);
    if (!other_pw_buffer) {
        return -ENOMEM;
    }
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    struct registry_entity *it = get_by_name(dev_name);
    int err = -EWRONGCRED;
    if (it && check_password(it->password, password, other_pw_buffer)) {
        list_del_rcu(&it->list);
        err = 0;
    }
    spin_unlock_irqrestore(&write_lock, flags);
    kfree(other_pw_buffer);
    if (!err && it) {
        synchronize_rcu();
        hashset_destroy(it->dev, &it->set);
        kfree(it);
    }
    return err;
}

static inline bool by_mm(struct registry_entity *it, void *args) {
    dev_t *dev = (dev_t*)args;
    return it->dev == *dev;
}

/**
 * registry_lookup_dev returns true if there exists a registered entity associated with the device number (dev),
 * false otherwise.
 */
bool registry_lookup_dev(dev_t dev) {
    return registry_lookup_rcu(by_mm, (void*)&dev);
}

static inline bool is_active(struct registry_entity *it, void *args) {
    dev_t *dev = (dev_t*)args;
    return it->dev == *dev && !it->session_id;
}

bool registry_lookup_active(dev_t dev) {
    return registry_lookup_rcu(is_active, (void*)&dev);
}

/**
 * gen_uuid generates a unique identifier leveraging the kernel API uuid_*. A uuid identifies
 * a session, that is all the interaction with a block device between the moment it has been mounted and
 * it has been unmounted. It stores the string representation of the id in @param out, a pointer to a char
 * buffer of size @param n. It returns 0 on success or -1 otherwise.
 */
static int gen_uuid(char *out, size_t n) {
    uuid_t uuid;
    uuid_gen(&uuid);
    int err = snprintf(out, n, "%pUb", &uuid);
    if (err >= n) {
        pr_debug(pr_format("cannot parse uuid"));
        return -1;
    }
    return 0;
}

/**
 * update_dev updates the entity associated with a certain filesystem image with the device number associated to the
 * instance of the filesystem previously mounted. Together with the device number, a unique identifier is generated. The
 * ID is the name of the folder in '/snapshots' where the original blocks of the filesystem are stored. This functions fails if
 * either some error occurred while generating the unique ID or it was not possible to create all the necessary data structured to
 * represent the filesystem snapshot. It returns <0 on failure, 0 otherwise.
 */
static int update_dev(struct registry_entity *e, dev_t dev) {
    e->dev = dev;
    int err = gen_uuid(e->session_id, UUID_STRING_LEN + 1);
    if (err < 0) {
        return -ENOUUID;
    }
    return snapshot_create(e->dev, e->session_id, &e->set);
}

/**
 * registry_update updates a previously registered image/device with the associated device number
 * @param dev_name - the path of the image associated with the device
 * @param dev      - the device number associated to the device
 * @returns 0 on success, <0 otherwise.
 */
int registry_update(const char *dev_name, dev_t dev) {
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    struct registry_entity *node = get_by_name(dev_name);
    int err = 0;
    if (!node) {
        err = -EWRONGCRED;
        goto registry_update_out;
    }
    err = update_dev(node, dev);
registry_update_out:
    spin_unlock_irqrestore(&write_lock, flags);
    return err;
}

bool registry_get_session(dev_t dev, char *session) {
    rcu_read_lock();
    bool found = false;
    struct registry_entity *it;
    list_for_each_entry_rcu(it, &registry_db, list) {
        found = it->dev == dev;
        if (found) {
            strncpy(session, it->session_id, UUID_STRING_LEN + 1);
            break;
        }
    }
    rcu_read_unlock();
    return found;
}