#include "restore.h"
#include <argp.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define IOCTL_SNAPSHOT_MAGIC      0xca
#define IOCTL_ACTIVATE_SNAPSHOT   _IOWR(IOCTL_SNAPSHOT_MAGIC, IOCTL_ACTIVATE_SNAPSHOT_NO, struct ioctl_params)
#define IOCTL_DEACTIVATE_SNAPSHOT _IOWR(IOCTL_SNAPSHOT_MAGIC, IOCTL_DEACTIVATE_SNAPSHOT_NO, struct ioctl_params)

#define LS_SNAPSHOT 0xbeef
#define RESTORE_SNP 0xc0be

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
    char          *s1;
    char          *s2;
};

static error_t parse_opt(int opt, char *arg, struct argp_state *state) {
    struct argp_fields *fields = state->input;
    switch (opt) {
        case 'p':
            fields->s1 = arg;
            break;
        case 'w':
            fields->s2 = arg;
            break;
        case ARGP_KEY_ARG:
            if (state->arg_num == 0) {
                if (!strcmp(arg, "activate")) {
                    fields->command = IOCTL_ACTIVATE_SNAPSHOT;
                } else if (!strcmp(arg, "deactivate")) {
                    fields->command = IOCTL_DEACTIVATE_SNAPSHOT;
                } else if (!strcmp(arg, "ls")) {
                    fields->command = LS_SNAPSHOT;
                } else if (!strcmp(arg, "restore")) {
                    fields->command = RESTORE_SNP;
                } else {
                    argp_error(state, "expected one of activate, deactivate or ls but got %s", arg);
                }
            } else if (fields->command == RESTORE_SNP) {
                if (state->arg_num == 1) {
                    fields->s1 = arg;
                } else if (state->arg_num == 2) {
                    fields->s2 = arg;
                } else {
                    argp_usage(state);
                }
            } else {
                argp_usage(state);
            }
            break;
        case ARGP_KEY_END:
            if (!fields->command) {
                argp_error(state, "You must specify a command (activate|deactivate|ls|restore)");
            } else if (fields->command == IOCTL_ACTIVATE_SNAPSHOT
                       || fields->command == IOCTL_DEACTIVATE_SNAPSHOT) {
                if (!fields->s1 || !fields->s2) {
                    argp_error(state, "activate/deactivate require --path and --password");
                }
            } else if (fields->command == RESTORE_SNP) {
                if (!fields->s1 || !fields->s2) {
                    argp_error(state, "restore requires image_path and session_id");
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
    FILE* fp = fopen("/sys/class/bsnapshot_cls/bsnapshot/active", "r");
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
    switch (args.command) {
        case IOCTL_ACTIVATE_SNAPSHOT:
        case IOCTL_DEACTIVATE_SNAPSHOT:
            int fd = open("/dev/bsnapshot", O_RDWR);
            if (fd < 0) {
                printf("cannot open file, got error %d\n", errno);
                exit(errno);
            }
            char *path = malloc(PATH_MAX);
            if (!path) {
                exit(-ENOMEM);
            }
            if (!realpath(args.s1, path)) {
                perror(args.s1);
                exit(-errno);
            }
            printf("%s\n", path);
            struct ioctl_params params = {
                .path = path,
                .path_len = strlen(path),
                .password = args.s2,
                .password_len = strlen(args.s2),
            };
            err = ioctl(fd, args.command, &params);
            if (!err) {
                err = params.error;                
            }
            free(path);
            break;
        case LS_SNAPSHOT:
            ls();
            break;
        case RESTORE_SNP:
            err = restore_snapshot(args.s1, args.s2);
            break;
        default:
            break;
    }
    exit(err);
}