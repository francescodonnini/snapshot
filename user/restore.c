#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/dirent.h>s
#define BLOCK_SIZE     (512)

static char block[BLOCK_SIZE];
static char path[4096];

static void dump_block(const char *block, off_t start) {
    char *str = malloc(4096);
    if (!str) return;
    memset(str, 0, 4096);
    char *s = str;
    size_t w = 0;
    size_t i = 0;
    while (i < 512) {
        s += sprintf(s, "%06x\t", start + i * 16);
        for (int j = 0; j < 16 && i + j < 512; ++j) {
            if (j < 15) {
                s += sprintf(s, "%02x ", block[i + j]);
            } else {
                s += sprintf(s, "%02x\n", block[i + j]);
            }
        }
        i += 16;
    }
    printf("%s\n", str);
    free(str);
}

static void hd(FILE *off, unsigned long sector) {
    off_t offset = sector * BLOCK_SIZE;
    fseek(off, offset, SEEK_SET);
    fread(block, sizeof(char), BLOCK_SIZE, off);
    dump_block(block, offset);
}

static int dd(FILE *off, const char *parent, unsigned long sector) {
    sprintf(path, "%s/%lu", parent, sector);
    FILE* iff = fopen(path, "r");
    if (!iff) {
        printf("cannot open file %s", path);
        return ferror(iff);
    }
    size_t n = fread(block, sizeof(char), BLOCK_SIZE, iff);
    if (n < BLOCK_SIZE) {
        printf("fread failed: cannot read block %s", path);
        return ferror(iff) ? ferror(iff) : feof(iff);
    }
    off_t offset = sector * BLOCK_SIZE;
    fseek(off, offset, SEEK_SET);
    n = fwrite(block, sizeof(char), BLOCK_SIZE, off);
    if (n < BLOCK_SIZE) {
        printf("fwrite failed: cannot write block %lu to %s", sector, path);
        return ferror(off) ? ferror(off) : feof(off);
    }
    return 0;
}

int main(int argc, const char *argv[]) {
    if (--argc != 2) {
        exit(EXIT_FAILURE);
    }
    const char *img_path = argv[1];
    FILE* off = fopen(img_path, "r+");
    if (!off) {
        printf("cannot open file %s\n", img_path);
        exit(EXIT_FAILURE);
    }

    char *parent = malloc(4096);
    if (!parent) {
        goto out;
    }
    sprintf(parent, "/snapshots/%s", argv[2]);
    DIR *d = opendir(parent);
    if (d) {
        for (struct dirent *dir = readd)
    }
    struct dirent *dir;

    int err = dd(off, "/snapshots/3d0c63f5-920a-4e4a-a950-2bd95e6628fb", sector);
    if (err) {
        fclose(off);
        exit(err);
    }
    hd(off, sector);
out:
    fclose(off);
}