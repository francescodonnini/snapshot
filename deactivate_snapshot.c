#include "include/api.h"
#include "include/pr_format.h"
#include <linux/printk.h>

int deactivate_snapshot(const char *dev_name, const char *password) {
    pr_debug(pr_format("deactivate_snapshot(%s, %s)\n"), dev_name, password);
    
    return 0;
}