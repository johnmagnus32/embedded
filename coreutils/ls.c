/*
 * ls — list a directory (or the cwd). Exercises the new opendir/readdir over
 * getdents64, and stat() for the -l long form.
 *
 * Supported: `ls [-l] [-a] [path...]`. Directories are listed by their entries;
 * a non-directory path is printed as-is. Deliberately minimal — no sorting,
 * columns, or colour — just enough to prove the dirent + stat path end to end.
 */
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int opt_long, opt_all;

/* one-char type indicator from a d_type (DT_*). */
static char type_char(unsigned char t)
{
	switch (t) {
	case DT_DIR: return 'd';
	case DT_LNK: return 'l';
	case DT_CHR: return 'c';
	default:     return '-';
	}
}

static void list_one(const char *path)
{
	DIR *d = opendir(path);
	if (!d) {
		/* not a directory (or missing) — just print the name */
		puts(path);
		return;
	}
	struct dirent *e;
	while ((e = readdir(d)) != 0) {
		if (!opt_all && e->d_name[0] == '.')
			continue;                       /* hide dotfiles unless -a */
		if (opt_long) {
			/* stat the entry for size; build "<type> <size> <name>" */
			char full[512];
			int n = 0;
			for (const char *p = path; *p && n < (int)sizeof(full) - 1; p++) full[n++] = *p;
			if (n && full[n-1] != '/' && n < (int)sizeof(full) - 1) full[n++] = '/';
			for (const char *p = e->d_name; *p && n < (int)sizeof(full) - 1; p++) full[n++] = *p;
			full[n] = '\0';

			struct stat st;
			long size = 0;
			if (stat(full, &st) == 0) size = (long)st.st_size;
			dprintf(1, "%c %d %s\n", type_char(e->d_type), (int)size, e->d_name);
		} else {
			puts(e->d_name);
		}
	}
	closedir(d);
}

int main(int argc, char **argv)
{
	int i = 1;
	for (; i < argc && argv[i][0] == '-'; i++) {
		for (const char *o = argv[i] + 1; *o; o++) {
			if (*o == 'l') opt_long = 1;
			else if (*o == 'a') opt_all = 1;
		}
	}

	if (i >= argc) {
		list_one(".");
		return 0;
	}
	int multi = (argc - i) > 1;
	for (; i < argc; i++) {
		if (multi) dprintf(1, "%s:\n", argv[i]);
		list_one(argv[i]);
	}
	return 0;
}
