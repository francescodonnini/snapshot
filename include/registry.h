#ifndef AOS_SNAPSHOT_REGISTRY_H
#define AOS_SNAPSHOT_REGISTRY_H

#define EBDEVNAME 5000

int registry_init(void);

void registry_cleanup(void);

int registry_insert(const char *dev_name, const char *password);

void registry_delete(const char *dev_name, const char *password);

int registry_check_password(const char *dev_name, const char *password);

#endif