# forge/engine/engine.mk — the forge Make engine: a pure dependency-graph walker, nothing else.
# All config + provider resolution live in bash (forge-env.sh, reading the product's local.conf).
# Make just globs the recipes, asks forge-env for each node's resolved prerequisites, and runs
# run-recipe.sh per node. A product Makefile includes this; PRODUCT_DIR is the only thing it needs.
PRODUCT_DIR ?= $(CURDIR)
FORGE_DIR   := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
# The one seed the scripts need — passed explicitly (Make's parse-time $(shell) does NOT inherit an
# `export`ed var). CLI knob overrides (KERNEL=mainline …) reach the scripts via Make's own env already.
_SEED       := PRODUCT_DIR=$(PRODUCT_DIR)

# Overrides go through the ENVIRONMENT (`LIBC=custom make …`) so forge-env sees them at parse time; a
# command-line make-var (`make LIBC=custom`) does NOT reach $(shell), so it would silently build the
# default — warn loudly if one slips in.
$(if $(MAKEOVERRIDES),$(warning forge: override via env — 'VAR=val make …', not 'make VAR=val' (a command-line make-var does not reach the resolver)))

_RECIPES := $(wildcard $(PRODUCT_DIR)/recipes-*/*/recipe.sh $(PRODUCT_DIR)/packages/*/recipe.sh $(FORGE_DIR)/../meta/recipes-*/*/recipe.sh)
_NODES   := $(sort $(foreach r,$(_RECIPES),$(notdir $(patsubst %/,%,$(dir $(r))))))   # node = recipe dir name (unique; a product recipe shadows forge's)

# One rule per node (= recipe name): its prerequisites come from forge-env (virtual/* -> provider,
# ${PACKAGES} expanded, the compiler edge + the `make` barrier added), then run-recipe.sh builds it.
define _recipe
$(1): $(shell $(_SEED) $(FORGE_DIR)/forge-env.sh deps $(1))
	@$(_SEED) $(FORGE_DIR)/run-recipe.sh $(1)
endef
$(foreach n,$(_NODES),$(eval $(call _recipe,$(n))))

.PHONY: clean $(_NODES)
clean: ; rm -rf $(PRODUCT_DIR)/build
.DEFAULT_GOAL := image
