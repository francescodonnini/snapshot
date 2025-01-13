#include "include/hash.h"
#include "include/pr_format.h"
#include <linux/errname.h>
#include <crypto/hash.h>
#include <linux/printk.h>
#include <linux/types.h>

struct sdesc {
    struct shash_desc shash;
    char   *digest;
};

static struct sdesc* sdesc_init(const char *alg_name) {
    int err;
    struct sdesc *sdesc = kmalloc(sizeof(struct shash_desc), GFP_KERNEL);
    if (sdesc == NULL) {
        err = -ENOMEM;
        goto no_sdesc_alloc;
    }
    struct crypto_shash *alg = crypto_alloc_shash(alg_name, CRYPTO_ALG_TYPE_SHASH, 0);
    if (IS_ERR(alg)) {
        err = -ENOMEM;
        goto no_crypto_alloc;
    }
    char *digest = kmalloc(crypto_shash_digestsize(alg), GFP_KERNEL);
    if (digest == NULL) {
        err = -ENOMEM;
        goto no_digest_alloc;
    }
    sdesc->shash.tfm = alg;
    sdesc->digest = digest;
    return sdesc;

no_digest_alloc:
    crypto_free_shash(alg);
no_crypto_alloc:
    kfree(sdesc);
no_sdesc_alloc:
    return ERR_PTR(err);
}

/**
 * hash - calculates hash of key
 * @param alg_name the hashing algorithm to be used -e.g. sha1.
 * @param key string to hash
 * @param key_len number of chars of key
 * @param hash hash
 * @return 0 if the message digest creation was successfull, <0 otherwise (see crypto_shash_* API)
 */
char *hash(const char *alg_name, const char *key, int len) {
    struct sdesc *sdesc = sdesc_init(alg_name);
    if (IS_ERR(sdesc)) {
        return ERR_PTR(PTR_ERR(sdesc));
    }
    
    int err = crypto_shash_digest(&sdesc->shash, key, len, sdesc->digest);
    char *digest = sdesc->digest;
    if (err < 0) {
        digest = ERR_PTR(err);
        kfree(sdesc->digest);
        pr_debug(pr_format("cannot create hash digest (%d): %s\n"), err, errname(err));
    }
    crypto_free_shash(sdesc->shash.tfm);
    kfree(sdesc);
    return digest;
}