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
    init_rwsem(&r->rw_sem);
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
    return lt->key > rt->key
           ? ((lt->key == rt->key) ? 0 : 1)
           : -1;
}

int rbitmap64_add(struct rbitmap64 *r, uint64_t x, unsigned long *added) {
    return -1;
}