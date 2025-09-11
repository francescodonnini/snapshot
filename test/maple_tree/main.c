#include <linux/maple_tree.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>

struct b_range {
    int lo, hi;
};

static struct b_range data[] = {
    {.lo=16, .hi=24},
};
static int n = sizeof(data) / sizeof(struct b_range);

static int insert_all(struct maple_tree *t) {
    for (int i = 0; i < n; ++i) {
        struct b_range *r = &data[i];
        int err = mtree_store_range(t, r->lo, r->hi, r, GFP_KERNEL);
        if (err) {
            pr_err("cannot insert range [%d, %d), got error %d", r->lo, r->hi, err);
            return err;
        }
    }
    return 0;
}

static void update_span(struct b_range *q, struct b_range *r, unsigned long *span) {
    if (q->lo >= r->lo && q->hi <= r->hi) {
        q->lo = q->hi;
    } else if (q->lo >= r->lo) {
        q->lo = r->hi;
    } else if (q->hi <= r->hi) {
        q->hi = r->lo;
    }
    *span = q->hi - q->lo;
    pr_info("update_span: span=%lu", *span);
}

static bool mt_covers(struct maple_tree *tree, struct b_range *q) {
    unsigned long span = 1;
    struct b_range tmp;
    memcpy(&tmp, q, sizeof(struct b_range));
    struct b_range *r;
    unsigned long index = q->lo;
    mt_for_each(tree, r, index, q->hi) {
        update_span(&tmp, r, &span);
    }
    return !span;
}

static void mt_test(struct maple_tree *tree, int test_no, struct b_range *query) {
    char *result = mt_covers(tree, query) ? "yes" : "no";
    pr_info("test %d: is query=[%d, %d) fully covered? %s", test_no, query->lo, query->hi, result);
}

static int __init mtree_test_init(void) {
    struct maple_tree tree;
    mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE | MT_FLAGS_USE_RCU);
    int err = insert_all(&tree);
    if (err) goto out;

    struct b_range queries[] = {
        {.lo=8, .hi=16},
    };
    for (int i = 0; i < sizeof(queries)/sizeof(struct b_range); ++i) {
        mt_test(&tree, i+1, &queries[i]);
    }

out:
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