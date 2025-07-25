#ifndef REGISTRY_LOOKUP_H
#define REGISTRY_LOOKUP_H
#include "registry.h"

extern bool registry_lookup(const char *dev_name);

extern bool registry_lookup_mm(dev_t dev);

#endif