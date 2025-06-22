#include "include/pr_format.h"
#include "include/probes.h"
#include "include/kretprobe_handlers.h"
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/kprobes.h>
#include <linux/list.h>

static struct kprobe *kprobe_table[] = {
};
static size_t KPROBES_NUM = sizeof(kprobe_table) / sizeof(struct kprobe*);

static struct kretprobe mount_bdev_kretp = {.kp={.symbol_name="mount_bdev"}, .entry_handler=mount_bdev_entry_handler, .handler=mount_bdev_handler};

static struct kretprobe *kretprobe_table[] = {
    &mount_bdev_kretp
};
static size_t KRETPROBES_NUM = sizeof(kretprobe_table) / sizeof(struct kretprobe*);

static int register_all_kprobes(void) {
    if (KPROBES_NUM <= 0) {
        return 0;
    }
    int err = register_kprobes(kprobe_table, KPROBES_NUM);
    if (err) {
        pr_debug(pr_format("cannot register kprobes\n"));
    }
    return err;
}

static int register_all_kretprobes(void) {
    if (KRETPROBES_NUM <= 0) {
        return 0;
    }
    int err = register_kretprobes(kretprobe_table, KRETPROBES_NUM);
    if (err) {
        pr_debug(pr_format("cannot register kretprobes\n"));
    }
    return err;
}
 
int probes_init(void) {
    int err = register_all_kprobes();
    if (err) {
        return err;
    }
    err = register_all_kretprobes();
    if (err) {
        return err;
    }
    return 0;

}

void probes_cleanup(void) {
    unregister_kprobes(kprobe_table, KPROBES_NUM);
    unregister_kretprobes(kretprobe_table, KRETPROBES_NUM);
    pr_debug(pr_format("kprobes unregistered!"));
}