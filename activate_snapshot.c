#include "include/api.h"
#include "include/pr_format.h"
#include <linux/printk.h>

int activate_snapshot(const char *dev_name, const char *password) {
    pr_debug(ss_pr_format("activate_snapshot(%s, %s)\n"), dev_name, password);
    return 0;
}