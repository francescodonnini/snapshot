#include "include/pr_format.h"
#include "include/probes.h"
#include "include/kprobe_handlers.h"
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/kprobes.h>
#include <linux/list.h>

static struct kprobe KPROBES_TABLE[] = {
    {.symbol_name="vfs_write", .pre_handler=vfs_write_pre_handler},
    {.symbol_name="mount_bdev", .pre_handler=mount_bdev_pre_handler},
};
static size_t KPROBES_TABLE_SIZE = (sizeof(KPROBES_TABLE) / sizeof(struct kprobe));

static int try_register(struct kprobe *kp) {
    int err = register_kprobe(kp);
    if (err) {
        pr_debug(pr_format("cannot register kprobe for vfs_write, got error %d\n"), err);
    }
    return err;
}

static void unregister(struct kprobe *table, int n) {
    while (--n >= 0) {
        unregister_kprobe(&table[n]);
    }
}

int probes_init(void) {
    int i, err = 0;
    for (i = 0; i < KPROBES_TABLE_SIZE; ++i) {
        err = try_register(&KPROBES_TABLE[i]);
        if (err) {
            break;
        }
    }
    if (err) {
        unregister(KPROBES_TABLE, i);
        return err;
    }
    return 0;
}

static void unregister_all(void) {
    unregister(KPROBES_TABLE, KPROBES_TABLE_SIZE - 1);
}

void probes_cleanup(void) {
    unregister_all();
}