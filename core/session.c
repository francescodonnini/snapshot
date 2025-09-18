#include "session.h"
#include "itree.h"
#include "pr_format.h"
#include <linux/slab.h>
#include <linux/uuid.h>

/**
 * get_session_id_len return the number of bytes required to hold a session id without the NUL terminator
 */
int get_session_id_len(void) {
    return UUID_STRING_LEN;
}

static int gen_uuid(char *out, size_t n) {
    uuid_t uuid;
    uuid_gen(&uuid);
    int err = snprintf(out, n, "%pUb", &uuid);
    if (err >= n) {
        pr_debug(pr_format("cannot parse uuid"));
        return -1;
    }
    return 0;
}

struct session *session_create(dev_t dev) {
    struct session *s;
    s = kzalloc(sizeof(*s) + get_session_id_len() + 1, GFP_ATOMIC);
    if (!s) {
        return NULL;
    }
    if (gen_uuid(s->id, get_session_id_len() + 1)) {
        goto out;
    }
    ktime_get_ts64(&s->created_on);
    if (itree_create(s)) {
        goto out;
    }
    s->dev = dev;
    pr_debug(pr_format("session %s,%d:%d (uptime %llu sec %ld nsec)"), s->id, MAJOR(dev), MINOR(dev), s->created_on.tv_sec, s->created_on.tv_nsec);
    return s;

out:
    kfree(s);
    return NULL;
}

void session_destroy(struct session *s) {
    itree_destroy(s);
    kfree(s);
}