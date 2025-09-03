#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#define IOCTL_SNAPSHOT_MAGIC      0xca
#define IOCTL_ACTIVATE_SNAPSHOT   _IOWR(IOCTL_SNAPSHOT_MAGIC, IOCTL_ACTIVATE_SNAPSHOT_NO, struct ioctl_params)
#define IOCTL_DEACTIVATE_SNAPSHOT _IOWR(IOCTL_SNAPSHOT_MAGIC, IOCTL_DEACTIVATE_SNAPSHOT_NO, struct ioctl_params)

enum {
    IOCTL_ACTIVATE_SNAPSHOT_NO = 0x70,
    IOCTL_DEACTIVATE_SNAPSHOT_NO,
    IOCTL_SNAPSHOT_MAX_NR
};

struct ioctl_params {
    char   *path;
    size_t  path_len;
    char   *password;
    size_t  password_len;
    int     error;
};

int main(int argc, const char *argv[]) {
    if (argc != 4) {
        printf("expected 4 arguments, got %d\n", argc - 1);
        exit(EXIT_FAILURE);
    }
    int fd = open("/dev/bsnapshot", O_RDWR);
    if (fd < 0) {
        printf("cannot open file, got error %d\n", errno);
        exit(errno);
    }
    unsigned long command;
    if (!strcmp(argv[1], "activate")) {
        command = IOCTL_ACTIVATE_SNAPSHOT;
    } else if (!strcmp(argv[1], "deactivate")) {
        command = CHRDEV_IOCTL_DEACTIVATE;
    } else {
        printf("expected either 'activate' or 'deactivate', got %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }
    struct ioctl_params params = {
        .path = argv[2],
        .path_len = strlen(argv[2]),
        .password = argv[3],
        .password_len = strlen(argv[3]),
        .error = 0,
    };
    int err = ioctl(fd, command, &params);
    if (err) {
        exit(err);
    }
}