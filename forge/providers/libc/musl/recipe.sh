# providers/libc/musl/recipe.sh — musl (prebuilt-complete libc). musl-as-libc is really a TOOLCHAIN
# property: the musl cross-compiler *is* the libc, sysroot and all (the toolchain-musl host package).
# So this recipe is thin — no source, no build.sh, no libc-profile.sh. The libc class + cc-profile
# dispatch on a PROPERTY (does that file exist?), not the name; musl ships neither file, so both
# no-op it. The recipe exists so libc is validated + resolved generically (no enum in resolve.mk).
PKG_NAME=musl

PKG_FETCH=prebuilt
# Declaring the dep makes musl's taskhash track the toolchain (a bump ripples into linked packages).
# No PKG_ARTIFACT: musl's build is a no-op, nothing to cache.
PKG_HOST_DEPENDS=toolchain-musl
inherit libc
