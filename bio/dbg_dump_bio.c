#include "bio.h"
#include "pr_format.h"
#include <linux/printk.h>

void dbg_dump_bio(const char *prefix, struct bio *bio) {
    pr_debug(
        pr_format(
            "%s"
            "address          =%p\n"
            "refcnt           =%d\n"
            "bdev             =%d,%d\n"
            "opf              =%d (%s)\n"
            "vcnt             =%d\n"
            "iter::sector_size=%lld\n"
            "iter::size=%d\n"),
        prefix,
        bio,
        atomic_read(&bio->__bi_cnt),
        MAJOR(bio->bi_bdev->bd_dev), MINOR(bio->bi_bdev->bd_dev),
        bio->bi_opf, bio_op(bio) == REQ_OP_WRITE ? "REQ_OP_WRITE" : "REQ_OP_READ",
        bio->bi_vcnt,
        bio->bi_iter.bi_sector,
        bio->bi_iter.bi_size
    );
}