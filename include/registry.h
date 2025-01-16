#ifndef AOS_SNAPSHOT_REGISTRY_H
#define AOS_SNAPSHOT_REGISTRY_H

#define EDUPNAME   5000
#define ETOOBIG    5001
#define EWRONGCRED 5002

int registry_init(void);

void registry_cleanup(void);

int registry_insert(const char *dev_name, const char *password);

int registry_delete(const char *dev_name, const char *password);

int registry_check_password(const char *dev_name, const char *password);

#endif