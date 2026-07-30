/*
 * echo — print arguments separated by spaces, followed by a newline.
 * Supports -n (suppress the trailing newline). The classic minimal echo.
 */
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
{
	int i = 1;
	int newline = 1;

	if (i < argc && strcmp(argv[i], "-n") == 0) {
		newline = 0;
		i++;
	}

	for (; i < argc; i++) {
		write(1, argv[i], strlen(argv[i]));
		if (i + 1 < argc)
			write(1, " ", 1);
	}
	if (newline)
		write(1, "\n", 1);
	return 0;
}
