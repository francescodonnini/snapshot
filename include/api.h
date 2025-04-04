#ifndef AOS_API_H
#define AOS_API_H
// It indicates that a device visible at the path specified by activate_snapshot is already mounted
// so it is impossible to maintain a snapshot of it (there may be update operations in progress) 
#define EALRDYMNTD 5003

int activate_snapshot(const char *dev_name, const char *password);

int deactivate_snapshot(const char *dev_name, const char *password);

#endif