#ifndef AOS_REGISTRY_H
#define AOS_REGISTRY_H
#include <stdbool.h>

// EDUPNAME    indicates that someone has tried to register a device already registered
// ETOOBIG     indicates that the device name given to registry_insert exceeds the maximum number
//             of bytes allowed (4096 B)
// EWRONGCRED  indicates that a device with the specified name or password does not exist
//
#define EDUPNAME   5000
#define ETOOBIG    5001
#define EWRONGCRED 5002

int registry_init(void);

void registry_cleanup(void);

int registry_insert(const char *dev_name, const char *password);

int registry_delete(const char *dev_name, const char *password);

bool registry_check_password(const char *dev_name, const char *password);

#endif