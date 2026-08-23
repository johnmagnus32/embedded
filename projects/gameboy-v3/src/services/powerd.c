/* services/powerd.c — battery + brightness + idle/power policy. NOT a display
 * client (it never touches the compositor) — it polls sysfs. This is the pattern
 * every non-display service follows: audiod (ALSA mixer + jack-detect routing),
 * hapticd (input-FF), libraryd (SD scan), btd (BlueZ). One daemon per hardware
 * policy domain so a hung game can't take the device down.
 *
 * MOCK: reads the MAX17048 capacity + can set the backlight. The idle→dim→off→
 * suspend state machine, the low-battery haptic warning, the power-key long-press
 * clean shutdown (gpio-poweroff), and the cpufreq governor switch are TODO.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define BATT "/sys/class/power_supply/max17048/capacity"   /* MAX17048 fuel gauge */
#define BL   "/sys/class/backlight/backlight/brightness"    /* TPS61165 via pwm-bl */

static int read_int(const char *path)
{
	char b[32];
	int fd = open(path, O_RDONLY);
	if (fd < 0) return -1;
	int n = read(fd, b, sizeof(b) - 1);
	close(fd);
	if (n <= 0) return -1;
	b[n] = 0;
	return atoi(b);
}

static void write_int(const char *path, int v)
{
	char b[32];
	int fd = open(path, O_WRONLY);
	if (fd < 0) return;
	int n = snprintf(b, sizeof(b), "%d", v);
	if (write(fd, b, n) < 0) { /* best-effort */ }
	close(fd);
}

int main(void)
{
	fprintf(stderr, "powerd: up\n");
	for (;;) {
		int pct = read_int(BATT);
		if (pct >= 0) fprintf(stderr, "powerd: battery %d%%\n", pct);
		/* TODO: idle timer -> dim BL -> screen off -> suspend-to-RAM (once mainline
		 * T113 deep-sleep is proven); low-batt (<=5%) warning via hapticd; power-key
		 * long-press -> clean shutdown + gpio-poweroff; cpufreq perf when a game is
		 * foreground, powersave in the menu. */
		(void)write_int;   /* write_int(BL, level) — used by the idle policy above */
		sleep(10);
	}
	return 0;
}
