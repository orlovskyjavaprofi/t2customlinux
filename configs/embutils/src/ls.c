#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include "embfeatures.h"
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <dirent.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <sys/types.h>

/* Forward Prototypes */
struct de;
int byname(const void *a, const void *b);
int bysize(const void *a, const void *b);
int byctime(const void *a, const void *b);
int bymtime(const void *a, const void *b);
int byatime(const void *a, const void *b);
static void lsfiles(void);
static void printname(struct de* d);
static void ls_dir(const char* name);
static void oops(const char *fn);
static void nomem(void);

/* Data Structures */
struct statlist { struct stat s; struct statlist* next; };
enum filetype { REGULAR, SYMLINK, BROKENLINK, SUBDIR, SOCKET, FIFO, CHAR, BLOCK };
struct de {
    const char *name, *link;
    struct stat s, fs;
    enum filetype type;
    char qual;
    struct de *next;
} *root_files = NULL, *root_dirs = NULL, *last_files = NULL, *last_dirs = NULL;

unsigned int num_files = 0, num_dirs = 0;
static int _A, _F, _L, _R, _a, _b, _d, _i, _n, _o, _p, _r, _s;
static time_t now;
static unsigned int ttywidth = 80;
static int res = 0;
static int displaytotal = 0;
#ifdef SUPPORT_COLORS
static char* colors = NULL;
#endif

enum { SINGLECOL, VERTICAL, HORIZONTAL, LONG, COMMAS } display = VERTICAL;

/* Implementation of Comparison Functions */
int byname(const void *a, const void *b) {
    int res = strcmp((*(struct de **)a)->name, (*(struct de **)b)->name);
    return _r ? -res : res;
}

int bysize(const void *a, const void *b) {
    struct de *da = *(struct de **)a;
    struct de *db = *(struct de **)b;
    if (da->s.st_size > db->s.st_size) return _r ? 1 : -1;
    if (da->s.st_size < db->s.st_size) return _r ? -1 : 1;
    return byname(a, b);
}

int byctime(const void *a, const void *b) {
    int res = (int)((*(struct de **)a)->s.st_ctime - (*(struct de **)b)->s.st_ctime);
    return _r ? -res : res;
}

int bymtime(const void *a, const void *b) {
    int res = (int)((*(struct de **)a)->s.st_mtime - (*(struct de **)b)->s.st_mtime);
    return _r ? -res : res;
}

int byatime(const void *a, const void *b) {
    int res = (int)((*(struct de **)a)->s.st_atime - (*(struct de **)b)->s.st_atime);
    return _r ? -res : res;
}

static int (*compare)(const void *, const void *) = byname;

/* Error Handling */
static void nomem(void) { write(2, "ls: out of memory\n", 18); exit(1); }
static void oops(const char *fn) {
    write(2, "ls: ", 4);
    if (fn) { write(2, fn, strlen(fn)); write(2, ": ", 2); }
    write(2, "error\n", 6);
    res = 1;
}

/* Include Support Files */
#include "fmt_ulong.c"
#define DONT_NEED_PUT
#define DONT_NEED_PUTULONG
#define WANT_BUFFER_2
#include "buffer.c"

/* Note: Ensure dols, lsfiles, ls_dir, and printname definitions match your local logic */
#include "ls_impl.c" 

int main(int argc, char *argv[]) {
    time(&now);
    int i, args = 0;
#ifdef SUPPORT_COLORS
    colors = getenv("LS_COLORS");
#endif

    for (i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') {
            ++args;
        } else {
            for (int j = 1; argv[i][j] != 0; ++j) {
                switch (argv[i][j]) {
                    case '1': display = SINGLECOL; break;
                    case 'l': display = LONG; break;
                    case 'C': display = VERTICAL; break;
                    case 'x': display = HORIZONTAL; break;
                    case 'S': compare = bysize; break;
                    case 'c': compare = byctime; break;
                    case 't': compare = bymtime; break;
                    case 'u': compare = byatime; break;
                    case 'f': compare = NULL; break;
                    case 'A': _A = 1; break;
                    case 'F': _F = 1; break;
                    case 'L': _L = 1; break;
                    case 'R': _R = 1; break;
                    case 'a': _a = 1; break;
                    case 'd': _d = 1; break;
                    case 'i': _i = 1; break;
                    case 'n': _n = 1; break;
                    case 'r': _r = 1; break;
                    case 's': _s = 1; break;
                    default: exit(1);
                }
            }
        }
    }

    /* Final logic for directory iteration */
    if (args < 1) dols(".", ".", 0);
    else {
        for (i = 1; i < argc; ++i) {
            if (argv[i][0] != '-') dols(argv[i], argv[i], 0);
        }
    }
    
    buffer_flush(buffer_1);
    return res;
}