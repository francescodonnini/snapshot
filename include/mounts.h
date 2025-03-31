#ifndef AOS_MOUNTS_H
#define AOS_MOUNTS_H
#define EALRDYMNTD 5003

int init_procfs(void);

int find_mount(const char *dev_name);

#endif