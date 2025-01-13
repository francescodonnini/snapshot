#include "include/ioctl_api.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MAX_LINE_SIZE 1024

static inline char *input(const char *prompt, char *line, int size) {
    printf("%s", prompt);
    return fgets(line, size, stdin);
}

static char *ltrim(char *s) {
    while (*s && isspace(*s))
        ++s;
    return s;
}

static char *rtrim(char *s, int n) {
    if (s == &s[n])
        return s;
    while (&s[n] > s && isspace(s[n]))
        --n;
    s[n] = 0;
    return s;
}

static inline char *trim(char *s, int n) {
    s = ltrim(s);
    return rtrim(s, n);
}

static int splitn(char *s, int n, char *splits[n]) {
    int i = 0; 
    for (char *p = strtok(s, " \t"); i < n && p != NULL; p = strtok(NULL, " \t")) {
        splits[i++] = p;
    }
    printf("%d\n", n-i);
    return n - i;
}

int main(int argc, const char *argv[]) {
    int fd = open("/dev/snapshot_test", O_RDWR);
    if (fd < 0) {
        printf("cannot open device, error %s\n", strerror(errno));
        return 1;
    }

    char line[MAX_LINE_SIZE], *splits[3];
    while (input(">>> ", line, MAX_LINE_SIZE)) {
        size_t n = strlen(line);
        line[--n] = 0;
        char *s = trim(line, n);
        if (!strcmp(s, "exit")) {
            break;
        }
        int cmd = -1;
        if (splitn(s, 3, splits) == 0) {
            if (!strcmp(splits[0], "activate")) {
                cmd = CHRDEV_IOCTL_ACTIVATE;
            } else if (!strcmp(splits[0], "deactivate")) {
                printf("deactivate selected\n");
                cmd = CHRDEV_IOCTL_DEACTIVATE;
            }
        }
        if (cmd != -1) {
            struct ioctl_params params = {
                .path = splits[1],
                .path_len = strlen(splits[1]),
                .password = splits[2],
                .password_len = strlen(splits[2]),
                .error = 0xdeadbeef
            };
            int err = ioctl(fd, cmd, &params);
            if (err) {
                printf("error (%d) %s\n", errno, strerror(errno));
            }
            printf("operation completed with return value %d\n", params.error);
        }
    }
    
    close(fd);
}