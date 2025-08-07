#ifndef FAST_HASH_H
#define FAST_HASH_H

// non-cryptographic hash function created by Glenn Fowler, Landon Curt Noll, and Kiem-Phong Vo
static inline unsigned long fast_hash(const char *name) {
    unsigned long h = 0xcbf29ce484222325;
    for (const char *c = name; *c; ++c) {
        h *= 0x100000001b3;
        h ^= *c;
    }
    return h;
}

#endif