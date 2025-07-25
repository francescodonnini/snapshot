#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char *argv[]) {
    if (--argc != 2) exit(-1);
    FILE* fp = fopen(argv[1], "w");
    if (!fp) exit(-1);
    fprintf(fp, "%s\n", argv[2]);
    fclose(fp);
    return 0;
}