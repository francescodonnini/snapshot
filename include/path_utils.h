#ifndef AOS_PATH_UTILS_H
#define AOS_PATH_UTILS_H

/**
 * path_join -- utility to join two path components. It fails if:
 * 1. the sum of the bytes between parent and child is greater or equal to the maximum number of bytes
 *    a path can possibly have.
 * 2. parent or child is an empty string
 * @param parent is the first component of the path
 * @param child  is the second componente of the path
 * @returns buffer on success, NULL on error.
 */
char *path_join(const char *parent, const char *child, char *buffer);

char *path_join_alloc(const char *parent, const char *child);
#endif