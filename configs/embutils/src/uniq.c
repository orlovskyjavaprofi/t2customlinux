#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

/* Forward Prototypes */
void write_str(const char *s);
void print_and_free(char *buf, int count);
void error_msg(const char *msg);
int read_line(char **buffer);
int fmt_ulongpadright(char *b, unsigned long n, int pad);

int opt_count = 0, opt_notrep = 0, opt_chars = 0, opt_suprep = 0;
int input_fd = 0, output_fd = 1;

void write_str(const char *s) {
    write(output_fd, s, strlen(s));
}

void print_and_free(char *buf, int count) {
    char b[15];
    int condition = (opt_notrep && count > 1) || 
                    (opt_suprep && count == 1) || 
                    (!opt_notrep && !opt_suprep);
    
    if (condition) {
        if (opt_count) {
            int len = fmt_ulongpadright(b, (unsigned long)count, 7);
            b[len] = '\0';
            write_str(b);
            write_str(" ");
        }
        write_str(buf);
        write_str("\n");
    }
    free(buf);
}

void error_msg(const char *msg) {
    write(2, msg, strlen(msg));
    exit(EXIT_FAILURE);
}

int read_line(char **buffer) {
    int len, b_len = 1;
    char c;
    char *b = malloc(b_len);
    if (!b) error_msg("error allocating memory.\n");
    b[0] = '\0';
    
    while ((len = read(input_fd, &c, 1)) > 0 && c != '\n') {
        char *tmp = realloc(b, b_len + 1);
        if (!tmp) { free(b); error_msg("error reallocating memory.\n"); }
        b = tmp;
        b[b_len - 1] = c;
        b[b_len++] = '\0';
    }
    *buffer = b;
    return (len <= 0 && b_len == 1) ? -1 : b_len;
}

int main(int argc, char *argv[]) {
    int i, cur_line_count = 0, line_len;
    char *current_line = NULL, *line_in_buffer = NULL;

    for (i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'c': opt_count = 1; break;
                case 'd': opt_notrep = 1; break;
                case 'u': opt_suprep = 1; break;
                case 's':
                    if (argv[++i]) opt_chars = (int)strtol(argv[i], NULL, 10);
                    else error_msg("Missing argument for -s.\n");
                    break;
                default: error_msg("usage: uniq [-c|-d|-u] [-s chars] [in [out]]\n");
            }
        } else break;
    }

    if (i < argc && strcmp(argv[i], "-") != 0) {
        if ((input_fd = open(argv[i], O_RDONLY)) < 0) error_msg("Error opening input.\n");
        if (i + 1 < argc) {
            if ((output_fd = open(argv[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
                error_msg("Error opening output.\n");
        }
    }

    while ((line_len = read_line(&line_in_buffer)) > -1) {
        if (current_line == NULL) {
            current_line = line_in_buffer;
            cur_line_count = 1;
        } else {
            /* Safely handle opt_chars offset */
            if (strcmp(current_line + (strlen(current_line) > (size_t)opt_chars ? opt_chars : 0),
                       line_in_buffer + (strlen(line_in_buffer) > (size_t)opt_chars ? opt_chars : 0)) == 0) {
                cur_line_count++;
                free(line_in_buffer);
            } else {
                print_and_free(current_line, cur_line_count);
                current_line = line_in_buffer;
                cur_line_count = 1;
            }
        }
    }
    if (current_line) print_and_free(current_line, cur_line_count);
    return EXIT_SUCCESS;
}