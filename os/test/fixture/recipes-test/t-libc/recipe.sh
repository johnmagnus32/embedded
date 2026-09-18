# t-libc — fixture recipe: provides libc. Trivial (no source, no compile).
PKG_NAME=t-libc
PKG_PROVIDES=libc
PKG_FETCH=none
do_build() { log "T-BUILD-RAN t-libc"; }
