#ifndef __SNAPSHOT_PR_FORMAT_H__
#define __SNAPSHOT_PR_FORMAT_H__
#include <linux/module.h>

// adds some informations to the printk format string.
// e.g. printk(ss_pr_format("hello")) prints "[<MODULE NAME>:<FUNCTION NAME>:<LINE NUMBER>] hello"
#define ss_pr_format(fmt) "[%s:%s:%d] " "" fmt, module_name(THIS_MODULE), __func__, __LINE__

#endif