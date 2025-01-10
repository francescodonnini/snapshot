#include "include/hash.h"
#include "include/pr_format.h"
#include <crypto/hash.h>
#include <linux/printk.h>

struct sdesc {
    struct shash_desc shash;
    char   *ctx;
};

/**
 * hash - calculates hash of key
 * @param alg_name the hashing algorithm to be used -e.g. sha1.
 * @param key string to hash
 * @param key_len number of chars of key
 * @param hash hash
 * @return 0 if the message digest creation was successfull, <0 otherwise (see crypto_shash_* API)
 */
int hash(const char *alg_name, const char *key, int len, char *hash) {
    struct crypto_shash *alg = crypto_alloc_shash(alg_name, CRYPTO_ALG_TYPE_SHASH, 0);
    if (IS_ERR(alg)) {
        return PTR_ERR(alg);
    }
    struct shash_desc shash;
    shash.tfm = alg;
    int err = crypto_shash_digest(&shash, key, len, hash);
    crypto_free_shash(alg);
    return err;
}