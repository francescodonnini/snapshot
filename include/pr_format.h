#ifndef AOS_PR_FORMAT_H
#define AOS_PR_FORMAT_H
#include <linux/module.h>

// adds some informations to the printk format string.
// e.g. printk(pr_format("hello")) prints "[<MODULE NAME>] hello"
#define pr_format(fmt) "[%s] " "" fmt, module_name(THIS_MODULE)

#endif