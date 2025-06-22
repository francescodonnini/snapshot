#include "include/pr_format.h"
#include "include/probes.h"
#include "include/kretprobe_handlers.h"
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/kprobes.h>
#include <linux/list.h>

static struct kprobe kprobe_table[] = {
};
static size_t KPROBES_NUM = (sizeof(kprobe_table) / sizeof(struct kprobe));

static int try_register_kprobe(struct kprobe *kp) {
    int err = register_kprobe(kp);
    if (err) {
        pr_debug(pr_format("cannot register kprobe for vfs_write, got error %d\n"), err);
    }
    return err;
}

static int register_kprobes(void) {
    int i, err = 0;
    for (i = 0; i < KPROBES_NUM && !err; ++i) {
        err = try_register_kprobe(&kprobe_table[i]);
    }
    if (err) {
        unregister(kprobe_table, i);
        return 0;
    }
    pr_debug(pr_format("all kprobes have been registered successfully!"));
    return 0;
}

static struct kretprobe kretprobe_table[] = {
    {.kp={.symbol_name="mount_bdev"}, .entry_handler=mount_bdev_entry_handler, .handler=mount_bdev_handler}
};
static size_t KRETPROBES_NUM = (sizeof(kretprobe_table) / sizeof(struct kretprobe));

static int try_register_kretprobe(struct kretprobe *kp) {
    int err = register_kprobe(kp);
    if (err) {
        pr_debug(pr_format("cannot register kprobe for vfs_write, got error %d\n"), err);
    }
    return err;
}

static int register_kretprobes(void) {
    int i, err = 0;
    for (i = 0; i < KPROBES_NUM && !err; ++i) {
        err = try_register_kretprobe(&kprobe_table[i]);
    }
    if (err) {
        unregister_kretprobe_until(kprobe_table, i);
        return 0;
    }
    pr_debug(pr_format("all kprobes have been registered successfully!"));
    return 0;
}

static void unregister_kprobe_until(struct kprobe *table, int n) {
    while (--n >= 0) {
        unregister_kprobe(&table[n]);
    }
}

static void unregister_kretprobe_until(struct kretprobe *table, int n) {
    while (--n >= 0) {
        unregister_kretprobe(&table[n]);
    }
}

static inline void unregister_kprobe_all(void) {
    unregister_kprobe_until(kprobe_table, KPROBES_NUM);
}

static inline void unregister_kretprobe_all(void) {
    unregister_kretprobe_until(kretprobe_table, KRETPROBES_NUM);
}

static void unregister_all(void) {
    unregister_kprobe_all();
    unregister_kretprobe_all();
}

int probes_init(void) {
    int err = register_kprobes();
    if (err) return 0;
    err = register_kretprobes();
    if (err) {
        unregister_kprobe_all();
    }
    return 0;

}

void probes_cleanup(void) {
    unregister_all();
    pr_debug(pr_format("kprobes unregistered!"));
}