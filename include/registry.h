#ifndef AOS_REGISTRY_H
#define AOS_REGISTRY_H
#include "session.h"
#include <linux/types.h>

// EDUPNAME    indicates that someone has tried to register a device already registered
// ETOOBIG     indicates that the device name given to registry_insert exceeds the maximum number
//             of bytes allowed (4096 B)
// EWRONGCRED  indicates that a device with the specified name or password does not exist
//
#define EDUPNAME   5000
#define ETOOBIG    5001
#define EWRONGCRED 5002
#define ENOUUID    5003
#define ENOSSN     5004

int registry_init(void);

void registry_cleanup(void);

int registry_add_sector(dev_t dev, sector_t sector, bool *added);

int registry_lookup_sector(dev_t dev, sector_t sector, bool *present);

ssize_t registry_show_session(char *buf, size_t size);

bool registry_has_directory(dev_t dev, char *id, bool *has_dir);

int registry_insert(const char *dev_name, const char *password);

int registry_delete(const char *dev_name, const char *password);

bool registry_lookup_active(dev_t dev);

void registry_update_dir(dev_t dev, const char *session);

int registry_session_get(const char *dev_name, dev_t dev); 

int registry_session_put(dev_t dev);

void registry_destroy_session(dev_t dev);

#endif