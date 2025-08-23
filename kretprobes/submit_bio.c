#include "bio.h"
#include "bio_utils.h"
#include "bnull.h"
#include "hashset.h"
#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "registry_lookup.h"
#include <linux/bio.h>
#include <linux/types.h>

static inline void set_arg1(struct pt_regs *regs, struct bio *arg1) {
#ifdef CONFIG_X86_64
    regs->di = (unsigned long)arg1;
#else
#error "unsupported architecture"
#endif
} 

static void dummy_end_io(struct bio *bio) {
    struct bio *orig_bio = bio->bi_private;
    bio_enqueue(orig_bio);
    bio_put(bio);
}

static struct bio *create_dummy_bio(struct bio *orig_bio) {
    struct block_device *bdev = bnull_get_bdev();
    if (!bdev) {
        pr_debug(pr_format("bnull instance of struct block_device is NULL"));
    }
    struct bio *dummy = bio_alloc(bdev, 0, REQ_OP_DISCARD, GFP_ATOMIC);
    if (!dummy) {
        pr_debug(pr_format("cannot create a dummy bio request for device (%d, %d)\n"), MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev));
        return NULL;
    }
    dummy->bi_end_io = dummy_end_io;
    dummy->bi_iter.bi_sector = 0;
    dummy->bi_iter.bi_size = 0;
    dummy->bi_private = orig_bio;
    return dummy;
}

/**
 * skip_handler returns true if the submit_bio entry handler shouldn't execute, that is a bio:
 * 1. is null or is not a write bio;
 * 2. has been already intercepted by the kretprobe. A bio request could be intercepted twice if it is attempting to write a block that has been never
 *    written before;
 * 3. is attempting to write to a block whose snapshot has been already saved in /snapshots.
 *    skip_handler always returns true in case of errors, if the hashset_* API(s) are misbeheaving, then executing the submit_bio handler could lead to catastrophic
 *    results.
 */
static bool skip_handler(struct bio *bio) {
    if (!bio || !op_is_write(bio->bi_opf)) {
        return true;
    }
    bool present;
    dev_t devno;
    if (!bio_denvo_safe(bio, &devno)) {
        pr_debug(pr_format("cannot read device number from bio struct"));
        return true;
    }
    sector_t sector = bio_sector(bio);
    int err = registry_lookup_sector(devno, sector, &present);
    if (err) {
        if (err != -ENOSSN) {
            pr_debug(pr_format("hashset_add completed with error %d"), err);
        }
        return true;
    }
    if (present) {
        pr_debug(pr_format("bio: dev=%d,%d, sector=%llu is already in the block table"), MAJOR(devno), MINOR(devno), sector);
    } else {
        pr_debug(pr_format("bio: dev=%d,%d, sector=%llu is not in the block table"), MAJOR(devno), MINOR(devno), sector);
    }
    return present;
}

/**
 * This entry handler do the following steps:
 * 1. Checks if the bio should be intercepted. A bio request should be intercepted if it's
 *    a write operation, it's associated to a device previously registered by the user, it hasn't
 *    been already intercepted and it is directed to a region of the device not already hit by another
 *    write request.
 * 2. If a bio request should be intercepted, we cannot submit it to the bio layer because we need to copy the original
 *    content of the region hit by the request before applying the write. We replace the bio pointer in the stack area
 *    (the area where struct pt_regs points to) with another bio request that is directed to the bnull block device
 *    (see the 'bnull' directory for more details). The request is called dummy because it does pratically nothing: it is
 *    an empty read requests that is discarded by the device anyway.
 * 3. The original bio request is submitted to workqueue by bio_enqueue for further processing.
 * 4. The write request should be eventually submitted to the bio layer so this kretprobe will intercept the bio request twice,
 *    and even the second time the latter is eligible to be intercepted so we need to keep track of the requests already intercepted.
 */
int submit_bio_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct bio *bio = get_arg1(struct bio*, regs);
    // skip the return handler if at least one of the following conditions apply:
    // * the request is NULL
    // * the request is not for writing;
    // * the request was sent to a device not registered;
    if (skip_handler(bio)) {
        return 1;
    }
    struct bio *dummy_bio = create_dummy_bio(bio);
    if (dummy_bio) {
        set_arg1(regs, dummy_bio);
    } else {
        pr_debug(pr_format("cannot create dummy bio"));
    }
    return 0;
}
