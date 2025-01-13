#include "include/api.h"
#include "include/pr_format.h"
#include "include/registry.h"
#include <linux/printk.h>

int activate_snapshot(const char *dev_name, const char *password) {
    int err = registry_insert(dev_name, password);
    if (err) {
        pr_debug(pr_format("cannot insert %s because of error %d\n"), dev_name, err);
        return err;
    }
    pr_debug(pr_format("activate_snapshot(%s)\n"), dev_name);
    return 0;
}