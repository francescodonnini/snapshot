#include "bio.h"
#include "pr_format.h"
#include <linux/printk.h>
#include <linux/types.h>

static const char *bio_op_str(struct bio *bio)
{
    switch (bio_op(bio)) {
    case REQ_OP_READ:            return "REQ_OP_READ";
    case REQ_OP_WRITE:           return "REQ_OP_WRITE";
    case REQ_OP_FLUSH:           return "REQ_OP_FLUSH";
    case REQ_OP_DISCARD:         return "REQ_OP_DISCARD";
    case REQ_OP_SECURE_ERASE:    return "REQ_OP_SECURE_ERASE";
    case REQ_OP_ZONE_APPEND:     return "REQ_OP_ZONE_APPEND";
    case REQ_OP_WRITE_ZEROES:    return "REQ_OP_WRITE_ZEROES";
    case REQ_OP_ZONE_OPEN:       return "REQ_OP_ZONE_OPEN";
    case REQ_OP_ZONE_CLOSE:      return "REQ_OP_ZONE_CLOSE";
    case REQ_OP_ZONE_FINISH:     return "REQ_OP_ZONE_FINISH";
    case REQ_OP_ZONE_RESET:      return "REQ_OP_ZONE_RESET";
    case REQ_OP_ZONE_RESET_ALL:  return "REQ_OP_ZONE_RESET_ALL";
    case REQ_OP_DRV_IN:          return "REQ_OP_DRV_IN";
    case REQ_OP_DRV_OUT:         return "REQ_OP_DRV_OUT";
    case REQ_OP_LAST:            return "REQ_OP_LAST";
    default:                     return "UNKNOWN_REQ_OP";
    }
}

static inline const char *bool_str(bool b) {
    return b ? "true" : "false";
}

static const char *bio_status_str(struct bio *bio) {
    switch (bio->bi_status) {
    case BLK_STS_OK:                   return "BLK_STS_OK";
    case BLK_STS_NOTSUPP:              return "BLK_STS_NOTSUPP";
    case BLK_STS_TIMEOUT:              return "BLK_STS_TIMEOUT";
    case BLK_STS_NOSPC:                return "BLK_STS_NOSPC";
    case BLK_STS_TRANSPORT:            return "BLK_STS_TRANSPORT";
    case BLK_STS_TARGET:               return "BLK_STS_TARGET";
    case BLK_STS_RESV_CONFLICT:        return "BLK_STS_RESV_CONFLICT";
    case BLK_STS_MEDIUM:               return "BLK_STS_MEDIUM";
    case BLK_STS_PROTECTION:           return "BLK_STS_PROTECTION";
    case BLK_STS_RESOURCE:             return "BLK_STS_RESOURCE";
    case BLK_STS_IOERR:                return "BLK_STS_IOERR";
    case BLK_STS_DM_REQUEUE:           return "BLK_STS_DM_REQUEUE";
    case BLK_STS_AGAIN:                return "BLK_STS_AGAIN";
    case BLK_STS_DEV_RESOURCE:         return "BLK_STS_DEV_RESOURCE";
    case BLK_STS_ZONE_OPEN_RESOURCE:   return "BLK_STS_ZONE_OPEN_RESOURCE";
    case BLK_STS_ZONE_ACTIVE_RESOURCE: return "BLK_STS_ZONE_ACTIVE_RESOURCE";
    case BLK_STS_OFFLINE:              return "BLK_STS_OFFLINE";
    case BLK_STS_DURATION_LIMIT:       return "BLK_STS_DURATION_LIMIT";
    case BLK_STS_INVAL:                return "BLK_STS_INVAL";
    default:                           return "UNKNOWN_BLK_STATUS";
    }
}

void dbg_dump_bio(const char *prefix, struct bio *bio) {
    pr_debug(
        pr_format(
            "%s"
            "address             =%p\n"
            "status              =%s\n"
            "bdev:\n"
            "    maj,min         =(%d,%d)\n"
            "    start_sec,sec_no=%llu,%llu\n"
            "has_next            =%s\n"
            "refcnt              =%d\n"
            "opf                 =%d (%s)\n"
            "vcnt                =%d\n"
            "iter::bvec_done     =%s\n"
            "iter::idx           =%d\n"
            "iter::sector        =%lld\n"
            "iter::size=%u\n"),
        prefix,
        bio,
        bio_status_str(bio),
        MAJOR(bio->bi_bdev->bd_dev), MINOR(bio->bi_bdev->bd_dev),
        bio->bi_bdev->bd_start_sect, bio->bi_bdev->bd_nr_sectors,
        bool_str(bio->bi_next != NULL),
        atomic_read(&bio->__bi_cnt),
        bio_op(bio), bio_op_str(bio),
        bio->bi_vcnt,
        bool_str(bio->bi_iter.bi_bvec_done),
        bio->bi_iter.bi_idx,
        bio->bi_iter.bi_sector,
        bio->bi_iter.bi_size
    );
}