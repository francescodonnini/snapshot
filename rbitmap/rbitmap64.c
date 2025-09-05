#include "rbitmap64.h"
#include "rbitmap32.h"
#include <linux/slab.h>
#include <linux/wordpart.h>

struct container_u32 {
    struct rb_node    rb_node;
    uint32_t          key;
    struct rbitmap32 *bitmap;
};

int rbitmap64_init(struct rbitmap64 *r) {
    mutex_init(r->lock);
    r->root = RB_ROOT_CACHED;
    return 0;
}

struct container_u32* container_u32_alloc(uint32_t key) {
    struct container_u32 *c;
    c = kmalloc(sizeof(*c), GFP_KERNEL);
    if (!c) {
        return NULL;
    }
    c->key = key;
    return c;
}

static int cmp_u32(const struct rb_node *ltn, const struct rb_node *rtn) {
    const struct container_u32 *lt = rb_entry(ltn, struct container_u32, rb_node);
    const struct container_u32 *rt = rb_entry(rtn, struct container_u32, rb_node);
    if (lt->key < rt->key) {
        return -1;
    }
    if (lt->key > rt->key) {
        return 1;
    }
    return 0;    
}

int rbitmap64_add(struct rbitmap64 *r, uint64_t x) {
    struct container_u32 *hh = container_u32_alloc(upper_32_bits(x));
    if (!hh) {
        return -ENOMEM;
    }
    struct rb_node *exist = rb_find_add_cached(&hh->rb_node, &r->root, cmp_u32);
    if (exist) {
        return rbitmap32_add(r, lower_32_bits(x));
    }
    hh->bitmap = rbitmap32_alloc();
    if (!hh->bitmap) {
        return -ENOMEM;
    }
    return rbitmap32_add(hh->bitmap, lower_32_bits(x))
}

int rbitmap64_contains(struct rbitmap64 *r, uint64_t x) {
    struct rb_node *rb_node = rb_find(&upper_32_bits(x), &r->root, cmp_u32);
    if (!rb_node) {
        return 0;
    }
    struct container_u32 *hh = rb_entry(rb_node, struct container_u32, rb_node);
    return rbitmap32_contains(hh->bitmap, lower_32_bits(x));
}