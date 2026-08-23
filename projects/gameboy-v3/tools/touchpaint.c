/* touchpaint.c — end-to-end panel proof: draw a red square wherever the FT7311 touch reports.
 *
 * Combines the two halves we validated separately: reads touch coordinates over i2c-dev (FT7311
 * @0x38) and paints directly into /dev/fb0 (format-agnostic pack, like fbtest). A red square
 * appearing under your finger proves BOTH that touch registers correctly AND that we draw at the
 * right screen location. Squares accumulate (no per-frame clear) so a few taps leave a visible
 * trail; the raw coordinates are also printed to stderr (over UART) so we can spot any
 * axis-swap/flip vs. where you actually touched.
 *
 * Cross-compile static (musl):
 *   arm-buildroot-linux-musleabihf-gcc -static -O2 -o touchpaint touchpaint.c
 * Baked into the rootfs at /usr/bin/touchpaint via the product overlay. Run:
 *   touchpaint [seconds] [bus]     (default 60 s, auto-scan buses for 0x38)
 * (Unbind fbcon first: echo 0 > /sys/class/vtconsole/vtcon1/bind)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/i2c-dev.h>

#define FT_ADDR 0x38
#define SQ_HALF 20          /* red square is (2*SQ_HALF+1) px per side */

static uint32_t pack(const struct fb_var_screeninfo *v, unsigned r, unsigned g, unsigned b)
{
	uint32_t p = 0;
	if (v->red.length)   p |= (uint32_t)(r >> (8 - v->red.length))   << v->red.offset;
	if (v->green.length) p |= (uint32_t)(g >> (8 - v->green.length)) << v->green.offset;
	if (v->blue.length)  p |= (uint32_t)(b >> (8 - v->blue.length))  << v->blue.offset;
	return p;
}

static int rd_reg(int fd, uint8_t reg, uint8_t *buf, int n)
{
	if (write(fd, &reg, 1) != 1) return -1;
	if (read(fd, buf, n) != n) return -1;
	return 0;
}

static int find_touch(int forced_bus)
{
	int lo = forced_bus >= 0 ? forced_bus : 0;
	int hi = forced_bus >= 0 ? forced_bus : 12;
	for (int b = lo; b <= hi; b++) {
		char path[32];
		snprintf(path, sizeof path, "/dev/i2c-%d", b);
		int fd = open(path, O_RDWR);
		if (fd < 0) continue;
		if (ioctl(fd, I2C_SLAVE, FT_ADDR) < 0) { close(fd); continue; }
		uint8_t v;
		if (read(fd, &v, 1) >= 0) { fprintf(stderr, "FT7311 found on %s\n", path); return fd; }
		close(fd);
	}
	return -1;
}

int main(int argc, char **argv)
{
	int secs = argc > 1 ? atoi(argv[1]) : 60;
	int forced_bus = argc > 2 ? atoi(argv[2]) : -1;

	int ffd = open("/dev/fb0", O_RDWR);
	if (ffd < 0) { perror("open /dev/fb0"); return 1; }
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo f;
	if (ioctl(ffd, FBIOGET_VSCREENINFO, &v) || ioctl(ffd, FBIOGET_FSCREENINFO, &f)) {
		perror("FBIOGET_*SCREENINFO"); return 1;
	}
	unsigned Bpp = v.bits_per_pixel / 8;
	size_t sz = (size_t)f.line_length * v.yres;
	unsigned char *fb = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, ffd, 0);
	if (fb == MAP_FAILED) { perror("mmap"); return 1; }

	int ifd = find_touch(forced_bus);
	if (ifd < 0) { fprintf(stderr, "FT7311 (0x38) not found — check pull-ups/RST/wiring.\n"); return 2; }

	uint32_t bg  = pack(&v, 0x18, 0x18, 0x18);   /* dark gray background */
	uint32_t red = pack(&v, 0xff, 0x00, 0x00);

	/* paint the background once */
	for (unsigned y = 0; y < v.yres; y++) {
		unsigned char *row = fb + (size_t)y * f.line_length;
		for (unsigned x = 0; x < v.xres; x++) {
			unsigned char *p = row + (size_t)x * Bpp;
			for (unsigned i = 0; i < Bpp; i++) p[i] = (bg >> (8 * i)) & 0xff;
		}
	}
	fprintf(stderr, "fb %ux%u %ubpp — touch the screen; red squares mark hits (%d s)\n",
		v.xres, v.yres, v.bits_per_pixel, secs);

	int iters = secs * 20;   /* 50 ms poll */
	int painted = 0;
	for (int i = 0; i < iters; i++) {
		uint8_t r[16];
		if (rd_reg(ifd, 0x00, r, 16) == 0) {
			int n = r[2] & 0x0f;
			if (n > 0 && n <= 10) {
				int x = ((r[3] & 0x0f) << 8) | r[4];
				int y = ((r[5] & 0x0f) << 8) | r[6];
				if (x < 0) x = 0; if (x >= (int)v.xres) x = v.xres - 1;
				if (y < 0) y = 0; if (y >= (int)v.yres) y = v.yres - 1;
				fprintf(stderr, "  touch x=%d y=%d\n", x, y);
				for (int yy = y - SQ_HALF; yy <= y + SQ_HALF; yy++) {
					if (yy < 0 || yy >= (int)v.yres) continue;
					unsigned char *row = fb + (size_t)yy * f.line_length;
					for (int xx = x - SQ_HALF; xx <= x + SQ_HALF; xx++) {
						if (xx < 0 || xx >= (int)v.xres) continue;
						unsigned char *p = row + (size_t)xx * Bpp;
						for (unsigned k = 0; k < Bpp; k++) p[k] = (red >> (8 * k)) & 0xff;
					}
				}
				painted++;
			}
		}
		usleep(50000);
	}
	fprintf(stderr, "done — %d touch samples painted.\n", painted);
	return 0;
}
