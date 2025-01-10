#ifndef __SNAPSHOT_API_H__
#define __SNAPSHOT_API_H__

int activate_snapshot(const char *dev_name, const char *password);

int deactivate_snapshot(const char *dev_name, const char *password);

#endif