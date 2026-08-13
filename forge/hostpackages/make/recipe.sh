# hostpackages/make/recipe.sh — GNU Make >= 4.0, as a HOST PACKAGE (the kernel Makefile requires
# it). CONDITIONAL: PKG_HOST_SKIP_IF no-ops the build if the host's own make is already new enough.
PKG_NAME=make
inherit host-autotools
PKG_VERSION=4.4.1
PKG_SITE=https://mirrors.kernel.org/gnu/make
PKG_SOURCE=make-4.4.1.tar.gz
PKG_SHA256=dd16fb1d67bfab79a72f5e8390735c49e3e8e70b4945a15ab1f81ddb78658fb3
PKG_HOST_DEST=${HOSTMAKE_DIR}
PKG_HOST_VERIFY_BIN=make
PKG_HOST_SKIP_IF=host_have_make_ge4   # a skip-if value must be a simple command

# True iff `make` on PATH is GNU Make >= 4.0. Lives here — its only user is the skip-if above.
host_have_make_ge4() {
  case "$(make --version 2>/dev/null | sed -n '1s/.*GNU Make //p')" in
    4.*|5.*|6.*|7.*|8.*|9.*) return 0 ;; *) return 1 ;;
  esac
}
