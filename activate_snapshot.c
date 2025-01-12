#include "include/api.h"
#include "include/pr_format.h"
#include "include/registry.h"
#include <linux/printk.h>

int activate_snapshot(const char *dev_name, const char *password) {
    pr_debug(ss_pr_format("activate_snapshot(%s, %s)\n"), dev_name, password);
    int err = registry_insert(dev_name, password);
    if (err) {
        pr_debug(ss_pr_format("cannot insert %s, %s because of error %d\n"), dev_name, password, err);
        return err;
    }
    return 0;
}