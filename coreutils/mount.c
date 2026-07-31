/*
 * mount — attach a filesystem. Minimal util-linux/busybox-compatible form:
 *     mount [-t TYPE] SOURCE TARGET
 * enough for /init's  `mount -t proc none /proc` (+ sysfs, devtmpfs).
 *
 * Wraps mount(2). On our custom kernel SYS_mount currently succeeds as a no-op
 * (no procfs/sysfs/devtmpfs backend yet — future work); on mainline it really
 * mounts. Either way /init can issue the standard mounts portably instead of
 * relying on a shell "command not found". Deliberately minimal: no -o options,
 * no /etc/fstab, no bind/remount — just the TYPE/SOURCE/TARGET path /init uses.
 */
#include <sys/mount.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
	const char *type = "none";
	const char *args[2];       /* SOURCE, TARGET */
	int n = 0;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-t") == 0) {
			if (++i >= argc) { dprintf(2, "mount: -t needs a TYPE\n"); return 1; }
			type = argv[i];
		} else if (n < 2) {
			args[n++] = argv[i];
		} else {
			dprintf(2, "mount: too many arguments\n"); return 1;
		}
	}
	if (n != 2) {
		dprintf(2, "usage: mount [-t TYPE] SOURCE TARGET\n");
		return 1;
	}

	if (mount(args[0], args[1], type, 0, 0) != 0) {
		dprintf(2, "mount: %s on %s (type %s) failed\n", args[0], args[1], type);
		return 1;
	}
	return 0;
}
