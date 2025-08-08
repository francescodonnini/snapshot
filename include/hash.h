#ifndef AOS_HASH_H
#define AOS_HASH_H

char* hash_alloc(const char *alg_name, const char *key, int key_len);

int hash(const char *alg_name, const char *key, int key_len, char *out, int out_size);

#endif