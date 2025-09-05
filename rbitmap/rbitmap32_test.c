#include "rbitmap32.h"
#include <linux/crypto.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/types.h>

static int __init rbitmap32_test_start(void) {
    uint64_t v1[] = {0, 1, 12, 31, 44, 57}; 
    int32_t n = sizeof(v1) / sizeof(uint64_t);
    struct rbitmap32 r;
    for (int32_t i = 0; i < n; ++i) {
        bool added;
        int err = rbitmap32_add(&r, v1[i], &added);
        if (err < 0) {
            pr_info("cannot add %llu to bitmap, got error %d", v1[i], err);
        } else {
            if (added)
                pr_info("%llu has been added to bitmap successfully", v1[i]);
            else
                pr_info("%llu is already present", v1[i]);
        }
    }
    for (int32_t i = 0; i < n; ++i) {
        bool b = rbitmap32_contains(&r, v1[i]);
        if (b) {
            pr_info("%llu is in bitmap", v1[i]);
        } else {
            pr_info("%llu is not in bitmap", v1[i]);
        }
    }
    uint64_t v2[] = {3, 5, 13, 33, 47, 59};
    n = sizeof(v2) / sizeof(uint64_t);
    for (int32_t i = 0; i < n; ++i) {
        bool b = rbitmap32_contains(&r, v2[i]);
        if (b) {
            pr_info("%llu is in bitmap", v2[i]);
        } else {
            pr_info("%llu is not in bitmap", v2[i]);
        }
    }
    for (int32_t i = 0; i < n; ++i) {
        bool added;
        int err = rbitmap32_add(&r, v1[i], &added);
        if (err < 0) {
            pr_info("cannot add %llu to bitmap, got error %d", v1[i], err);
        } else {
            if (added)
                pr_info("%llu has been added to bitmap successfully", v1[i]);
            else
                pr_info("%llu is already present", v1[i]);
        }
    }
    rbitmap32_destroy(&r);
    return 0;
}

static void __exit rbitmap32_test_end(void) {

}

MODULE_AUTHOR("Francesco Donnini <donnini.francesco00@gmail.com>");
MODULE_DESCRIPTION("Block-device snapshot");
MODULE_LICENSE("GPL");

module_init(rbitmap32_test_start);
module_exit(rbitmap32_test_end);