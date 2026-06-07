#ifndef WRITE12_H
#define WRITE12_H

#include <unistd.h>
#include <string.h>

#ifdef __dietlibc__
#include <write12.h>
#else
/* * GCC 16 requires us to acknowledge the return value of write().
 * We cast to (void) to suppress -Wunused-result warnings.
 */
static inline int __write1(const char* s) {
    ssize_t ret = write(1, s, strlen(s));
    (void)ret; 
    return 0;
}

static inline int __write2(const char* s) {
    ssize_t ret = write(2, s, strlen(s));
    (void)ret;
    return 0;
}
#endif

#endif /* WRITE12_H */