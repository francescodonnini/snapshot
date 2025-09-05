#include "rbitmap32.h"
#include <linux/crypto.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/prandom.h>
#include <linux/printk.h>
#include <linux/types.h>

static int n = 4;
module_param(n, int, 0660);

static struct rnd_state rnd;
static uint32_t *data;

static int init(int n) {
    data = kmalloc_array(n, sizeof(uint32_t), GFP_KERNEL);
    if (!data) {
        return -ENOMEM;
    }
    uint64_t seed = 3141592653589793238ULL;
    prandom_seed_state(&rnd, seed);
    for (int i = 0; i < n; ++i) {
        uint32_t x = prandom_u32_state(&rnd);
        data[i] = x;
    }
    return 0;
}

static int __init rbitmap32_test_start(void) {
    if (init(n)) {
        return -ENOMEM;
    }
    pr_info("test started: n=%d", n);
    int count = 0;
    struct rbitmap32 r;
    for (int32_t i = 0; i < n; ++i) {
        uint32_t x = data[i];
        bool added;
        int err = rbitmap32_add(&r, x, &added);
        if (err < 0) {
            pr_info("cannot add %u to bitmap, got error %d", x, err);
        } else {
            if (added) ++count;
        }
    }
    pr_info("total elements added are %d", count);
    rbitmap32_destroy(&r);
    return 0;
}

static void __exit rbitmap32_test_end(void) {
    kfree(data);
}

MODULE_AUTHOR("Francesco Donnini <donnini.francesco00@gmail.com>");
MODULE_DESCRIPTION("Block-device snapshot");
MODULE_LICENSE("GPL");

module_init(rbitmap32_test_start);
module_exit(rbitmap32_test_end);