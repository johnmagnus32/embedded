/*
 * pwd — print the current working directory (via getcwd -> getcwd(2)).
 */
#include <unistd.h>
#include <stdio.h>

int main(void)
{
	char buf[256];
	if (getcwd(buf, sizeof(buf)) == 0) {
		dprintf(2, "pwd: cannot get cwd\n");
		return 1;
	}
	puts(buf);
	return 0;
}
