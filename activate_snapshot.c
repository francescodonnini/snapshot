#include "include/api.h"
#include "include/find_mount.h"
#include "include/pr_format.h"
#include "include/registry.h"
#include <linux/printk.h>

int activate_snapshot(const char *dev_name, const char *password) {
    bool is_mounted;
    int err = find_mount(dev_name, &is_mounted);
    if (err || is_mounted) {
        pr_debug(pr_format("device %s is already mounted\n"), dev_name);
        return -EALRDYMNTD;
    }
    err = registry_insert(dev_name, password);
    if (err) {
        pr_debug(pr_format("cannot insert %s because of error %d\n"), dev_name, err);
        return err;
    }
    pr_debug(pr_format("activate_snapshot(%s)\n"), dev_name);
    return 0;
}