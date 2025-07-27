#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "probes.h"
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/kprobes.h>
#include <linux/list.h>

static struct kretprobe submit_bio_kretprobe = {
    .kp.symbol_name = "submit_bio",
    .handler = submit_bio_handler,
    .entry_handler = submit_bio_entry_handler,
    .data_size = sizeof(struct submit_bio_data),
};

static struct kretprobe mount_bdev_kretprobe = {
    .kp.symbol_name = "mount_bdev",
    .handler = mount_bdev_handler,
    .entry_handler = mount_bdev_entry_handler,
    .data_size = sizeof(struct mount_bdev_data)
};

static struct kretprobe *kretprobe_table[] = {
    &submit_bio_kretprobe,
    &mount_bdev_kretprobe
};
static size_t KRETPROBES_NUM = sizeof(kretprobe_table) / sizeof(struct kretprobe *);

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
    int err = register_all_kretprobes();
    if (err) {
        return err;
    }
    return 0;
}

void probes_cleanup(void) {
    unregister_kretprobes(kretprobe_table, KRETPROBES_NUM);
    pr_debug(pr_format("kprobes unregistered!"));
}