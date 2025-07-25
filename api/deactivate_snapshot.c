#include "api.h"
#include "pr_format.h"
#include "registry.h"
#include <linux/printk.h>

int deactivate_snapshot(const char *dev_name, const char *password) {
    return registry_delete(dev_name, password);
}