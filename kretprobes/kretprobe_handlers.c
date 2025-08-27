#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "probes.h"
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/kprobes.h>
#include <linux/list.h>

static struct kretprobe submit_bio_kretprobe = {
    .kp.symbol_name = "submit_bio",
    .entry_handler = submit_bio_entry_handler,
};

static struct kretprobe mount_bdev_kretprobe = {
    .kp.symbol_name = "mount_bdev",
    .entry_handler = mount_bdev_entry_handler,
    .handler = mount_bdev_handler,
    .data_size = sizeof(char**)
};

static struct kretprobe umount_kretprobe = {
    .kp.symbol_name = "path_umount",
    .entry_handler = path_umount_entry_handler,
    .handler = path_umount_handler,
    .data_size = sizeof(dev_t)
};

static struct kretprobe get_tree_bdev_kretprobe = {
    .kp.symbol_name = "get_tree_bdev",
    .entry_handler = get_tree_entry_handler,
    .handler = get_tree_handler,
    .data_size = sizeof(struct fs_context*),
};

static struct kretprobe ext4_fill_super_kretprobe = {
    .kp.symbol_name = "ext4_fill_super",
    .entry_handler = ext4_fill_super_entry_handler,
    .handler = ext4_fill_super_handler,
    .data_size = sizeof(dev_t),
};

static struct kretprobe *kretprobe_table[] = {
    &submit_bio_kretprobe,
    &mount_bdev_kretprobe,
    &umount_kretprobe,
    &get_tree_bdev_kretprobe,
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
    pr_debug(pr_format("submit_bio: #missed=%d"), submit_bio_kretprobe.nmissed);
    unregister_kretprobes(kretprobe_table, KRETPROBES_NUM);
}