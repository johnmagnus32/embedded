# init/Makefile — the custom init (+ initctl, later phases). Standalone: `make` (host) or
# `make CC=arm-buildroot-linux-musleabihf-gcc` (cross for the board). forge's init provider
# overrides O= and DESTDIR= for the in-tree cross build + install.
CC     ?= cc
# out-of-source; build/ is gitignored repo-wide (never in-tree next to source).
O      ?= build
WARN   := -Wall -Wextra
ALL_CFLAGS  := $(WARN) -O2 -std=c11 $(CFLAGS)     # init.c defines _GNU_SOURCE itself
ALL_LDFLAGS := $(LDFLAGS)

all: $(O)/init $(O)/initctl

$(O):
	mkdir -p $(O)

$(O)/init: init.c | $(O)
	$(CC) $(ALL_CFLAGS) init.c $(ALL_LDFLAGS) -o $@

$(O)/initctl: initctl.c | $(O)
	$(CC) $(ALL_CFLAGS) initctl.c $(ALL_LDFLAGS) -o $@

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
