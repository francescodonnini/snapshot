#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE_SIZE 1024

static inline char *input(const char *prompt, char *line, int size) {
    printf("%s", prompt);
    return fgets(line, size, stdin);
}

int main(int argc, const char *argv[]) {
    int fd = open("/dev/snapshot_test", O_RDWR);
    if (fd < 0) {
        printf("cannot open device, error %s\n", strerror(errno));
        return 1;
    }
    char line[MAX_LINE_SIZE], buffer[MAX_LINE_SIZE];
    while (input(">>> ", line, MAX_LINE_SIZE)) {
        size_t n = strlen(line);
        line[--n] = 0;
        if (!strcmp(line, "exit")) {
            break;
        }
        n = write(fd, line, n);
        n = read(fd, buffer, MAX_LINE_SIZE);
        printf("reads %d bytes from file\n", n);
        buffer[n] = 0;
        printf("%s\n", buffer);
    }
    close(fd);
}