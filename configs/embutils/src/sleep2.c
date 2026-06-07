#define _GNU_SOURCE
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>  /* Required for strlen, strcmp */
#include <stdlib.h>  /* Required for exit() */

static const char usage_msg[] = 
    "Usage:\tsleep NUMBER[SUFFIX]\n"
    "\tsleep NUMBER.NUMBER\n"
    "\tsleep HOUR:MIN:SEC\n"
    "Pause for NUMBER seconds or until HOUR:MIN:SEC\n";

static const char version_msg[] = "sleep 3.0 (Kutils)\n";
static const char too_few_args[] = "sleep: too few arguments. Try 'sleep --help'.\n";
static const char try_help[] = "'. Try 'sleep --help' for more information.\n";

static void message(const char* p) {
    write(2, p, strlen(p));
}

static unsigned long readno(char const** pp) {
    const char* p = *pp;
    unsigned long ret = 0;
    while ((unsigned int)(*p - '0') < 10u)
        ret = 10 * ret + (unsigned int)(*p++ - '0');
    *pp = p;
    return ret;
}

static unsigned long readno_ns(char const** pp) {
    const char* p = *pp;
    unsigned long mul = 1000000000;
    unsigned long ret = 0;
    while ((unsigned int)(*p - '0') < 10u) {
        mul /= 10;
        ret += mul * (unsigned int)(*p++ - '0');
    }
    *pp = p;
    return ret;
}

time_t timezonediff(time_t value) {
    struct tm t;
    time_t diff;
    localtime_r(&value, &t); 
    diff = ((t.tm_wday * 24 + t.tm_hour) * 60 + t.tm_min) * 60 + t.tm_sec;
    gmtime_r(&value, &t); 
    diff -= ((t.tm_wday * 24 + t.tm_hour) * 60 + t.tm_min) * 60 + t.tm_sec;
    
    if (diff < -43200) diff += 7 * 86400;
    if (diff > 43200)  diff -= 7 * 86400;
    return diff;
}

int main(int argc, char** argv) {
    int hour = 0, min = 0, clockmode = 0;
    time_t sec = 0;
    long nsec = 0;
    const char* p;
    struct timespec tv;
    struct timeval t;
    
    if (argc <= 1) {
        message(too_few_args);
        return 1;
    }
    
    p = argv[1];
    if (*p >= '0' && *p <= '9') {
        sec = readno(&p);
        switch (*p) {
            case 'd': case 'D': sec *= 24; /* Fall through */
            case 'h': case 'H': sec *= 60; /* Fall through */
            case 'm': case 'M': sec *= 60; /* Fall through */
            case '\0': case 's': case 'S': break;
            case '.': p++; nsec = readno_ns(&p); break;
            case ':':
                clockmode = 1; hour = (int)sec; sec = 0; p++;
                min = (int)readno(&p);
                if (*p == ':') { p++; sec = readno(&p); }
                break;
            default: goto error;
        }
    } else if (*p == ':') {
        clockmode = 1; hour = -1; p++;
        min = (int)readno(&p);
        if (*p == ':') { p++; sec = readno(&p); }
    } else if (strcmp(p, "--help") == 0) {
        message(usage_msg); return 0;
    } else if (strcmp(p, "--version") == 0) {
        message(version_msg); return 0;
    } else {
    error:
        message("sleep: illegal argument '");
        message(argv[1]);
        message(try_help);
        return 1;
    }

    if (clockmode) {
        gettimeofday(&t, NULL);
        t.tv_sec += timezonediff(t.tv_sec);
        t.tv_sec %= 86400;
        sec = sec + 60L * min + 3600L * hour - 1;
        if (hour == -1) { while (sec < t.tv_sec) sec += 3600; }
        if (sec < t.tv_sec) sec += 86400;
        tv.tv_sec = sec - t.tv_sec;
        tv.tv_nsec = 999999999L - (t.tv_usec * 1000L); 
    } else {
        tv.tv_sec = sec;
        tv.tv_nsec = nsec; 
    }
    
    while (nanosleep(&tv, &tv));
    return 0;
}