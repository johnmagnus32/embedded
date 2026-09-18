# t-app — fixture package (in PACKAGES). PKG_CLASS=target + PKG_DEPENDS=libc so the engine's
# implied "a target that links libc also needs the compiler" edge kicks in: recipe_deps should add
# cross-cc to its resolved prerequisites.
PKG_NAME=t-app
PKG_CLASS=target
PKG_DEPENDS=libc
PKG_FETCH=none
do_build() { log "T-BUILD-RAN t-app"; }
