#include "api.h"
#include "auth.h"
#include "pr_format.h"
#include "registry.h"
#include <linux/printk.h>

int activate_snapshot(const char *dev_name, const char *password) {
    if (auth_check_password(password)) {
        return -EWRONGCRED;
    }
    return registry_insert(dev_name);
}