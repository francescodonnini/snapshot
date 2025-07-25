#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "linux/bio.h"

int submit_bio_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct bio *bio = get_arg1(struct bio*, regs);
    if (!bio) {
        pr_debug(pr_format("submit_bio called with NULL bio\n"));
    }
    struct block_device *bdev = bio->bi_bdev;
    pr_debug(pr_format("submit_bio called on device (%d, %d)\n"), MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev));
    return 0;
}

int submit_bio_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    return 0;
}