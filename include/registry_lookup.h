#ifndef REGISTRY_LOOKUP_H
#define REGISTRY_LOOKUP_H
#include "registry.h"

extern bool registry_lookup_dev(dev_t dev);

extern bool registry_lookup_active(dev_t dev);

#endif