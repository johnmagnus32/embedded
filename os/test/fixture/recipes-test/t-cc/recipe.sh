# t-cc — fixture recipe: provides cross-cc. PKG_HOST_CC_PREFIX lets load_env resolve a
# (fake) CROSS_COMPILE without a real toolchain — nothing here actually compiles.
PKG_NAME=t-cc
PKG_PROVIDES=cross-cc
PKG_HOST_CC_PREFIX=testcc-
PKG_FETCH=none
do_build() { log "T-BUILD-RAN t-cc"; }
