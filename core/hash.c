#include "hash.h"
#include "pr_format.h"
#include <crypto/hash.h>
#include <linux/printk.h>
#include <linux/types.h>

static int init_shash(const char *alg_name, struct shash_desc **desc_out) {
    struct crypto_shash *alg = crypto_alloc_shash(alg_name, 0, 0);
    if (IS_ERR(alg)) {
        return PTR_ERR(alg);
    }
    struct shash_desc *desc;
    size_t desc_size = crypto_shash_descsize(alg) + sizeof(*desc);
    desc = kzalloc(desc_size, GFP_KERNEL);
    if (!desc) {
        crypto_free_shash(alg);
        return -ENOMEM;
    }
    desc->tfm = alg;
    *desc_out = desc;
    return 0;
}

static inline void free_shash_desc(struct shash_desc *desc) {
    crypto_free_shash(desc->tfm);
    kfree(desc);
}

/**
 * hash_alloc calculates hash of key using an algorithm specified by alg_name - e.g. sha1.
 * @param alg_name the hashing algorithm to be used -e.g. sha1.
 * @param key string to hash
 * @param key_len number of chars of key
 * @param hash hash
 * @return 0 if the message digest creation was successfull, <0 otherwise (see crypto_shash_* API)
 */
char *hash_alloc(const char *alg_name, const char *key, int len) {
    struct shash_desc *desc;
    int err = init_shash(alg_name, &desc);
    if (err) {
        return ERR_PTR(err);
    }
    char *out = kmalloc(crypto_shash_digestsize(desc->tfm), GFP_KERNEL);
    if (out == NULL) {
        err = -ENOMEM;
        goto no_out_buffer;
    }
    err = crypto_shash_digest(desc, key, len, out);
    if (err) {
        goto no_digest_out;
    }
    free_shash_desc(desc);
    return out;

no_digest_out:
    kfree(out);
no_out_buffer:
    free_shash_desc(desc);
    return ERR_PTR(err);
}

/**
 * hash is the same as hash_alloc but it expects the caller to allocate a buffer to contains the hash of the key
 */
int hash(const char *alg_name, const char *key, int key_len, char *out, int out_size) {
    struct shash_desc *desc;
    int err = init_shash(alg_name, &desc);
    if (err) {
        return err;
    }
    unsigned int digest_size = crypto_shash_digestsize(desc->tfm);
    if (digest_size > out_size) {
        pr_err("buffer reserved to contain hash digest is not big enough: expected %d but got %d", digest_size, out_size);
        err = -1;
        goto hash_out;
    }
    err = crypto_shash_digest(desc, key, key_len, out);
hash_out:
    free_shash_desc(desc);
    return err;
}