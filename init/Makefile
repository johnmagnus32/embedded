# init/Makefile — the custom init (+ initctl, later phases). Standalone: `make` (host) or
# `make CC=arm-buildroot-linux-musleabihf-gcc` (cross for the board). forge's init provider
# overrides O= and DESTDIR= for the in-tree cross build + install.
CC     ?= cc
# out-of-source; build/ is gitignored repo-wide (never in-tree next to source).
O      ?= build
WARN   := -Wall -Wextra
ALL_CFLAGS  := $(WARN) -O2 -std=c11 $(CFLAGS)     # init.c defines _GNU_SOURCE itself
ALL_LDFLAGS := $(LDFLAGS)
# crt0 + libc archive for a -nostdlib libc (custom). Empty on musl/host (crt+libc auto-linked).
# crt0 goes BEFORE the source, the libc archive AFTER (link-order: archive resolves the objects).
LIBC_CRT ?=
LIBC_LIB ?=

all: $(O)/init $(O)/initctl

$(O):
	mkdir -p $(O)

$(O)/init: init.c | $(O)
	$(CC) $(ALL_CFLAGS) $(ALL_LDFLAGS) $(LIBC_CRT) init.c $(LIBC_LIB) -o $@

$(O)/initctl: initctl.c | $(O)
	$(CC) $(ALL_CFLAGS) $(ALL_LDFLAGS) $(LIBC_CRT) initctl.c $(LIBC_LIB) -o $@

# Installs PID 1 as /init — the initramfs entry point the kernel execs. Deliberately NOT /sbin/init:
# busybox ships /sbin/init -> bin/busybox, and merging a real /sbin/init over that symlink would
# dereference it and clobber busybox. /init is ours alone (busybox creates /linuxrc, never /init).
# initctl (the control client) goes to /sbin/initctl.
install: all
	install -D -m 0755 $(O)/init    $(DESTDIR)/init
	install -D -m 0755 $(O)/initctl $(DESTDIR)/sbin/initctl

clean:
	rm -rf $(O)

.PHONY: all install clean
