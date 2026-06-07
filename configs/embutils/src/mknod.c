#define _DEFAULT_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h> /* Required for makedev */
#include <unistd.h>        /* Required for mknod, umask */
#include <stdlib.h>
#include "write12.h"

static const char *Name = "mknod: ";

static void oops(const char *message) {
    __write2(Name);
    __write2(message);
    __write2("\n");
}

static void error(const char *message) {
    oops(message);
    exit(1);
}

/* Forward declaration */
int parsemode(const char *mode_str, mode_t *and_mask, mode_t *or_mask);

void panic(void) {
    switch (errno) {
        case EPERM:  oops("permission denied"); break;
        case EEXIST: oops("file exists"); break;
        default:     oops("system error"); break;
    }
}

void usage(void) {
    error("Usage: mknod [-m mode] filename type major minor\n"
          "       mkfifo [-m mode] filename\n"
          "type == {b|c|u|p}");
}

int main(int argc, char *argv[]) {
    int i;
    enum { MKNOD, MKFIFO } me;
    mode_t mode = 0666 & ~umask(0);
    mode_t and_mask, or_mask;
    char *name = NULL;
    char type = 0;
    unsigned short major = 0, minor = 0;
    dev_t dev = 0;

    char *Me = strrchr(argv[0], '/');
    Me = (Me) ? Me + 1 : argv[0];

    if (strcmp(Me, "mknod") == 0) {
        me = MKNOD;
    } else if (strcmp(Me, "mkfifo") == 0) {
        me = MKFIFO;
        Name = "mkfifo: ";
    } else {
        error("argv[0] must be mknod or mkfifo.");
    }

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'm' && i + 1 < argc) {
                parsemode(argv[++i], &and_mask, &or_mask);
                mode = (mode & and_mask) | or_mask;
            }
        } else {
            if (me == MKFIFO) {
                if (mknod(argv[i], mode | S_IFIFO, 0) != 0) panic();
            } else if (!name) name = argv[i];
            else if (!type) {
                if (strlen(argv[i]) != 1) error("type must be b, c, u, or p");
                type = argv[i][0];
            } else if (!major) major = (unsigned short)atoi(argv[i]);
            else if (!minor) minor = (unsigned short)atoi(argv[i]);
        }
    }

    if (me == MKNOD) {
        if (!name || !type) usage();
        dev = makedev(major, minor);
        switch (type) {
            case 'b': mode |= S_IFBLK;  break;
            case 'c': 
            case 'u': mode |= S_IFCHR;  break;
            case 'p': mode |= S_IFIFO;  break;
            default:  usage();
        }
        if (mknod(name, mode, dev) != 0) panic();
    }

    return 0;
}