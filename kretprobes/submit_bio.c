#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "registry_lookup.h"
#include <linux/bio.h>

int submit_bio_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct bio *bio = get_arg1(struct bio*, regs);
    if (!bio) {
        pr_debug(pr_format("submit_bio called with NULL bio\n"));
    }
    struct block_device *bdev = bio->bi_bdev;
    if (!registry_lookup_mm(bdev->bd_dev)) {
        // skip the return handler if the device is not registered
        return 1;
    }
    struct submit_bio_data *data = (struct submit_bio_data*)kp->data;
    data->dev = bdev->bd_dev;
    pr_debug(pr_format("submit_bio called on device (%d, %d)\n"), MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev));
    return 0;
}

int submit_bio_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct submit_bio_data *data = (struct submit_bio_data*)kp->data;
    pr_debug(pr_format("submit_bio completed for (%d, %d)\n"), MAJOR(data->dev), MINOR(data->dev));
    return 0;
}