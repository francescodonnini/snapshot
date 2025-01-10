#include <linux/init.h>
#include <linux/module.h>

static void __exit snapshot_exit(void) {

}

module_exit(snapshot_exit);
