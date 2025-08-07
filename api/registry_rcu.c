#include "registry.h"
#include "fast_hash.h"
#include "hash.h"
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
        kfree(it);
    }
}

/**
 * mk_node allocates memory for a struct registry_entity and initializes some of its fields:
 * it copies dev_name and the hash of password to the newly allocated memory area, and it stores the hash of dev_name.
 * It returns:
 * 
 * * -ETOOBIG if @param dev_name is too long to represent a valid file path;
 * 
 * * -ENOMEM if kmalloc failed to allocate enough memory to hold struct registry_entity
 * 
 * *  other errors if it was not possibile to compute the cryptographic hash function of password (see 'hash' and 'hash2' for details)
 * 
 * * a pointer to the newly allocated memory area otherwise.
 */
static struct registry_entity* mk_node(const char *dev_name, const char *password) {
    int err;
    size_t n = strnlen(dev_name, PATH_MAX);
    if (n == PATH_MAX) {
        err = -ETOOBIG;
        goto no_node;
    }
    size_t total_size = sizeof(struct registry_entity) + (n + 1) + SHA1_HASH_LEN + (UUID_STRING_LEN + 1);
    struct registry_entity *node = kmalloc(total_size, GFP_KERNEL);
    if (node == NULL) {
        err = -ENOMEM;
        goto no_node;
    }
    node->list.prev = NULL;
    node->list.next = NULL;
    node->dev_name_hash = fast_hash(dev_name);
    node->dev_name = (char*) node + sizeof(struct registry_entity);
    node->password = node->dev_name + (n + 1);
    err = hash2("sha1", password, strlen(password), node->password, SHA1_HASH_LEN);
    if (err) {
        goto free_node;
    }
    node->session_id = node->password + SHA1_HASH_LEN;
    strscpy(node->dev_name, dev_name, n + 1);
    return node;

free_node:
    kfree(node);
no_node:
    return ERR_PTR(err);
}

static struct registry_entity* get_raw(const char *dev_name) {
    unsigned long h = fast_hash(dev_name);
    struct registry_entity *it;
    list_for_each_entry(it, &registry_db, list) {
        if (h == it->dev_name_hash && !strcmp(it->dev_name, dev_name)) {
            return it;
        }
    }
    return NULL;
}

static inline bool lookup_raw(const char *dev_name) {
    return get_raw(dev_name) != NULL;
}

/**
 * try_add adds a registry_entity inside the registry database if another entity
 * with the same name does not already exist. It assumes that the lock is held.
 * @param ep the entity to be added
 * @return 0 if the insertion was sucessfull, -EDUPNAME otherwise
 */
static int try_add(struct registry_entity *ep) {
    if (lookup_raw(ep->dev_name)) {
        return -EDUPNAME;
    }
    list_add_rcu(&ep->list, &registry_db);
    return 0;
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
    if (err != n - 1) {
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
        return -1;
    }
    return snapshot_create(e->dev, e->session_id);
}

/**
 * registry_insert tries to register a device/image file. It returns 0 on success, <0 otherwise.
 */
int registry_insert(const char *dev_name, const char *password) {
    struct registry_entity *ep = mk_node(dev_name, password);
    if (IS_ERR(ep)) {
        return PTR_ERR(ep);
    }
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    int err = try_add(ep);
    spin_unlock_irqrestore(&write_lock, flags);
out:
    if (err) {
        kfree(ep);
    }
    return err;
}

/**
 * check_password computes the hash of @param password and compares it to @param pw_hash, that is an already hashed password.
 * It returns true if the hashes match, false if they don't match or some error occurred while computing the
 * cryptographic hash of @param password (see hash/hash2 for further details).
 */
static bool check_password(const char *pw_hash, const char *password) {
    char *h = hash("sha1", password, strlen(password));
    if (IS_ERR(h)) {
        return false;
    }
    bool b = memcmp(pw_hash, h, SHA1_HASH_LEN) == 0;
    kfree(h);
    return b;
}

/**
 * registry_delete deletes the credentials associated with the block-device dev_name
 * @param dev_name the name of the block-device
 * @param password the password protecting the snapshot
 * @return -EWRONGCRED if the password or the device name are wrong, 0 otherwise 
 */
int registry_delete(const char *dev_name, const char *password) {
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    struct registry_entity *ep = get_raw(dev_name);
    int err = -EWRONGCRED;
    if (ep && check_password(ep->password, password)) {
        list_del_rcu(&ep->list);
        err = 0;
    }
    spin_unlock_irqrestore(&write_lock, flags);
    synchronize_rcu();
    kfree(ep);
    return err;
}

/**
 * registry_check_password checks if a block-device identified by @param dev_name has
 * been registered with the password @param password
 * @param dev_name the name of the block-device
 * @param password the password protecting the snapshot
 * @return false if the passwords does not match or a snapshot with dev_name does not exist, true otherwise.
 */
bool registry_check_password(const char *dev_name, const char *password) {
    rcu_read_lock();
    struct registry_entity *ep = get_raw(dev_name);
    bool b = !ep && check_password(ep->password, password);
    rcu_read_unlock();
    return b;
}

static inline bool registry_lookup_aux(bool(*pred)(struct registry_entity*, void *args), void *args) {
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

static inline bool by_mm(struct registry_entity *it, void *args) {
    dev_t *dev = (dev_t*)args;
    return it->dev == *dev;
}

/**
 * registry_lookup_mm returns true if there exists a registered entity associated with the device number @param dev,
 * false otherwise.
 */
bool registry_lookup_mm(dev_t dev) {
    return registry_lookup_aux(by_mm, (void*)&dev);
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
    struct registry_entity *ep = get_raw(dev_name);
    int err = 0;
    if (!ep) {
        err = -1;
        goto ru_release_lock;
    }
    err = update_dev(ep, dev);
ru_release_lock:
    spin_unlock_irqrestore(&write_lock, flags);
    return err;
}