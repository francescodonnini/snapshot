#ifndef AUTH_H
#define AUTH_H
#include <linux/types.h>

bool auth_check_password(const char *password);

void auth_clear_password(void);

int auth_set_password(const char *password);

#endif