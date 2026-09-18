# t-cc-initial — fixture recipe: provides cross-cc-initial (the stage-1 compiler slot).
PKG_NAME=t-cc-initial
PKG_PROVIDES=cross-cc-initial
PKG_HOST_CC_PREFIX=testcc-
PKG_FETCH=none
do_build() { log "T-BUILD-RAN t-cc-initial"; }
