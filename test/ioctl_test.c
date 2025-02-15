#include "include/ioctl_api.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

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
    return n - i;
}

int main(int argc, const char *argv[]) {
    char line[MAX_LINE_SIZE], *splits[3];
    while (input(">>> ", line, MAX_LINE_SIZE)) {
        size_t n = strlen(line);
        line[--n] = 0;
        char *s = trim(line, n);
        if (!strcmp(s, "exit")) {
            break;
        }
        int err = -1;
        if (splitn(s, 3, splits) == 0) {
            if (!strcmp(splits[0], "activate")) {
                err = activate(splits[1], splits[2]);
            } else if (!strcmp(splits[0], "deactivate")) {
                err = deactivate(splits[1], splits[2]);
            }
        } else {
            printf("invalid command\n");
            continue;
        }
        if (err < 0) {
            if (err == -1) {
                printf("error %d: %s\n", errno, strerror(errno));
            } else {
                printf("error %d: %s\n", err, snapshot_strerror(err));
            }
        }
    }
}