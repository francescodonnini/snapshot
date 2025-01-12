#include "include/hash.h"
#include "include/pr_format.h"
#include "include/registry.h"
#include <linux/container_of.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/string.h>

#define BDEV_NAME_MAX_LEN 32
#define SHA1_HASH_LEN 20

struct registry_node {
    struct list_head list;
    char *dev_name;
    char *password;
};

static LIST_HEAD(lt_head);

/**
 * registry_init initializes all necessary data structures to manage snapshots credentials
 * @return always 0
 */
int registry_init() {
    return 0;
}

void registry_cleanup() {
    while (!list_empty(&lt_head)) {
        struct registry_node *node = container_of(lt_head.next, struct registry_node, list);
        kfree(node->dev_name);
        kfree(node->password);
        list_del
    }
}

static inline struct registry_node* lookup_node_raw(const char *dev_name) {
    struct registry_node *node;
    list_for_each_entry(node, &lt_head, list) {
        if (!strncmp(node->dev_name, dev_name, BDEV_NAME_MAX_LEN)) {
            return node;
        }
    }
    return NULL;
}

static struct registry_node* get_node(const char *dev_name) {
    return lookup_node_raw(dev_name);
}

static inline int lookup_node(const char *dev_name) {
    return get_node(dev_name) != NULL;
}

static struct registry_node *mk_node(const char *dev_name, const char *password) {
    int err;
    struct registry_node *node = kmalloc(sizeof(struct registry_node), GFP_KERNEL);
    if (node == NULL) {
        err = -ENOMEM;
        goto no_node;
    }
    node->dev_name = kmalloc(BDEV_NAME_MAX_LEN + 1, GFP_KERNEL);
    if (node->dev_name == NULL) {
        err = -ENOMEM;
        goto no_dev_name;
    }
    char pw_hash[SHA1_HASH_LEN];
    err = hash("sha1", password, strlen(password), pw_hash);
    if (err) {
        goto no_hash;
    }
    node->password = kmalloc(SHA1_HASH_LEN, GFP_KERNEL);
    if (node->password == NULL) {
        err = -ENOMEM;
        goto no_password;
    }
    node->list.next = NULL;
    node->list.prev = NULL;
    strncpy(node->dev_name, dev_name, BDEV_NAME_MAX_LEN);
    strcpy(node->password, pw_hash);
    return node;

no_password:
    kfree(node->dev_name);
no_hash:
no_dev_name:
    kfree(node);
no_node:
    return ERR_PTR(-ENOMEM);
}

/**
 * registry_insert insert the credentials of a new snapshot if dev_name has not already
 * been used.
 * @param dev_name the name of the block device
 * @param password the password protecting the snapshot
 * @return
 * * -EBDEVNAME if dev_name has been already used to register a block-device
 * * -ENOMEM if there isn't enough space to allocate the credentials of a new block-device
 * * 0 otherwise
 */
int registry_insert(const char *dev_name, const char *password) {
    list_add(&(node->list), &lt_head);
    return 0;
}

static int check_password(struct registry_node *node, const char *password) {
    char pw_hash[SHA1_HASH_LEN];
    if (hash("sha1", password, strlen(password), pw_hash)) {
        return 0;
    }
    return strcmp(node->password, pw_hash) == 0;
}

/**
 * registry_delete deletes the credentials associated with the block-device dev_name
 * @param dev_name the name of the block-device
 * @param password the password protecting the snapshot
 */
void registry_delete(const char *dev_name, const char *password) {
    write_lock(&registry_lock);
    struct registry_node *node = lookup_node_raw(dev_name);
    if (check_password(node, password)) {
        hlist_del_init(&(node->list));
        pr_debug(ss_pr_format("(%s %s) successfully deleted\n"))
    }
    write_unlock(&registry_lock);
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
    if (!rp) {
        return 0;
    }
    return check_password(rp, password);
}