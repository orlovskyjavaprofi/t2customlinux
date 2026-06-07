#define _DEFAULT_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "write12.h"

static int res = 0;

static void error(const char *message) {
    __write2("rmdir: ");
    __write2(message);
    __write2("\n");
}

void panic(void) {
    switch (errno) {
        case EPERM:      error("permission denied"); break;
        case EEXIST:     error("file exists"); break;
        case ENOTEMPTY:  error("directory is not empty"); break;
        case ENOTDIR:    error("not a directory"); break;
        case ENOENT:     error("no such file or directory"); break;
        case EACCES:     error("directory search permissions denied"); break;
        case EINVAL:     error("can't remove current directory"); break;
        default:         error("unknown error"); break;
    }
    res = 1;
}

void dormdir(const char *s, int v, int iexist) {
    if (rmdir(s) != 0) {
        // If iexist is set, we ignore EEXIST (or ENOENT for -p consistency)
        if (!iexist || (errno != EEXIST && errno != ENOENT)) {
            panic();
            return;
        }
    }
    if (errno == 0 && v) {
        __write2("rmdir: removing directory, ");
        __write2(s);
        __write2("\n");
    }
}

void minusp(char *s, int v) {
    char *t = strrchr(s, '/');
    while (1) {
        dormdir(s, v, 1);
        if (t == NULL) return;
        *t = '\0';
        // Look for the next parent
        t = strrchr(s, '/');
        // If we hit the root or no more parents, stop
        if (t == s) {
            dormdir(s, v, 1);
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    int i, p = 0, v = 0;
    
    if (argc < 2) {
        __write1("usage: rmdir [-vp] directory...\n");
        return 0;
    }
    
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (int j = 1; argv[i][j]; j++) {
                if (argv[i][j] == 'p') p = 1;
                else if (argv[i][j] == 'v') v = 1;
            }
        } else {
            if (p) minusp(argv[i], v);
            else dormdir(argv[i], v, 0);
        }
    }
    return res;
}