# hostpackages/binman-venv/recipe.sh — the binman/pylibfdt python venv, as a HOST PACKAGE. U-Boot's
# SPL+binman build needs these modules; the Bootlin toolchain's bundled python lacks them. LAZY:
# provisioned iff U-Boot is in the build (the uboot recipe declares it), so a full-custom build never
# touches Python. No PKG_VERSION/SITE/SHA — a pyvenv is pinned by its module list, not a tarball SHA.
PKG_NAME=binman-venv
PKG_CLASS=native
inherit host-pyvenv
PKG_PYMODULES="setuptools pyelftools pyyaml importlib_resources"
PKG_HOST_DEST=${PYENV_DIR}
# The venv EMBEDS the host base python (its python3 is a symlink to it), which the recipehash can't see
# (host env is deliberately not hashed). Assumption: the host python stays put; if it moves/upgrades the
# venv breaks and the U-Boot build fails loud on the binman import — run `clean`. (Real fix someday: a
# python3-native recipe so nothing depends on the host python.)
