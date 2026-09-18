/*
 * stdio.h — a minimal stdio: formatted output over a shared format engine.
 *
 * The engine (stdio.c) drives either a bounded buffer (snprintf/vsnprintf) or an fd
 * (printf/dprintf, unbuffered — no FILE* streams yet). Supported directives:
 *   %s %c %d %i %u %x %X %o %p %%  with field width, precision, the '-' and '0' flags,
 *   and '*' width/precision and %.*s.
 * NOT yet supported: float (%f %e %g) and FILE streams/fopen — see libc/PLAN.md (next slice).
 */
#ifndef _LIBC_STDIO_H
#define _LIBC_STDIO_H

#include <stddef.h>
#include <stdarg.h>

typedef struct _gv_file FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#define EOF (-1)
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

FILE  *fopen(const char *path, const char *mode);
FILE  *fdopen(int fd, const char *mode);
int    fclose(FILE *f);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *f);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f);
int    fseek(FILE *f, long off, int whence);
long   ftell(FILE *f);
void   rewind(FILE *f);
int    fgetc(FILE *f);
int    getc(FILE *f);
int    ungetc(int c, FILE *f);
char  *fgets(char *s, int size, FILE *f);
int    fputc(int c, FILE *f);
int    putc(int c, FILE *f);
int    fputs(const char *s, FILE *f);
int    fflush(FILE *f);
int    feof(FILE *f);
int    ferror(FILE *f);
void   clearerr(FILE *f);
int    fprintf(FILE *f, const char *fmt, ...);
int    vfprintf(FILE *f, const char *fmt, va_list ap);
void   perror(const char *s);

int printf(const char *fmt, ...);
int dprintf(int fd, const char *fmt, ...);            /* fd-targeted printf */
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t size, const char *fmt, ...);

int vprintf(const char *fmt, va_list ap);
int vdprintf(int fd, const char *fmt, va_list ap);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

int puts(const char *s);                              /* writes s + '\n' to stdout */
int putchar(int c);

#endif /* _LIBC_STDIO_H */
