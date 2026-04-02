.SUFFIXES:
MAKEFLAGS += --no-builtin-rules --no-builtin-variables --no-print-directory
SHELL := /bin/sh
.SHELLFLAGS := -eu -c

include config.mk

unexport CC LD AR

ROOT := $(abspath ..)
TC_BUILD := $(ROOT)/build/toolchain
LLVM_SRC := $(TC_BUILD)/llvm-project
LLVM_BUILD := $(TC_BUILD)/build
LLVM_INST := $(TC_BUILD)/install
PATCH_DIR := $(abspath patches)
STAMP_DIR := $(TC_BUILD)/.stamps

STAMP_FETCH := $(STAMP_DIR)/fetched
STAMP_PATCHED := $(STAMP_DIR)/patched
STAMP_BUILD := $(STAMP_DIR)/built

.PHONY: all clean distclean fetch patch build snapshot reset-patches

all: build

$(STAMP_FETCH):
	@mkdir -p $(STAMP_DIR)
	@if [ ! -d "$(LLVM_SRC)/.git" ]; then \
		echo "[TC] cloning llvm-project $(LLVM_VERSION)"; \
		git clone --depth 1 --branch $(LLVM_VERSION) $(LLVM_REPO) $(LLVM_SRC); \
	fi

	@cd $(LLVM_SRC) && git tag -f _upstream_base
	@touch $@

fetch: $(STAMP_FETCH)

$(STAMP_PATCHED): $(STAMP_FETCH) $(PATCH_DIR)/series $(wildcard $(PATCH_DIR)/*.patch)
	@cd $(LLVM_SRC) && git checkout . && git clean -fdx -e build
	@cd $(LLVM_SRC) && git checkout _upstream_base 2>/dev/null || true
	@if [ -s "$(PATCH_DIR)/series" ]; then \
		echo "[TC] applying patch series"; \
		while IFS= read -r p; do \
			case "$$p" in \
				''|\#*) continue ;; \
			esac; \
			echo "  [PATCH] $$p"; \
			cd $(LLVM_SRC) && git apply --index "$(PATCH_DIR)/$$p"; \
		done < "$(PATCH_DIR)/series"; \
		cd $(LLVM_SRC) && git commit -m "yerp: applied patch series" --allow-empty; \
	fi

	@touch $@

patch: $(STAMP_PATCHED)

LLVM_CMAKE_FLAGS := \
	-G Ninja \
	-S $(LLVM_SRC)/llvm \
	-B $(LLVM_BUILD) \
	-DCMAKE_BUILD_TYPE=$(LLVM_BUILD_TYPE) \
	-DCMAKE_INSTALL_PREFIX=$(LLVM_INST) \
	-DLLVM_ENABLE_PROJECTS="$(LLVM_PROJECTS)" \
	-DLLVM_TARGETS_TO_BUILD="$(LLVM_TARGETS_TO_BUILD)" \
	-DLLVM_PARALLEL_COMPILE_JOBS=$(LLVM_JOBS) \
	-DLLVM_PARALLEL_LINK_JOBS=$(LLVM_LINK_JOBS) \
	-DLLVM_USE_LINKER=lld \
	-DLLVM_INSTALL_UTILS=OFF \
	-DLLVM_INCLUDE_TESTS=OFF \
	-DLLVM_INCLUDE_EXAMPLES=OFF \
	-DLLVM_INCLUDE_BENCHMARKS=OFF \
	-DLLVM_INCLUDE_DOCS=OFF \
	-DLLVM_ENABLE_BINDINGS=OFF \
	-DLLVM_ENABLE_TERMINFO=OFF \
	-DLLVM_ENABLE_ZLIB=OFF \
	-DLLVM_ENABLE_ZSTD=OFF \
	-DCLANG_ENABLE_STATIC_ANALYZER=OFF \
	-DCLANG_ENABLE_ARCMT=OFF

$(STAMP_BUILD): $(STAMP_PATCHED)
	@echo "[TC] configuring LLVM"
	@cmake $(LLVM_CMAKE_FLAGS)
	@echo "[TC] building LLVM"
	@ninja -C $(LLVM_BUILD) -j$(LLVM_JOBS)
	@echo "[TC] installing to $(LLVM_INST)"
	@ninja -C $(LLVM_BUILD) install
	@touch $@

build: $(STAMP_BUILD)

snapshot:
	@if [ ! -d "$(LLVM_SRC)/.git" ]; then \
		echo "error: no LLVM source tree to snapshot"; exit 1; \
	fi
	@mkdir -p $(PATCH_DIR)
	@cd $(LLVM_SRC) && \
		git add -A && \
		DIFF=$$(git diff --cached HEAD); \
		if [ -z "$$DIFF" ]; then \
			git reset HEAD >/dev/null 2>&1; \
			echo "[TC] worktree is clean, nothing to snapshot"; \
			exit 0; \
		fi; \
		NEXT=$$(printf '%04d' $$(( $$(ls $(PATCH_DIR)/*.patch 2>/dev/null | wc -l) + 1 ))); \
		PATCH_NAME="$${NEXT}-wip.patch"; \
		echo "[TC] snapshotting: patches/$$PATCH_NAME"; \
		git diff --cached HEAD > "$(PATCH_DIR)/$$PATCH_NAME"; \
		echo "$$PATCH_NAME" >> "$(PATCH_DIR)/series"; \
		git commit -m "yerp: snapshot $$PATCH_NAME"

reset-patches:
	@echo "[TC] resetting LLVM to upstream base"
	@cd $(LLVM_SRC) && git checkout . && git clean -fdx -e build
	@cd $(LLVM_SRC) && git checkout _upstream_base 2>/dev/null || true
	@rm -f $(STAMP_PATCHED) $(STAMP_BUILD)

clean:
	@rm -rf $(LLVM_BUILD) $(LLVM_INST) $(STAMP_DIR)

distclean:
	@rm -rf $(TC_BUILD)
