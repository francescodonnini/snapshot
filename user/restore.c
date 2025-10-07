#include "restore.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fts.h>

struct snap_header {
    unsigned long sector;
    unsigned long nbytes;
};

static char *alloc_buffer(char *buf, size_t size, size_t *old_size) {
    if (!buf) {
        buf = malloc(size);
    } else if (*old_size < size) {
        buf = realloc(buf, size);
        if (buf) {
            *old_size = size;
        }
    }
    return buf;
}

int restore_snapshot(const char *dev, const char *snapshot) {
    FILE* off = fopen(dev, "rb+");
    if (!off) {
        perror(dev);
        return -errno;
    }

    FILE* iff = fopen(snapshot, "rb");
    if (!iff) {
        perror(snapshot);
        fclose(off);
        return -errno;
    }

    int err = 0;
    char *buffer = NULL;
    size_t last_buffer_size = 0;
    for (;;) {
        struct snap_header header;
        if (fread(&header, sizeof(header), 1, iff) < 1) {
            if (ferror(iff)) {
                err = -errno;
                perror(snapshot);
            }
            break;
        }

        buffer = alloc_buffer(buffer, header.nbytes, &last_buffer_size);
        if (!buffer) {
            err = -ENOMEM;
            break;
        }

        size_t n = fread(buffer, 1, header.nbytes, iff);
        if (n < header.nbytes) {
            if (ferror(iff)) {
                err = -errno;
                perror(snapshot);
            } else {
                err = EOF;
                fprintf(stderr, "EOF met too early");
            }
            break;
        }
        off_t offset = header.sector * 512;
        if (fseek(off, offset, SEEK_SET)) {
            err = -errno;
            perror("seek failed");
            break;
        }
        n = fwrite(buffer, 1, header.nbytes, off);
        if (n < header.nbytes) {
            err = -errno;
            perror(dev);
            break;
        }
    }
    if (buffer) {
        free(buffer);
    }
    fclose(iff);
    fclose(off);
    return err;
}