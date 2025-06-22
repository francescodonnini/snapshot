#ifndef AOS_HASH_H
#define AOS_HASH_H

char* hash(const char *alg_name, const char *key, int key_len);

int hash2(const char *alg_name, const char *key, int key_len, char *out);

#endif