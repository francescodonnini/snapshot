#ifndef AOS_REGISTRY_H
#define AOS_REGISTRY_H
#include "b_range.h"
#include "session.h"
#include <linux/time64.h>
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

int registry_init(void);

void registry_cleanup(void);

int registry_insert(const char *dev_name);

int registry_delete(const char *dev_name);

int registry_session_prealloc(const char *dev_name, dev_t dev);

int registry_session_put(dev_t dev);

void registry_session_destroy(dev_t dev);

bool registry_session_id(dev_t dev, struct timespec64 *time, char *dirname, size_t n, struct timespec64 *created_on);

int registry_add_range(dev_t dev, struct timespec64 *created_on, struct b_range *range);

int registry_lookup_range(dev_t dev, unsigned long start, unsigned long end_excl);

ssize_t registry_show_session(char *buf, size_t size);

#endif