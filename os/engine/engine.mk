# os/engine/engine.mk — the Make engine: a pure dependency-graph walker. All config + resolution
# live in bash (os-env.sh, reading the product's local.conf); Make globs the recipes, asks os-env
# for each recipe's prerequisites, and runs run-recipe.sh per recipe.
PRODUCT_DIR ?= $(CURDIR)
OS_DIR   := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

_RECIPE_PATHS := $(wildcard $(PRODUCT_DIR)/recipes-*/*/recipe.sh $(PRODUCT_DIR)/packages/*/recipe.sh $(OS_DIR)/../meta/recipes-*/*/recipe.sh)
_RECIPES      := $(sort $(foreach r,$(_RECIPE_PATHS),$(notdir $(patsubst %/,%,$(dir $(r))))))

define _recipe
$(1): $(shell PRODUCT_DIR=$(PRODUCT_DIR) $(OS_DIR)/os-env.sh deps $(1))
	@PRODUCT_DIR=$(PRODUCT_DIR) $(OS_DIR)/run-recipe.sh $(1)
endef
$(foreach r,$(_RECIPES),$(eval $(call _recipe,$(r))))

.PHONY: clean $(_RECIPES)
clean: ; rm -rf $(PRODUCT_DIR)/build
.DEFAULT_GOAL := image
