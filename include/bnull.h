#ifndef BNULL_H
#define BNULL_H

int bnull_init(void);

void bnull_cleanup(void);

struct block_device *bnull_get_bdev(void);

#endif