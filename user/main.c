#include <argp.h>
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

#define LS_SNAPSHOT 0xbeef

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

static struct argp_option options[] = {
    {"path",     'p', "PATH",     0, "Path to device (required for activate/deactivate)" },
    {"password", 'w', "PASSWORD", 0, "Password (required for activate/deactivate)" },
    { 0 }
};

struct argp_fields {
    unsigned long  command;
    char          *path;
    char          *password;
};

static error_t parse_opt(int opt, char *arg, struct argp_state *state) {
    struct argp_fields *fields = state->input;
    switch (opt) {
        case 'p':
            fields->path = arg;
            break;
        case 'w':
            fields->password = arg;
            break;
        case ARGP_KEY_ARG:
            if (state->arg_num == 0) {
                if (!strcmp(arg, "activate")) {
                    fields->command = IOCTL_ACTIVATE_SNAPSHOT;
                } else if (!strcmp(arg, "deactivate")) {
                    fields->command = IOCTL_DEACTIVATE_SNAPSHOT;
                } else if (!strcmp(arg, "ls")) {
                    fields->command = LS_SNAPSHOT;
                } else {
                    argp_error(state, "expected one of activate, deactivate or ls but got %s", arg);
                }
            } else {
                argp_usage(state);
            }
            break;
        case ARGP_KEY_END:
            if (!fields->command) {
                argp_error(state, "You must specify a command (activate|deactivate|ls)");
            } else if (fields->command == IOCTL_ACTIVATE_SNAPSHOT
                       || fields->command == IOCTL_DEACTIVATE_SNAPSHOT) {
                if (!fields->path || !fields->password) {
                    argp_error(state, "activate/deactivate require --path and --password");
                }
            }
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, "Blkdev Snapshot", "Command line interface to interact with blkdev snapshot" };

static void ls(void) {
    char line[1024];
    FILE* fp = fopen("/sys/kernel/sessions/active", "r");
    if (!fp) {
        printf("module is not mounted\n");
        return;
    }
    while (fgets(line, 1024, fp)) {
        printf("%s", line);
    }
}

int main(int argc, char *argv[]) {
    struct argp_fields args;
    memset(&args, 0, sizeof(args));
    error_t err = argp_parse(&argp, argc, argv, 0, 0, &args);
    if (err) {
        exit(err);
    }
    if (args.command == LS_SNAPSHOT) {
        ls();
        exit(EXIT_SUCCESS);
    } else {
        int fd = open("/dev/bsnapshot", O_RDWR);
        if (fd < 0) {
            printf("cannot open file, got error %d\n", errno);
            exit(errno);
        }
        struct ioctl_params params = {
            .path = args.path,
            .path_len = strlen(args.path),
            .password = args.password,
            .password_len = strlen(args.password),
        };
        int err = ioctl(fd, args.command, &params);
        exit(err);
        printf("%s %s %s\n", args.command == IOCTL_ACTIVATE_SNAPSHOT ? "activate" : "deactivate", args.path, args.password);
    }
}