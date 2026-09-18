/*
 * dirent.c — opendir/readdir/closedir over getdents64(2).
 *
 * A DIR holds the directory fd plus a buffer we fill with raw getdents64 records
 * (struct dirent64, variable-length via d_reclen). readdir() walks that
 * buffer one record at a time, copying the fields into a stable `struct dirent`,
 * and refills from the kernel when the buffer is exhausted. Single-threaded, one
 * static-free DIR per open via malloc.
 */
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "syscall_internal.h"

struct __dirstream {
	int           fd;
	int           buf_used;    /* valid bytes in buf                 */
	int           buf_pos;     /* next record offset within buf      */
	struct dirent de;          /* returned to the caller             */
	char          buf[2048];   /* raw getdents64 records             */
};

DIR *opendir(const char *path)
{
	int fd = openat(AT_FDCWD, path, O_RDONLY | O_DIRECTORY, 0);
	if (fd < 0)
		return NULL;
	DIR *d = malloc(sizeof(*d));
	if (!d) { close(fd); return NULL; }
	d->fd = fd;
	d->buf_used = 0;
	d->buf_pos = 0;
	return d;
}

struct dirent *readdir(DIR *d)
{
	if (d->buf_pos >= d->buf_used) {
		/* refill from the kernel */
		long n = __ret(__sys3(SYS_getdents64, d->fd, d->buf, sizeof(d->buf)));
		if (n <= 0)
			return NULL;            /* end of directory or error */
		d->buf_used = (int)n;
		d->buf_pos = 0;
	}

	struct dirent64 *k = (struct dirent64 *)(d->buf + d->buf_pos);
	d->de.d_ino  = k->d_ino;
	d->de.d_type = k->d_type;
	/* name is NUL-terminated within the record; copy safely */
	int i = 0;
	const char *name = (const char *)k + DIRENT64_NAMEOFF;
	while (name[i] && i < (int)sizeof(d->de.d_name) - 1) {
		d->de.d_name[i] = name[i];
		i++;
	}
	d->de.d_name[i] = '\0';

	if (k->d_reclen == 0)               /* defensive: avoid an infinite loop */
		d->buf_pos = d->buf_used;
	else
		d->buf_pos += k->d_reclen;
	return &d->de;
}

int closedir(DIR *d)
{
	if (!d) return -1;
	int fd = d->fd;
	free(d);
	return close(fd);
}

int alphasort(const struct dirent **a, const struct dirent **b)
{
	return strcmp((*a)->d_name, (*b)->d_name);
}

/* scandir: collect (a malloc'd copy of) each entry passing `filter` into a grown array,
 * sort with `compar`, and hand back the array via *namelist. Returns the count, or -1. */
int scandir(const char *dir, struct dirent ***namelist,
            int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **, const struct dirent **))
{
	DIR *d = opendir(dir);
	if (!d) return -1;

	struct dirent **list = NULL;
	size_t n = 0, cap = 0;
	struct dirent *e;
	while ((e = readdir(d))) {
		if (filter && !filter(e)) continue;
		if (n == cap) {
			size_t nc = cap ? cap * 2 : 16;
			struct dirent **nl = realloc(list, nc * sizeof *nl);
			if (!nl) goto fail;
			list = nl; cap = nc;
		}
		struct dirent *copy = malloc(sizeof *copy);
		if (!copy) goto fail;
		*copy = *e;
		list[n++] = copy;
	}
	closedir(d);
	if (compar)
		qsort(list, n, sizeof *list, (int (*)(const void *, const void *))compar);
	*namelist = list;
	return (int)n;

fail:
	for (size_t i = 0; i < n; i++) free(list[i]);
	free(list);
	closedir(d);
	return -1;
}
