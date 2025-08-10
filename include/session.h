#ifndef AOS_SESSION_H
#define AOS_SESSION_H
#include "hashset.h"

struct session {
    dev_t           dev;
    char           *id;
    struct hashset  hashset;
};

struct session *session_create(dev_t dev);

void session_destroy(struct session *s);

#endif