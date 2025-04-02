#include "include/mounts.h"
#include "include/pr_format.h"
#include <linux/namei.h>
#include <linux/path.h>

int find_mount(const char *dev_name, bool *found) {
    struct path path;
    int err = kern_path(dev_name, LOOKUP_FOLLOW, &path);
    if (err) {
        pr_debug(pr_format("kern_path() failed because of error %d\n"), err);
        return err;
    }
    *found = path_is_mountpoint(&path);
    return 0;
}