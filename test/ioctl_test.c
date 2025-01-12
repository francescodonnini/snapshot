#include "include/ioctl_api.h"
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MAX_LINE_SIZE 1024

int main(int argc, const char *argv[]) {
    int fd = open("/dev/snapshot_test", O_RDWR);
    if (fd < 0) {
        printf("cannot open device, error %s\n", strerror(errno));
        return 1;
    }
    
    char *dev_name = "/dev/sda42";
    char *password = "password";
    struct ioctl_params params = {
        .path = dev_name,
        .path_len = strlen(dev_name),
        .password = password,
        .password_len = strlen(password),
        .error = 0xdeadbeef
    };
    int err = ioctl(fd, CHRDEV_IOCTL_ACTIVATE, &params);
    if (err) {
        printf("error (%d) %s\n", errno, strerror(errno));
    }
    printf("operation completed with return value %d\n", params.error);

    close(fd);
}