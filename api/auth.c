#include "auth.h"
#include "hash.h"
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/string.h>
#define SHA256_LEN (32)
#define PASSWD_MAX (64)

static uint8_t *p_hash = NULL;

bool auth_check_password(const char *password) {
    if (p_hash == NULL) {
        return false;
    }
    size_t n = strnlen(password, PASSWD_MAX);
    if (n == PASSWD_MAX) {
        return false;
    }
    char *hash = hash_alloc("sha256", password, n);
    if (IS_ERR(hash)) {
        return false;
    }
    bool b = !memcmp(p_hash, hash, SHA256_LEN);
    kfree(hash);
    return b;
}

void auth_clear_password(void) {
    if (p_hash) {
        kfree(p_hash);
        p_hash = NULL;
    }
}

int auth_set_password(const char *password) {
    if (!password) {
        pr_err("password is null");
        return -EINVAL;
    }
    size_t n = strnlen(password, PASSWD_MAX);
    if (n == PASSWD_MAX) {
        pr_err("password too long");
        return -EINVAL;
    }
    p_hash = hash_alloc("sha256", password, n);
    if (IS_ERR(p_hash)) {
        return PTR_ERR(p_hash);
    }
    return 0;
}