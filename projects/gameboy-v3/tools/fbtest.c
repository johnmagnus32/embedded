/* fbtest.c — fill /dev/fb0 with solid colors, gradients, and test patterns to verify the
 * RGB666 bit map, channel order, and panel geometry.
 *
 * Format-agnostic: it reads the framebuffer's actual bitfields (red/green/blue offset+length)
 * from FBIOGET_VSCREENINFO and packs each pixel accordingly, so a "red" fill really drives the
 * framebuffer's RED channel regardless of RGB565 / XRGB8888. That isolates the *panel wiring*
 * as the only variable in the test.
 *
 * Cross-compile static (runs on the musl/BusyBox rootfs with no libc dep):
 *   arm-buildroot-linux-musleabihf-gcc -static -O2 -o fbtest fbtest.c
 * Baked into the rootfs at /usr/bin/fbtest via the product overlay. Run:
 *   fbtest <mode>
 * Solid/gradient : white red green blue gray rgrad ggrad bgrad
 * Patterns       : bars checker grid cross
 *
 * What each pattern settles:
 *   red/green/blue  -> CHANNEL order   (does "red" show red, or blue? -> RGB vs BGR)
 *   rgrad/ggrad/bgrad -> BIT/MSB order (smooth left-dark->right-bright ramp = correct;
 *                                       scrambled/non-monotonic = inverted bit order)
 *   bars            -> 8 SMPTE-ish bars: white yellow cyan green magenta red blue black.
 *                      Compound colors (yellow=R+G, cyan=G+B, magenta=R+B) make a channel
 *                      swap instantly obvious.
 *   checker/grid    -> GEOMETRY: shear, wrap, missing pixels, sync/porch errors
 *   cross           -> full-extent + centering: 2px border + centre crosshair
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

static uint32_t pack(const struct fb_var_screeninfo *v, unsigned r, unsigned g, unsigned b)
{
	uint32_t p = 0;
	if (v->red.length)   p |= (uint32_t)(r >> (8 - v->red.length))   << v->red.offset;
	if (v->green.length) p |= (uint32_t)(g >> (8 - v->green.length)) << v->green.offset;
	if (v->blue.length)  p |= (uint32_t)(b >> (8 - v->blue.length))  << v->blue.offset;
	return p;
}

/* 8 vertical bars, brightest->darkest, that make channel swaps obvious. */
static void bar_color(unsigned idx, unsigned *r, unsigned *g, unsigned *b)
{
	static const unsigned char t[8][3] = {
		{255,255,255}, /* white   */
		{255,255,0},   /* yellow  = R+G */
		{0,255,255},   /* cyan    = G+B */
		{0,255,0},     /* green   */
		{255,0,255},   /* magenta = R+B */
		{255,0,0},     /* red     */
		{0,0,255},     /* blue    */
		{0,0,0},       /* black   */
	};
	if (idx > 7) idx = 7;
	*r = t[idx][0]; *g = t[idx][1]; *b = t[idx][2];
}

int main(int argc, char **argv)
{
	const char *mode = argc > 1 ? argv[1] : "white";
	int fd = open("/dev/fb0", O_RDWR);
	if (fd < 0) { perror("open /dev/fb0"); return 1; }

	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo f;
	if (ioctl(fd, FBIOGET_VSCREENINFO, &v) || ioctl(fd, FBIOGET_FSCREENINFO, &f)) {
		perror("FBIOGET_*SCREENINFO"); return 1;
	}
	fprintf(stderr, "fb %ux%u %ubpp  R@%u/%u G@%u/%u B@%u/%u  stride=%u\n",
		v.xres, v.yres, v.bits_per_pixel,
		v.red.offset, v.red.length, v.green.offset, v.green.length,
		v.blue.offset, v.blue.length, f.line_length);

	size_t sz = (size_t)f.line_length * v.yres;
	unsigned char *fb = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fb == MAP_FAILED) { perror("mmap"); return 1; }

	unsigned Bpp = v.bits_per_pixel / 8;
	unsigned lastx = v.xres > 1 ? v.xres - 1 : 1;
	unsigned cx = v.xres / 2, cy = v.yres / 2;
	const unsigned sq = 40;   /* checker square / grid spacing */

	for (unsigned y = 0; y < v.yres; y++) {
		unsigned char *row = fb + (size_t)y * f.line_length;
		for (unsigned x = 0; x < v.xres; x++) {
			unsigned r = 0, g = 0, b = 0, ramp = x * 255 / lastx;
			if      (!strcmp(mode, "white")) r = g = b = 255;
			else if (!strcmp(mode, "red"))   r = 255;
			else if (!strcmp(mode, "green")) g = 255;
			else if (!strcmp(mode, "blue"))  b = 255;
			else if (!strcmp(mode, "gray"))  r = g = b = ramp;
			else if (!strcmp(mode, "rgrad")) r = ramp;
			else if (!strcmp(mode, "ggrad")) g = ramp;
			else if (!strcmp(mode, "bgrad")) b = ramp;
			else if (!strcmp(mode, "bars"))  bar_color(x * 8 / v.xres, &r, &g, &b);
			else if (!strcmp(mode, "checker")) { if (((x / sq) + (y / sq)) & 1) r = g = b = 255; }
			else if (!strcmp(mode, "grid"))  { if (x % sq == 0 || y % sq == 0) r = g = b = 255; }
			else if (!strcmp(mode, "cross")) {
				if (x < 2 || x >= v.xres - 2 || y < 2 || y >= v.yres - 2 ||
				    x == cx || y == cy) r = g = b = 255;
			}
			else { fprintf(stderr,
				"modes: white red green blue gray rgrad ggrad bgrad bars checker grid cross\n");
				return 2; }

			uint32_t px = pack(&v, r, g, b);
			unsigned char *p = row + (size_t)x * Bpp;
			for (unsigned i = 0; i < Bpp; i++) p[i] = (px >> (8 * i)) & 0xff;
		}
	}
	fprintf(stderr, "filled: %s\n", mode);
	return 0;
}
