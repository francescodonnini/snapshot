#ifndef AOS_SNAPSHOT_PR_FORMAT_H
#define AOS_SNAPSHOT_PR_FORMAT_H
#include <linux/module.h>

// adds some informations to the printk format string.
// e.g. printk(ss_pr_format("hello")) prints "[<MODULE NAME>] hello"
#define ss_pr_format(fmt) "[%s] " "" fmt, module_name(THIS_MODULE)

#endif