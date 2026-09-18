# t-init — fixture recipe: provides init. Trivial (no source, no compile) so os/test can
# exercise the ENGINE (resolution, cache, task pipeline) without building anything real. do_build just
# logs a marker the runner greps for to tell "ran" from "cached".
PKG_NAME=t-init
PKG_PROVIDES=init
PKG_FETCH=none
do_build() { log "T-BUILD-RAN t-init"; }
