#ifndef RBITMAP64_H
#define RBITMAP64_H
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/stddef.h>
#include <linux/types.h>

struct rbitmap64 {
    struct mutex          lock; 
    struct rb_root_cached root;
};

int rbitmap64_init(struct rbitmap64 *r);

int rbitmap64_add(struct rbitmap64 *r, uint64_t x);

int rbitmap64_contains(struct rbitmap64 *r, uint64_t x);

#endif