#include "include/hash.h"
#include "include/pr_format.h"
#include <linux/errname.h>
#include <crypto/hash.h>
#include <linux/printk.h>
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
        goto no_hash_alloc;
    }
    struct shash_desc desc = {
        .tfm = alg
    };
    err = crypto_shash_init(&desc);
    if (err < 0) {
        goto no_hash;
    }
    err = crypto_shash_update(&desc, key, len);
    if (err < 0) {
        goto no_hash;
    }
    char *out = kmalloc(crypto_shash_digestsize(desc.tfm), GFP_KERNEL);
    if (out == NULL) {
        err = -ENOMEM;
        goto no_hash;
    }
    err = crypto_shash_final(&desc, out);
    if (err < 0) {
        goto no_hash_final;
    }
    
    pr_debug(pr_format("hash digest of size %d is:\n"), crypto_shash_digestsize(desc.tfm));
    for (size_t i = 0; i < crypto_shash_digestsize(desc.tfm); ++i) {
        printk("%02x", out[i]);
    }
    crypto_free_shash(desc.tfm);    
    return out;

no_hash_final:
    kfree(out);
no_hash:
    crypto_free_shash(desc.tfm);
no_hash_alloc:
    return ERR_PTR(err);
}