#include "include/hash.h"
#include "include/pr_format.h"
#include "include/registry.h"
#include <linux/container_of.h>
#include <linux/slab.h>
#include <linux/limits.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/string.h>

#define SHA1_HASH_LEN     20

struct registry_node {
    struct list_head list;
    char *dev_name;
    char *password;
};

static LIST_HEAD(lt_head);
static DEFINE_RWLOCK(lock);

/**
 * registry_init initializes all necessary data structures to manage snapshots credentials
 * @return always 0
 */
int registry_init() {
    return 0;
}

void registry_cleanup() {
    unsigned long flags;
    write_lock_irqsave(&lock, flags);
    struct list_head *pos, *tmp;
    list_for_each_safe(pos, tmp, &lt_head) {
        struct registry_node *it = list_entry(pos, struct registry_node, list);
        kfree(it);
    }
    lt_head.next = NULL;
    lt_head.prev = NULL;
    write_unlock_irqrestore(&lock, flags);
}

static inline struct registry_node* get_node_raw(const char *dev_name) {
    struct registry_node *node;
    list_for_each_entry(node, &lt_head, list) {
        if (!strncmp(node->dev_name, dev_name, PATH_MAX)) {
            return node;
        }
    }
    return NULL;
}

static struct registry_node* get_node(const char *dev_name) {
    unsigned long flags;
    read_lock_irqsave(&lock, flags);
    struct registry_node *n = get_node_raw(dev_name);
    read_unlock_irqrestore(&lock, flags);
    return n;
}

static inline int lookup_node(const char *dev_name) {
    return get_node(dev_name) != NULL;
}

static struct registry_node *mk_node(const char *dev_name, const char *password) {
    int err;
    size_t n = strnlen(dev_name, PATH_MAX);
    if (n == PATH_MAX) {
        err = -ETOOBIG;
        goto no_node;
    }
    struct registry_node *node = kmalloc(sizeof(struct registry_node), GFP_KERNEL);
    if (node == NULL) {
        err = -ENOMEM;
        goto no_node;
    }
    node->dev_name = kmalloc(n + 1, GFP_KERNEL);
    if (node->dev_name == NULL) {
        err = -ENOMEM;
        goto no_dev_name;
    }
    node->password = hash("sha1", password, strlen(password));
    if (IS_ERR(node->password)) {
        goto no_hash;
    }
    node->list.next = NULL;
    node->list.prev = NULL;
    strscpy(node->dev_name, dev_name, n);
    return node;

no_hash:
    kfree(node->dev_name);
no_dev_name:
    kfree(node);
no_node:
    return ERR_PTR(err);
}

/**
 * registry_insert insert the credentials of a new snapshot if dev_name has not already
 * been used.
 * @param dev_name the name of the block device
 * @param password the password protecting the snapshot
 * @return
 * * -EDUPNAME if dev_name has been already used to register a block-
 * * -ETOOBIG  if dev_name length exceeds 4096 B
 * * -ENOMEM   if there isn't enough space to allocate the credentials of a new block-device
 * * 0 otherwise
 */
int registry_insert(const char *dev_name, const char *password) {
    if (lookup_node(dev_name)) {
        return -EDUPNAME;
    }
    struct registry_node *node = mk_node(dev_name, password);
    if (IS_ERR(node)) {
        return PTR_ERR(node);
    }
    unsigned long flags;
    write_lock_irqsave(&lock, flags);
    list_add(&node->list, &lt_head);
    write_unlock_irqrestore(&lock, flags);
    pr_debug("node %s inserted successfully\n", dev_name);
    return 0;
}

static int check_password(const char *pw_hash, const char *password) {
    char *h = hash("sha1", password, strlen(password));
    if (IS_ERR(h)) {
        return 0;
    }
    int ir = memcmp(pw_hash, h, SHA1_HASH_LEN) == 0;
    kfree(h);
    return ir;
}

/**
 * registry_delete deletes the credentials associated with the block-device dev_name
 * @param dev_name the name of the block-device
 * @param password the password protecting the snapshot
 * @return -EWRONGCRED if the password or the device name are wrong, 0 otherwise 
 */
int registry_delete(const char *dev_name, const char *password) {
    struct registry_node *node = get_node(dev_name);
    if (node == NULL || !check_password(node->password, password)) {
        return -EWRONGCRED;
    }
    unsigned long flags;
    write_lock_irqsave(&lock, flags);
    list_del(&(node->list));
    write_unlock_irqrestore(&lock, flags);
    pr_debug(pr_format("device %s deleted successfully\n"), dev_name);
    return 0;
}

/**
 * registry_check_password checks if a block-device identified by dev_name has
 * been registered with the password password
 * @param dev_name the name of the block-device
 * @param password the password protecting the snapshot
 * @return 0 if the passwords does not match or a snapshot with dev_name does not exist, 1 otherwise.
 */
int registry_check_password(const char *dev_name, const char *password) {
    struct registry_node *rp = get_node(dev_name);
    return rp != NULL && check_password(rp->password, password);
}