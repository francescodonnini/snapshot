#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char *argv[]) {
    if (--argc != 2) exit(-1);
    FILE* fp = fopen(argv[1], "w");
    fprintf(fp, "%s\n", argv[2]);
}