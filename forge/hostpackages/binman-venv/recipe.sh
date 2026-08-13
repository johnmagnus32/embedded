# hostpackages/binman-venv/recipe.sh — the binman/pylibfdt python venv, as a HOST PACKAGE. U-Boot's
# SPL+binman build needs these modules; the Bootlin toolchain's bundled python lacks them. LAZY:
# provisioned iff U-Boot is in the build (the uboot recipe declares it), so a full-custom build never
# touches Python. No PKG_VERSION/SITE/SHA — a pyvenv is pinned by its module list, not a tarball SHA.
PKG_NAME=binman-venv
inherit host-pyvenv
PKG_PYMODULES="setuptools pyelftools pyyaml importlib_resources"
PKG_HOST_DEST=${PYENV_DIR}
# Cache output is the venv's python3 (a symlink to the host base python), NOT the venv dir: the venv
# EMBEDS the host interpreter, which the taskhash can't see (host env is deliberately not hashed), so
# if that base python moves/upgrades the venv silently breaks. `[ -e ]` follows the symlink, so a
# vanished base python fails the check + forces a rebuild; a bare dir would wrongly pass.
PKG_HOST_VERIFY_BIN=python3
