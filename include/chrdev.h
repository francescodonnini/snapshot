#ifndef AOS_CHRDEV_H
#define AOS_CHRDEV_H
#include <linux/types.h>

int chrdev_init(void);

void chrdev_cleanup(void);

#endif