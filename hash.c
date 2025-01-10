#include "include/hash.h"
#include "include/pr_format.h"
#include <crypto/hash.h>
#include <linux/printk.h>
#include <linux/scatterlist.h>

/**
 * hash - calculates hash of key
 * @param alg_name the hashing algorithm to be used -e.g. sha1.
 * @param key string to hash
 * @param key_len number of chars of key
 * @param hash hash
 * @return 0 if the message digest creation was successfull, <0 otherwise (see crypto_shash_* API)
 */
int hash(const char *alg_name, const char *key, int key_len, char *hash) {
    int err;
    struct shash_desc desc;
    desc.tfm = crypto_alloc_shash(alg_name, 0, CRYPTO_ALG_TYPE_SHASH);
    if (IS_ERR(desc.tfm)) {
        printk(ss_pr_format("crypto_alloc_shash() failed."));
        return PTR_ERR(desc.tfm);
    }
    err = crypto_shash_init(&desc);
    if (err) {
        printk(ss_pr_format("crypto_shash_init failed"));
        goto free_shash;
    }
    err = crypto_shash_update(&desc, key, key_len);
    if (err) {
        printk(ss_pr_format("crypto_shash_update() failed"));
        goto free_shash;
    }
    err = crypto_shash_final(&desc, hash);
    if (err) {
        printk(ss_pr_format("crypto_shash_final() failed"));
        goto free_shash;
    }

free_shash:
    crypto_free_shash(desc.tfm);
    return err;
}