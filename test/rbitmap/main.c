#include "../../rbitmap/rbitmap32.h"
#include <linux/maple_tree.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/prandom.h>
#include <linux/printk.h>
#include <linux/slab.h>

static uint32_t         *data;
static size_t            n = 300000;
static struct rnd_state  rnd;
static uint64_t          seed = 3141592653589793238ULL;

static int init(void) {
    data = kmalloc_array(n, sizeof(uint32_t), GFP_KERNEL);
    if (!data) {
        return -ENOMEM;
    }

    prandom_seed_state(&rnd, seed);
	for (size_t i = 0; i < n; i++) {
        data[i] = prandom_u32_state(&rnd);
	}    
    return 0;
}

static int __init rbitmap32_test_init(void) {
    int err = init();
    if (err) {
        goto out;
    }
        
    struct rbitmap32 map;
    err = rbitmap32_init(&map);
    if (err) {
        goto out;
    }
    size_t n1 = 0;
    for (size_t i = 0; i < n; ++i) {
        bool added;
        err = rbitmap32_add(&map, data[i], &added);
        if (err) {
            break;
        }
        if (added) ++n1;
    }
    for (size_t i = 0; i < n; ++i) {
        if (!rbitmap32_contains(&map, data[i])) {
            pr_err("test failed: %u should be in the bitmap!", data[i]);
        }
    }
    size_t bytes = 0;
    size_t n2 = 0;
    for (size_t i = 0; i < 16; ++i) {
        struct rcontainer *c = &map.containers[i];
        switch (c->c_type) {
            case ARRAY_CONTAINER:
                n2 += c->array->size;
                bytes += sizeof(struct array16) + sizeof(uint16_t) * c->array->size;
                pr_info("(array) size=%d", c->array->size);
                break;
            case BITSET_CONTAINER:
                n2 += c->bitset->size;
                bytes += sizeof(*c->bitset);
                pr_info("(bitset) size=%d", c->bitset->size);
                break;
        }
    }
    pr_info("inserted %lu items, counted %lu items", n1, n2);
    pr_info("total bytes required %lu", bytes);
    rbitmap32_destroy(&map);
    kfree(data);
out:
    return err;
}

static void __exit rbitmap32_test_exit(void) {
}

MODULE_AUTHOR("Francesco Donnini <donnini.francesco00@gmail.com>");
MODULE_DESCRIPTION("Maple Tree API Test");
MODULE_LICENSE("GPL");

module_init(rbitmap32_test_init);
module_exit(rbitmap32_test_exit);