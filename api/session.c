#include "session.h"
#include "iset.h"
#include "itree.h"
#include "pr_format.h"
#include <linux/slab.h>
#include <linux/uuid.h>

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
    size_t size = sizeof(*s) + ALIGN(UUID_STRING_LEN + 1, sizeof(void*));
    s = kzalloc(size, GFP_ATOMIC);
    if (!s) {
        return NULL;
    }
    s->id = (char*)s + sizeof(*s);
    if (gen_uuid(s->id, UUID_STRING_LEN + 1)) {
        goto out;
    }
    int err = iset_create(s);
    if (err) {
        goto out;
    }
    err = itree_create(s);
    if (err) {
        goto out2;
    }
    s->dev = dev;
    s->has_dir = false;
    s->mntpoints = 1;
    return s;

out2:
    iset_destroy(s);
out:
    kfree(s);
    return NULL;
}

void session_destroy(struct session *s) {
    iset_destroy(s);
    itree_destroy(s);
    kfree(s);
}