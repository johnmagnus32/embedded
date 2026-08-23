/* touchtest.c — bring-up probe for the FT7311 capacitive touch (ATM0500D27-CT) over i2c-dev.
 *
 * 1. Scans /dev/i2c-* for an ACK at 0x38 (i2cdetect-style) — prints every address that answers.
 * 2. Reads the FocalTech chip/firmware/vendor ID registers.
 * 3. Polls the touch report block and prints coordinates as you touch the screen.
 *
 * FocalTech report map (edt-ft5x06 compatible): reg 0x02 low nibble = #touches; then 6 bytes
 * per point starting at 0x03: [XH: ev<<6 | x[11:8]][XL: x[7:0]][YH: id<<4 | y[11:8]][YL: y[7:0]].
 * event: 0=down 1=up 2=contact 3=none.
 *
 * Cross-compile static (musl):
 *   arm-buildroot-linux-musleabihf-gcc -static -O2 -o touchtest touchtest.c
 * Baked into the rootfs at /usr/bin/touchtest via the product overlay. Run:
 *   touchtest [bus] [seconds]      (no args = auto-scan all buses, report 20 s)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#define FT_ADDR 0x38

static int rd_reg(int fd, uint8_t reg, uint8_t *buf, int n)
{
	if (write(fd, &reg, 1) != 1) return -1;
	if (read(fd, buf, n) != n) return -1;
	return 0;
}

/* Open a bus that ACKs 0x38, set the slave addr, return fd (or -1). */
static int find_touch(int forced_bus)
{
	int lo = forced_bus >= 0 ? forced_bus : 0;
	int hi = forced_bus >= 0 ? forced_bus : 12;
	for (int b = lo; b <= hi; b++) {
		char path[32];
		snprintf(path, sizeof path, "/dev/i2c-%d", b);
		int fd = open(path, O_RDWR);
		if (fd < 0) continue;
		fprintf(stderr, "scanning %s:\n", path);
		int hit = 0;
		for (int a = 0x08; a <= 0x77; a++) {
			if (ioctl(fd, I2C_SLAVE, a) < 0) continue;
			uint8_t v;
			if (read(fd, &v, 1) >= 0) {
				fprintf(stderr, "  ACK 0x%02x%s\n", a, a == FT_ADDR ? "  <- FT7311 touch" : "");
				if (a == FT_ADDR) hit = 1;
			}
		}
		if (hit) { ioctl(fd, I2C_SLAVE, FT_ADDR); return fd; }
		close(fd);
	}
	return -1;
}

int main(int argc, char **argv)
{
	int forced_bus = argc > 1 ? atoi(argv[1]) : -1;
	int secs = argc > 2 ? atoi(argv[2]) : 20;

	int fd = find_touch(forced_bus);
	if (fd < 0) {
		fprintf(stderr, "\nFT7311 (0x38) NOT found on any bus.\n"
			"Check: RST tied HIGH (3.3V), VDD=3.3V present, SCL/SDA not swapped,\n"
			"and add ~4.7k pull-ups SCL->3.3V / SDA->3.3V if the bus is otherwise empty.\n");
		return 2;
	}

	uint8_t chip = 0, fw = 0, vend = 0;
	rd_reg(fd, 0xA3, &chip, 1);   /* chip id  */
	rd_reg(fd, 0xA6, &fw, 1);     /* firmware */
	rd_reg(fd, 0xA8, &vend, 1);   /* focaltech vendor id */
	fprintf(stderr, "\nFT7311 @0x38 present: chip_id=0x%02x fw=0x%02x vendor=0x%02x\n",
		chip, fw, vend);
	fprintf(stderr, "Touch the screen now — reporting for %d s ...\n", secs);

	int iters = secs * 20;   /* 50 ms poll */
	int seen = 0;
	for (int i = 0; i < iters; i++) {
		uint8_t r[16];
		if (rd_reg(fd, 0x00, r, 16) == 0) {
			int n = r[2] & 0x0f;
			if (n > 0 && n <= 10) {
				int ev = r[3] >> 6;
				int id = r[5] >> 4;
				int x = ((r[3] & 0x0f) << 8) | r[4];
				int y = ((r[5] & 0x0f) << 8) | r[6];
				fprintf(stderr, "  touches=%d  id=%d ev=%d  x=%d y=%d\n", n, id, ev, x, y);
				seen++;
			}
		}
		usleep(50000);
	}
	fprintf(stderr, "done — %d touch samples seen.\n", seen);
	return 0;
}
