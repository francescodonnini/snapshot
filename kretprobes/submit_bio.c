#include "bio.h"
#include "kretprobe_handlers.h"
#include "pr_format.h"
#include "registry_lookup.h"
#include <linux/bio.h>
#include <linux/types.h>

#define BIO_BITMASK (0x8000)

static void dummy_end_io(struct bio *bio) {
    dbg_dump_bio("dummy_end_io called on\n", bio);
    bio_put(bio);
}

static inline void bio_mark(struct bio *bio) {
    bio->bi_flags |= BIO_BITMASK;
}

static inline void bio_unmark(struct bio *bio) {
    bio->bi_flags &= ~BIO_BITMASK;
}

static bool skip_bio(struct bio *bio) {
    bool b = bio->bi_flags & BIO_BITMASK;
    if (b) {
        bio_unmark(bio);
    }
    return b;
}

static void init_dummy_bio(struct bio *bio, struct bio *orig) {
    bio->bi_end_io = dummy_end_io;
    bio->bi_iter.bi_sector = orig->bi_iter.bi_sector;
    bio->bi_iter.bi_size = 0;
}

static struct bio *create_dummy_bio(struct bio *orig_bio) {
    struct bio *dummy = bio_alloc(orig_bio->bi_bdev, 0, REQ_OP_WRITE, GFP_ATOMIC);
    if (!dummy) {
        pr_debug(pr_format("cannot create a dummy bio request for device (%d, %d)\n"), MAJOR(orig_bio->bi_bdev->bd_dev), MINOR(orig_bio->bi_bdev->bd_dev));
        return NULL;
    }
    init_dummy_bio(dummy, orig_bio);
    return dummy;
}

int submit_bio_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    struct bio *bio = get_arg1(struct bio*, regs);
    // skip the return handler if at least one of the following conditions apply:
    // * the request is NULL
    // * the request is not for writing;
    // * the request was sent to a device not registered;
    if (!bio
        || !op_is_write(bio_op(bio))
        || !registry_lookup_mm(bio->bi_bdev->bd_dev)
        || skip_bio(bio)) {
        return 1;
    }
    struct bio *dummy_bio = create_dummy_bio(bio);
    if (!dummy_bio) {
        return 1;
    }
    if (!bio_enqueue(bio)) {
        pr_debug(pr_format("cannot enqueue bio for device (%d, %d)\n"), MAJOR(bio->bi_bdev->bd_dev), MINOR(bio->bi_bdev->bd_dev));
        return 1;
    }
    bio_mark(bio);
    pr_debug(pr_format("submit_bio called on device (%d, %d)\n"), MAJOR(bio->bi_bdev->bd_dev), MINOR(bio->bi_bdev->bd_dev));
    set_arg1(regs, (unsigned long)dummy_bio);
    return 0;
}
