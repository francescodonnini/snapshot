#include <linux/maple_tree.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>

struct b_range {
    int a, b;
};

static struct b_range data[] = {
    {.a=0, .b=100},
    {.a=135, .b=200},
    {.a=201, .b=250},
};
static int n = sizeof(data) / sizeof(struct b_range);

static int insert_all(struct maple_tree *t) {
    for (int i = 0; i < n; ++i) {
        struct b_range *r = &data[i];
        int err = mtree_store_range(t, r->a, r->b, r, GFP_KERNEL);
        if (err) {
            pr_err("cannot insert range [%d, %d), got error %d", r->a, r->b, err);
            return err;
        }
    }
    return 0;
}

static int __init mtree_test_init(void) {
    struct maple_tree tree;
    mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE | MT_FLAGS_USE_RCU);
    int err = insert_all(&tree);
    if (err) goto out;

    unsigned long idx = 25;
    unsigned long end = 125;
    bool in = false;
    struct b_range *r_prev = NULL;
    struct b_range *r;
    mt_for_each(&tree, r, idx, end) {
        if (r_prev) {
            if (r_prev->b < r->a) {
                break;
            }
        }
        pr_info("[%d, %d)", r->a, r->b);
        if (end <= r->b) {
            in = true;
            break;
        }
        r_prev = r;
    }
out:
    pr_info("%d", in);
    mtree_destroy(&tree);
    return err;
}

static void __exit mtree_test_exit(void) {
}

MODULE_AUTHOR("Francesco Donnini <donnini.francesco00@gmail.com>");
MODULE_DESCRIPTION("Maple Tree API Test");
MODULE_LICENSE("GPL");

module_init(mtree_test_init);
module_exit(mtree_test_exit);