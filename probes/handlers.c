#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "probes.h"
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/fs_context.h>
#include <linux/kprobes.h>
#include <linux/list.h>
#include <linux/types.h>

static struct kretprobe ext4_fill_super_kretprobe = {
    .kp.symbol_name = "ext4_fill_super",
    .entry_handler = ext4_fill_super_entry_handler,
    .handler = ext4_fill_super_handler,
    .data_size = sizeof(struct file*),
};

static struct kretprobe kill_block_super_kretprobe = {
    .kp.symbol_name = "kill_block_super",
    .entry_handler = kill_block_super_entry_handler,
    .handler = kill_block_super_handler,
    .data_size = sizeof(dev_t),
};

static struct kretprobe submit_bio_kretprobe = {
    .kp.symbol_name = "submit_bio",
    .entry_handler = submit_bio_entry_handler,
};

static struct kretprobe singlefilefs_fill_super_kretprobe = {
    .kp.symbol_name = "singlefilefs_fill_super",
    .entry_handler = singlefilefs_fill_super_entry_handler,
    .handler = singlefilefs_fill_super_handler,
    .data_size = sizeof(struct file*),
};

static struct kretprobe *kretprobe_table[] = {
    &singlefilefs_fill_super_kretprobe,
    &ext4_fill_super_kretprobe,
    &kill_block_super_kretprobe,
    &submit_bio_kretprobe,
};
static const size_t KRETPROBES_NUM = sizeof(kretprobe_table) / sizeof(struct kretprobe*);

int probes_init(void) {
    if (KRETPROBES_NUM <= 0) {
        return 0;
    }
    int err = register_kretprobes(kretprobe_table, KRETPROBES_NUM);
    if (err) {
        pr_err("cannot register kretprobes, got error %d", err);
    }
    return err;
}

void probes_cleanup(void) {
    for (int i = 0; i < KRETPROBES_NUM; ++i) {
        struct kretprobe *kp = kretprobe_table[i];
        pr_debug(pr_format("%s: #missed=%d"), kp->kp.symbol_name, kp->nmissed);
    }
    unregister_kretprobes(kretprobe_table, KRETPROBES_NUM);
}