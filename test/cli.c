#include "include/ioctl_api.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define END_OF_FILE   -1
#define NO_OPT         0
#define ACTIVATE_OPT   1
#define DEACTIVATE_OPT 2
#define PATH_OPT       4
#define PASSWORD_OPT   8

#define usage(prog_name) fprintf(stderr, "see usage:\n%s (activate|deactivate) --path <path> --password <password>\n", argv[0]);

struct arg {
    int cmd;
    const char *path;
    const char *password;
};

static int readpos = 1;
static const char *readarg = NULL;

static int _parse_opt(const char *s) {
    if (!strcmp(s, "activate")) {
        return ACTIVATE_OPT;
    } else if (!strcmp(s, "deactivate")) {
        return DEACTIVATE_OPT;
    } else if (!strcmp(s, "--path") || !strcmp(s, "-p")) {
        return PATH_OPT;
    } else if (!strcmp(s, "--password") || !strcmp(s, "-w")) {
        return PASSWORD_OPT;
    }
    return NO_OPT;
}

static int try_readarg(int argc, const char *argv[]) {
    if (readpos < argc) {
        readarg = argv[readpos++];
        return 0;
    }
    return -1;
}

static int parse_opt(int argc, const char *argv[]) {
    if (readpos >= argc) {
        return -1;
    }
    readarg = NULL;
    switch (_parse_opt(argv[readpos])) {
        case ACTIVATE_OPT:
            ++readpos;
            return ACTIVATE_OPT;
        case DEACTIVATE_OPT:
            ++readpos;
            return DEACTIVATE_OPT;
        case PATH_OPT:
            ++readpos;
            if (try_readarg(argc, argv)) {
                return END_OF_FILE;
            }
            return PATH_OPT;
        case PASSWORD_OPT:
            ++readpos;
            if (try_readarg(argc, argv)) {
                return END_OF_FILE;
            }
            return PASSWORD_OPT;
        default:
            return NO_OPT;
    }
}

static int consume(int argc, const char *argv[], int what, int *opt) {
    int c = parse_opt(argc, argv);
    if (c & what) {
        if (opt) *opt = c;
        return 0;
    } else {
        return -1;
    }
}

static int parse_argv(int argc, const char *argv[], struct arg *args) {
    if (consume(argc, argv, ACTIVATE_OPT|DEACTIVATE_OPT, &args->cmd)) {
        usage(argv[0]);
        return -1;
    }
    if (consume(argc, argv, PATH_OPT, (int*)NULL)) {
        usage(argv[0]);
        return -1;
    }
    args->path = readarg;
    if (consume(argc, argv, PASSWORD_OPT, (int*)NULL)) {
        usage(argv[0]);
        return -1;
    }
    args->password = readarg;
    return 0;
}

int main(int argc, const char *argv[]) {
    struct arg args;
    if (parse_argv(argc, argv, &args)) {
        return -1;
    }
    int err = 0;
    if (args.cmd == ACTIVATE_OPT) {
        err = activate(args.path, args.password);
    } else if (args.cmd == DEACTIVATE_OPT) {
        err = deactivate(args.path, args.password);
    }
    if (err < 0) {
        if (err == -1) {
            printf("error %d: %s\n", errno, strerror(errno));
        } else {
            printf("error %d: %s\n", err, snapshot_strerror(err));
        }
    }
    return 0;
}