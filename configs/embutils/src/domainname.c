#define _GNU_SOURCE
#include <unistd.h>
#include <string.h>
#include <sys/utsname.h>
#include <errno.h>
#include <stdio.h>
#include "write12.h"

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (setdomainname(argv[1], strlen(argv[1]))) {
            switch (errno) {
                case EINVAL: __write2("name too long\n"); return 1;
                case EPERM:  __write2("must be superuser\n"); return 1;
                default:     __write2("unknown error\n"); return 1;
            }
        }
    } else {
#ifdef __linux__
        struct utsname un;
        size_t len;
        if (uname(&un) == 0) {
            // Use __domainname which is the standard field name in modern glibc
            len = strlen(un.__domainname);
            write(1, un.__domainname, len);
            write(1, "\n", 1);
        }
#else
        char buf[1024];
        if (getdomainname(buf, sizeof(buf)) == 0) {
            buf[sizeof(buf) - 1] = 0;
            size_t l = strlen(buf);
            buf[l] = '\n';
            write(1, buf, l + 1);
        }
#endif
    }
    return 0;
}