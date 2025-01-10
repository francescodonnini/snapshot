#include "include/hash.h"
#include "include/pr_format.h"
#include <crypto/hash.h>
#include <linux/printk.h>
#include <linux/scatterlist.h>

struct sdesc {
    struct shash_desc shash;
    char   *ctx;
};

static struct sdesc *init_sdesc(struct crypto_shash *alg) {
    int size = sizeof(struct shash_desc) + crypto_shash_descsize(alg);
    struct sdesc *sdesc = kmalloc(size, GFP_KERNEL);
    if (!sdesc) {
        return ERR_PTR(-ENOMEM);
    }
    sdesc->shash.tfm = alg;
    return sdesc;
}

static int do_hash(struct crypto_shash *alg, const unsigned char *key, unsigned int len, unsigned char *digest) {
    struct sdesc *sdesc = init_sdesc(alg);
    if (IS_ERR(sdesc)) {
        return PTR_ERR(sdesc);
    }
    int err = crypto_shash_digest(&sdesc->shash, key, len, digest);
    kfree(sdesc);
    return err;
}

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
    int err = do_hash(alg, key, len, hash);
    crypto_free_shash(alg);
    return err;
}