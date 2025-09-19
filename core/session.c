#include "session.h"
#include "itree.h"
#include "pr_format.h"
#include <linux/slab.h>

int get_dirname_prefix_len(void) {
    return 11;
}

int get_dirname_len(void) {
    return 32;
}

struct session *session_create(dev_t dev) {
    struct session *s;
    s = kzalloc(sizeof(*s), GFP_ATOMIC);
    if (!s) {
        return NULL;
    }
    ktime_get_ts64(&s->created_on);
    if (itree_create(s)) {
        goto out;
    }
    s->dev = dev;
    pr_debug(pr_format("session %d:%d (uptime %llu sec %ld nsec)"), MAJOR(dev), MINOR(dev), s->created_on.tv_sec, s->created_on.tv_nsec);
    return s;

out:
    kfree(s);
    return NULL;
}

void session_destroy(struct session *s) {
    itree_destroy(s);
    kfree(s);
}