#define _GNU_SOURCE
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "write12.h"

#define EMBEDDED
#include "__puts.c"

/* * Ensure environ is declared. 
 * On POSIX systems, it is in <unistd.h>. 
 * If not, we declare it manually.
 */
#ifndef environ
extern char **environ;
#endif

int main(int argc, char *argv[]) {
    int i;
    for (i = 1; i < argc; ++i) {
        char *v = argv[i];
        if (v[0] == '-') {
            if (v[1] == 0) {
                /* Handle plain '-' as clear environment */
                *environ = NULL;
            } else {
                int j;
                for (j = 1; v[j]; ++j) {
                    switch (v[j]) {
                        case 'i':
                            *environ = NULL;
                            break;
                        case 'u':
                            /* Handle -u option requiring argument */
                            if (v[j + 1]) {
                                unsetenv(v + j + 1);
                                goto next_arg; /* Move to next argv element */
                            } else if (i + 1 < argc) {
                                unsetenv(argv[++i]);
                                goto next_arg;
                            } else {
                                __write2("env: option requires argument: -u\n");
                                return 1;
                            }
                        default:
                            __write2("env: invalid option\n");
                            return 1;
                    }
                }
            }
        } else if (strchr(v, '=')) {
            putenv(v);
        } else {
            /* Execute the command */
            execvp(v, &argv[i]);
            /* If execvp returns, it failed */
            __write2("env: could not execute command\n");
            return 127;
        }
        next_arg:;
    }

    /* Print current environment */
    for (char **env = environ; *env != NULL; ++env) {
        __puts(*env);
    }
    return 0;
}