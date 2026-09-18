/*
 * stdio.c — the buffered FILE* stream layer over the raw fd syscalls.
 *
 * Output is UNBUFFERED (each fputc/fputs/fwrite/fprintf write()s immediately, so fflush is
 * a no-op); input is buffered (fgetc/fgets/fread refill via read()). stdin/stdout/stderr are
 * static streams on fds 0/1/2. The printf/snprintf format engine lives in printf.c; fprintf
 * here just targets a stream's fd through vdprintf. Enough for init (config read + logging)
 * and coreutils; a write buffer + real fmemopen can come later.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

struct _gv_file {
	int fd;
	int flags;                 /* bit0 = EOF seen, bit1 = error */
	int ungot;                 /* pushed-back byte, or -1 */
	int rpos, rlen;            /* read-buffer cursor / fill */
	unsigned char rbuf[1024];
};

static FILE _stdin  = { 0, 0, -1, 0, 0, {0} };
static FILE _stdout = { 1, 0, -1, 0, 0, {0} };
static FILE _stderr = { 2, 0, -1, 0, 0, {0} };
FILE *stdin  = &_stdin;
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;

FILE *fopen(const char *path, const char *mode)
{
	int fl;
	switch (mode[0]) {
	case 'r': fl = (mode[1] == '+') ? O_RDWR : O_RDONLY; break;
	case 'w': fl = O_CREAT | O_TRUNC  | ((mode[1] == '+') ? O_RDWR : O_WRONLY); break;
	case 'a': fl = O_CREAT | O_APPEND | ((mode[1] == '+') ? O_RDWR : O_WRONLY); break;
	default:  return NULL;
	}
	int fd = open(path, fl, 0644);
	if (fd < 0) return NULL;
	FILE *f = malloc(sizeof *f);
	if (!f) { close(fd); return NULL; }
	f->fd = fd; f->flags = 0; f->ungot = -1; f->rpos = 0; f->rlen = 0;
	return f;
}

/* fdopen — wrap an already-open fd in a FILE*. The mode string is advisory: the fd's real
 * access mode was fixed at open()/socket() time, so we only allocate the stream state. */
FILE *fdopen(int fd, const char *mode)
{
	(void)mode;
	if (fd < 0) return NULL;
	FILE *f = malloc(sizeof *f);
	if (!f) return NULL;
	f->fd = fd; f->flags = 0; f->ungot = -1; f->rpos = 0; f->rlen = 0;
	return f;
}

int fclose(FILE *f)
{
	if (!f) return EOF;
	int r = close(f->fd);
	if (f != &_stdin && f != &_stdout && f != &_stderr) free(f);
	return r;
}

int fgetc(FILE *f)
{
	if (f->ungot >= 0) { int c = f->ungot; f->ungot = -1; return c; }
	if (f->rpos >= f->rlen) {
		ssize_t n = read(f->fd, f->rbuf, sizeof f->rbuf);
		if (n <= 0) { f->flags |= (n == 0) ? 1 : 2; return EOF; }
		f->rlen = (int)n; f->rpos = 0;
	}
	return f->rbuf[f->rpos++];
}

int getc(FILE *f) { return fgetc(f); }

int ungetc(int c, FILE *f)
{
	if (c == EOF) return EOF;
	f->ungot = (unsigned char)c;
	f->flags &= ~1;
	return (unsigned char)c;
}

char *fgets(char *s, int size, FILE *f)
{
	if (size <= 0) return NULL;
	int i = 0;
	while (i < size - 1) {
		int c = fgetc(f);
		if (c == EOF) break;
		s[i++] = (char)c;
		if (c == '\n') break;
	}
	if (i == 0) return NULL;           /* nothing read (EOF at start) */
	s[i] = '\0';
	return s;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *f)
{
	size_t total = size * nmemb, got = 0;
	unsigned char *p = ptr;
	while (got < total) { int c = fgetc(f); if (c == EOF) break; p[got++] = (unsigned char)c; }
	return size ? got / size : 0;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f)
{
	size_t total = size * nmemb, done = 0;
	const unsigned char *p = ptr;
	while (done < total) {                       /* loop: a single write() may be short (pipe/socket) */
		ssize_t w = write(f->fd, p + done, total - done);
		if (w <= 0) { f->flags |= 2; break; }
		done += (size_t)w;
	}
	return size ? done / size : 0;
}

int fputc(int c, FILE *f)
{
	unsigned char ch = (unsigned char)c;
	if (write(f->fd, &ch, 1) != 1) { f->flags |= 2; return EOF; }
	return (unsigned char)c;
}

int putc(int c, FILE *f) { return fputc(c, f); }

int fputs(const char *s, FILE *f)
{
	size_t n = strlen(s);
	return write(f->fd, s, n) == (ssize_t)n ? (int)n : EOF;
}

int fflush(FILE *f) { (void)f; return 0; }   /* output is unbuffered */
int feof(FILE *f)   { return f->flags & 1; }
int ferror(FILE *f) { return f->flags & 2; }
void clearerr(FILE *f) { f->flags = 0; }

int fseek(FILE *f, long off, int whence)
{
	/* SEEK_CUR is relative to the LOGICAL position, but the kernel offset is ahead by the
	 * read-ahead bytes still in the buffer (+ any ungot char); discount them first. */
	if (whence == SEEK_CUR)
		off -= (f->rlen - f->rpos) + (f->ungot >= 0 ? 1 : 0);
	if (lseek(f->fd, off, whence) < 0) return -1;
	f->rpos = f->rlen = 0; f->ungot = -1; f->flags &= ~1;
	return 0;
}

long ftell(FILE *f)
{
	off_t r = lseek(f->fd, 0, SEEK_CUR);
	if (r < 0) return -1;
	return (long)r - (f->rlen - f->rpos) - (f->ungot >= 0 ? 1 : 0);   /* discount unconsumed bytes */
}

void rewind(FILE *f) { fseek(f, 0, SEEK_SET); f->flags = 0; }

int vfprintf(FILE *f, const char *fmt, va_list ap) { return vdprintf(f->fd, fmt, ap); }

int fprintf(FILE *f, const char *fmt, ...)
{
	va_list ap; va_start(ap, fmt);
	int n = vdprintf(f->fd, fmt, ap);
	va_end(ap);
	return n;
}

void perror(const char *s)
{
	if (s && *s) { fputs(s, stderr); fputs(": ", stderr); }
	fputs(strerror(errno), stderr);
	fputc('\n', stderr);
}
