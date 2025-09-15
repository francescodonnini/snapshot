#ifndef AOS_UPDATE_SESSION_H
#define AOS_UPDATE_SESSION_H
#include <linux/blk_types.h>

int ext4_update_session(struct file *bd_file);

int singlefilefs_update_session(struct file *bd_file);

#endif