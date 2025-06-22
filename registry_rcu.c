#include "include/registry.h"
#include "include/hash.h"
#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#define SHA1_HASH_LEN (20)

struct registry_entity {
    struct list_head list;
    char             *dev_name;
    char             *password;
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
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    synchronize_rcu();
    struct registry_entity *it, *tmp;
    list_for_each_entry_safe(it, tmp, registry_db.next, list) {
        kfree(it);
    }
    spin_unlock_irqrestore(&write_lock, flags);
}

static struct registry_entity* mk_node(const char *dev_name, const char *password) {
    int err;
    size_t n = strnlen(dev_name, PATH_MAX);
    if (n == PATH_MAX) {
        err = -ETOOBIG;
        goto no_node;
    }
    size_t total_size = sizeof(struct registry_entity) + (n + 1) + SHA1_HASH_LEN;
    struct registry_entity *node = kmalloc(total_size, GFP_KERNEL);
    if (node == NULL) {
        err = -ENOMEM;
        goto no_node;
    }
    node->list.prev = NULL;
    node->list.next = NULL;
    node->dev_name = (char*) node + sizeof(struct registry_entity);
    node->password = node->dev_name + (n + 1);
    err = hash2("sha1", password, strlen(password), node->password, SHA1_HASH_LEN);
    if (err) {
        goto free_node;
    }
    strscpy(node->dev_name, dev_name, n + 1);
    return node;

free_node:
    kfree(node);
no_node:
    return ERR_PTR(err);
}

static struct registry_entity* get_raw(const char *dev_name) {
    struct registry_entity *it;
    list_for_each_entry(it, registry_db.next, list) {
        if (!strcmp(it->dev_name, dev_name)) {
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
 * with the same name does not already exist.
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
 * registry_check_password checks if a block-device identified by dev_name has
 * been registered with the password password
 * @param dev_name the name of the block-device
 * @param password the password protecting the snapshot
 * @return false if the passwords does not match or a snapshot with dev_name does not exist, true otherwise.
 */
int registry_insert(const char *dev_name, const char *password) {
    struct registry_entity *ep = mk_node(dev_name, password);
    if (IS_ERR(ep)) {
        return PTR_ERR(ep);
    }
    pr_debug(pr_fmt("created node: %s\n"), ep->dev_name);
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    int err = try_add(ep);
    spin_unlock_irqrestore(&write_lock, flags);
    return err;
}

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
 * registry_check_password checks if a block-device identified by dev_name has
 * been registered with the password password
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

bool registry_lookup(const char *dev_name) {
    struct registry_entity *it;
    bool b = false;
    rcu_read_lock();
    list_for_each_entry_rcu(it, registry_db.next, list) {
        b = !strcmp(it->dev_name, dev_name);
        if (b) {
            break;
        }
    }
    rcu_read_unlock();
    return b;
}