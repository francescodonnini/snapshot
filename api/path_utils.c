#include "path_utils.h"
#include "pr_format.h"
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/path.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/printk.h>

static inline char *add_parent(char *buffer, const char *parent, size_t n) {
    strcpy(buffer, parent);
    char *end = &buffer[n-1];
    if (*end != '/') {
        ++end;
        *end = '/';
    }
    return ++end;
}

static inline void add_child(char *buffer, const char *child, size_t n) {
    if (*child == '/') {
        ++child;
        --n;
    }
    strcpy(buffer, child);
    buffer[n] = 0;
}

/**
 * path_join -- utility to join two path components. It fails if:
 * 1. the sum of the bytes between parent and child is greater or equal to the maximum number of bytes
 *    a path can possibly have.
 * 2. parent or child is an empty string
 * @param parent is the first component of the path
 * @param child  is the second componente of the path
 * @returns buffer on success, NULL on error.
 */
char *path_join(const char *parent, const char *child, char *buffer) {
    size_t psz = strnlen(parent, PATH_MAX);
    size_t csz = strnlen(child, PATH_MAX);
    if (csz == 0 || psz == 0 || psz + csz >= PATH_MAX) {
        return NULL;
    }
    char *end = add_parent(buffer, parent, psz);
    add_child(end, child, csz);
    return buffer;
}

char *path_join_alloc(const char *parent, const char *child) {
    size_t n = strnlen(parent, PATH_MAX) + strnlen(child, PATH_MAX) + 1;
    if (n == 1 || n >= PATH_MAX) {
        return NULL;
    }
    char *buffer = kmalloc(n, GFP_KERNEL);
    if (!buffer) {
        return ERR_PTR(-ENOMEM);
    }
    return path_join(parent, child, buffer);
}