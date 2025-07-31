#ifndef AOS_BDGET_H
#define AOS_BDGET_H
#include <linux/blk_types.h>

struct block_device* bdget(void);

#endif /* AOS_BDGET_H */