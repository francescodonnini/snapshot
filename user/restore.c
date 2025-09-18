#include "restore.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fts.h>

static int dd(FILE *off, FTSENT *p) {
    unsigned long sector = strtoul(p->fts_name, NULL, 10);
    if (!sector && errno) {
        fprintf(stderr, "cannot convert file name %s to number, got error %d", p->fts_name, errno);
        return -errno;
    }
    FILE* iff = fopen(p->fts_path, "r");
    if (!iff) {
        int err = ferror(iff);
        fprintf(stderr, "cannot open file %s got error", p->fts_path, err);
        return err;
    }

    int err = fseek(iff, 0, SEEK_END);
    if (err) {
        perror("fseek failed");
        goto out;
    }
    long size = ftell(iff);
    err = fseek(iff, 0, SEEK_SET);
    if (err) {
        perror("fseek failed");
        goto out;
    }
    char *block = malloc(size);
    if (!block) {
        perror("out of memory");
        err = -ENOMEM;
        goto out2;
    }
    size_t n = fread(block, 1, size, iff);
    if (n < size) {
        if (feof(iff)) {
            perror("EOF met too early");
            err = EOF;
        } else {
            err = ferror(iff);
            fprintf(stderr, "fread failed, got error %d", err);
        }
        goto out2;
    }
    off_t offset = sector * 512;
    fseek(off, offset, SEEK_SET);
    n = fwrite(block, sizeof(char), size, off);
    if (n < size) {
        if (feof(iff)) {
            perror("EOF met too early");
            err = EOF;
        } else {
            err = ferror(iff);
            fprintf(stderr, "fwrite failed, got error %d", err);
        }
    }
out2:
    free(block);
out:
    fclose(iff);
    return 0;
}

int restore_snapshot(const char *dev, char * const *snapshot) {
    FILE* off = fopen(dev, "rb+");
    if (!off) {
        int err = ferror(off);
        fprintf(stderr, "cannot open %s got error %d", dev, err);
        return err;
    }    
    int err;
    const int fts_opts = FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR;
    FTS *ftsp = fts_open(snapshot, fts_opts, NULL);
    if (!ftsp) {
        perror("fts_open failed");
        err = -1;
        goto out;
    }
    
    FTSENT *chp = fts_children(ftsp, 0);
    if (!chp) {
        printf("directory %s is empty", snapshot);
        err = 0;
        goto out2;
    }

    for (FTSENT *p = fts_read(ftsp); p; p = fts_read(ftsp)) {
        switch (p->fts_info) {
            case FTS_F:
                err = dd(off, p);
                if (err) {
                    break;
                }
                break;
            default:
                break;
        }
    }
out2:
    fts_close(ftsp);
out:
    fclose(off);
    return err;
}