#define _DEFAULT_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include "write12.h"

/* Forward declaration to fix implicit declaration warnings */
int parsemode(const char *mode_str, mode_t *and_mask, mode_t *or_mask);

static void oops(const char *message) {
    __write2("mkdir: ");
    __write2(message);
    __write2("\n");
}

static int res = 0;

void panic(void) {
    switch (errno) {
        case EPERM:  oops("permission denied"); break;
        case EEXIST: oops("file exists"); break;
        case ENOENT: oops("file not found"); break;
        case ENOSPC: oops("no space left on device"); break;
        case EIO:    oops("I/O error"); break;
        default:     oops("unknown error"); break;
    }
    res = 1;
}

void domkdir(char *s, mode_t m, int v, int iexist, const char *full) {
    errno = 0;
    if (*s != 0 && mkdir(s, m) != 0) {
        if (!iexist || errno != EEXIST) {
            panic();
            return;
        }
    }
    if (errno == 0 && v) {
        __write2("mkdir: created directory `");
        __write2(full);
        __write2("'\n");
    }
}

void minusp(char *s, mode_t m, int v) {
    int fd = open(".", O_RDONLY);
    char *t;
    char *full = s;
    if (fd < 0) { panic(); return; }
    
    if (*s == '/') {
        chdir("/");
        ++s;
    }
    while ((t = strchr(s, '/'))) {
        *t = 0;
        if (*s != 0) {
            domkdir(s, m, v, 1, full);
            chdir(s);
        }
        s = t + 1;
        *t = '/';
    }
    if (*s) domkdir(s, m, v, 1, full);
    
    fchdir(fd);
    close(fd);
}

void usage(void) {
    __write1("Usage: mkdir [-pv] [-m mode] directory...\n"
             "  -p  no error if existing, make parent directories as needed\n"
             "  -m  set permission mode\n"
             "  -v  print a message for each created directory\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    int i;
    int p = 0, v = 0;
    mode_t mode = 0777 & ~umask(0);
    mode_t and_mask, or_mask;

    if (argc < 2) usage();

    for (i = 1; i < argc; i++) {
        if (!argv[i]) continue;
        if (argv[i][0] == '-') {
            int j, len = strlen(argv[i]);
            for (j = 1; j < len; j++) {
                switch (argv[i][j]) {
                    case '-':
                        if (strcmp(argv[i], "--parent") == 0) p = 1;
                        else if (strcmp(argv[i], "--verbose") == 0) v = 1;
                        else usage();
                        j = len; break;
                    case 'p': p = 1; break;
                    case 'v': v = 1; break;
                    case 'm': 
                        if (i + 1 < argc) {
                            parsemode(argv[++i], &and_mask, &or_mask); 
                            mode = (mode & and_mask) | or_mask;
                        }
                        break;
                    default: usage();
                }
            }
        } else {
            if (p) minusp(argv[i], mode, v);
            else domkdir(argv[i], mode, v, 0, argv[i]);
        }
    }
    return res;
}