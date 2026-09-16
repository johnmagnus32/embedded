# os/engine/engine.mk — the Make engine: a pure dependency-graph walker. All config + resolution
# live in bash (engine.sh, reading the product's local.conf); Make globs the recipes, asks engine.sh
# to resolve-dependencies for each, then to execute-recipe per recipe.
PRODUCT_DIR ?= $(CURDIR)
OS_DIR   := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

_RECIPE_PATHS := $(wildcard $(PRODUCT_DIR)/recipes-*/*/recipe.sh $(PRODUCT_DIR)/packages/*/recipe.sh $(OS_DIR)/../meta/recipes-*/*/recipe.sh)
_RECIPES      := $(sort $(foreach r,$(_RECIPE_PATHS),$(notdir $(patsubst %/,%,$(dir $(r))))))

define _recipe
$(1): $(shell PRODUCT_DIR=$(PRODUCT_DIR) $(OS_DIR)/engine.sh resolve-dependencies $(1))
	@PRODUCT_DIR=$(PRODUCT_DIR) $(OS_DIR)/engine.sh execute-recipe $(1)
endef
$(foreach r,$(_RECIPES),$(eval $(call _recipe,$(r))))

.PHONY: clean $(_RECIPES)
clean: ; rm -rf $(PRODUCT_DIR)/build
.DEFAULT_GOAL := image
