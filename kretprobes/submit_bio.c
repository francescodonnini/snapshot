#include "bio.h"
#include "bio_utils.h"
#include "bnull.h"
#include "hashset.h"
#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "registry_lookup.h"
#include <linux/bio.h>
#include <linux/types.h>

// The last bit of the bitmap struct bio::bi_flags is not currently used
// by the bio layer so we can use it freely
#define BIO_BITMASK (0x8000)

static void dummy_end_io(struct bio *bio) {
    bio_put(bio);
}

static inline void bio_mark(struct bio *bio) {
    bio->bi_flags |= BIO_BITMASK;
}

static inline void bio_unmark(struct bio *bio) {
    bio->bi_flags &= ~BIO_BITMASK;
}

static bool discard_bio(struct bio *bio) {
    bool found;
    int err = hashset_add(bio_devnum(bio), bio_sector(bio), &found);
    if (err) {
        pr_debug(pr_format("hashset_add completed with error %d"), err);
        return false;
    }
    if (found) {
        pr_debug(pr_format("discarded bio with dev=%d and sector=%llu"), bio_devnum(bio), bio_sector(bio));
    }
    return found;
}

/**
 * skip_bio returns true if the bio has been already intercepted (and scheduled for further processing)
 * by the kretprobe, false otherwise. When skip_bio detects that the bio request has been alread seen by the
 * kretprobe it resets the flag (and returns true).
 */
static bool skip_bio(struct bio *bio) {
    // b is true if the bio was previously marked (intercepted by the kretprobe)
    bool b = bio->bi_flags & BIO_BITMASK;
    if (b) {
        bio_unmark(bio);
    }
    return b;
}

static void init_dummy_bio(struct bio *bio) {
    bio->bi_end_io = dummy_end_io;
}

static struct bio *create_dummy_bio(struct bio *orig_bio) {
    struct bio *dummy = bio_alloc(bnull_get_bdev(), 0, REQ_OP_READ, GFP_ATOMIC);
    if (!dummy) {
        pr_debug(pr_format("cannot create a dummy bio request for device (%d, %d)\n"), MAJOR(orig_bio->bi_bdev->bd_dev), MINOR(orig_bio->bi_bdev->bd_dev));
        return NULL;
    }
    init_dummy_bio(dummy);
    return dummy;
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
 *    One way to do this is to set to 1 a flag of the bio structure that is currently (Linux 6.15) not used by the bio layer.
 */
int submit_bio_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct bio *bio = get_arg1(struct bio*, regs);
    // skip the return handler if at least one of the following conditions apply:
    // * the request is NULL
    // * the request is not for writing;
    // * the request was sent to a device not registered;
    if (!bio
        || !op_is_write(bio_op(bio))
        || !registry_lookup_mm(bio_devnum(bio))
        || discard_bio(bio)
        || skip_bio(bio)) {
        return 1;
    }
    struct bio *dummy_bio = create_dummy_bio(bio);
    if (!dummy_bio) {
        return 1;
    }
    if (!bio_enqueue(bio)) {
        pr_debug(pr_format("cannot enqueue bio for device (%d, %d)\n"), MAJOR(bio_devnum(bio)), MINOR(bio_devnum(bio)));
        return 1;
    }
    // if we made this far, it means that the kretprobe handler hasn't already seen the bio request
    bio_mark(bio);
    set_arg1(regs, (unsigned long)dummy_bio);
    return 0;
}
