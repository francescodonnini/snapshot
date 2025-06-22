#include "include/hash.h"
#include "include/pr_format.h"
#include <linux/errname.h>
#include <crypto/hash.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/types.h>

/**
 * hash - calculates hash of key
 * @param alg_name the hashing algorithm to be used -e.g. sha1.
 * @param key string to hash
 * @param key_len number of chars of key
 * @param hash hash
 * @return 0 if the message digest creation was successfull, <0 otherwise (see crypto_shash_* API)
 */
char *hash(const char *alg_name, const char *key, int len) {
    int err;
    struct crypto_shash *alg = crypto_alloc_shash(alg_name, 0, 0);
    if (IS_ERR(alg)) {
        err = PTR_ERR(alg);
        goto no_tfm_alloc;
    }
    struct shash_desc *desc;
    size_t desc_size = crypto_shash_descsize(alg) + sizeof(*desc);
    desc = kzalloc(desc_size, GFP_KERNEL);
    if (desc == NULL) {
        err = -ENOMEM;
        goto error;
    }
    desc->tfm = alg;

    size_t digest_size = crypto_shash_digestsize(alg);
    char *out = kmalloc(digest_size, GFP_KERNEL);
    if (out == NULL) {
        err = -ENOMEM;
        goto error;
    }
    err = crypto_shash_digest(desc, key, len, out);
    if (err < 0) {
        goto no_hash_digest;
    }
    crypto_free_shash(desc->tfm);
    kfree(desc);
    return out;

no_hash_digest:
    kfree(out);
error:
    kfree(desc);
    crypto_free_shash(desc->tfm);
no_tfm_alloc:
    return ERR_PTR(err);
}

int hash2(const char *alg_name, const char *key, int key_len, char *out, int out_size) {
    int err;
    struct crypto_shash *alg = crypto_alloc_shash(alg_name, 0, 0);
    if (IS_ERR(alg)) {
        err = PTR_ERR(alg);
        goto no_tfm_alloc;
    }
    struct shash_desc *desc;
    size_t desc_size = crypto_shash_descsize(alg) + sizeof(*desc);
    desc = kzalloc(desc_size, GFP_KERNEL);
    if (desc == NULL) {
        err = -ENOMEM;
        goto free_shash;
    }
    desc->tfm = alg;
    unsigned int digest_size = crypto_shash_digestsize(alg);
    if (digest_size > out_size) {
        pr_debug(pr_format("buffer reserved to contain hash digest is not big enough: expected %d but got %d\n"), digest_size, out_size);
        err = -1;
        goto free_desc;
    }
    err = crypto_shash_digest(desc, key, key_len, out);
free_desc:
    kfree(desc);
free_shash:
    crypto_free_shash(desc->tfm);
no_tfm_alloc:
    return err;
}