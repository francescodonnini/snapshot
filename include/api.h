#ifndef AOS_API_H
#define AOS_API_H
#define EALRDYMNTD 5003

int activate_snapshot(const char *dev_name, const char *password);

int deactivate_snapshot(const char *dev_name, const char *password);

#endif