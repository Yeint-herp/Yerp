.SUFFIXES:
MAKEFLAGS += --no-builtin-rules --no-builtin-variables --no-print-directory
SHELL := /bin/sh
.SHELLFLAGS := -eu -c

MODE ?= debug
ARCH ?= x86_64
BUILD_DIR ?= $(abspath ../build/$(ARCH)/$(MODE)/driver)
TC_INST ?= $(abspath ../build/toolchain/install)
LIB_DIR ?= $(abspath ../lib)
LIB_BUILD_DIR ?= $(abspath ../build/$(ARCH)/$(MODE)/lib)
SUPERVISOR_DIR ?= $(abspath ../supervisor)

DRIVERS := $(sort $(patsubst %/GNUmakefile,%,$(wildcard */GNUmakefile)))

.PHONY: all clean $(DRIVERS)

all: $(DRIVERS)

$(DRIVERS):
	@$(MAKE) -C $@ MODE=$(MODE) ARCH=$(ARCH) \
		BUILD_DIR=$(BUILD_DIR)/$@ \
		TC_INST=$(TC_INST) \
		LIB_DIR=$(LIB_DIR) \
		LIB_BUILD_DIR=$(LIB_BUILD_DIR) \
		SUPERVISOR_DIR=$(SUPERVISOR_DIR)

clean:
	@rm -rf $(BUILD_DIR)
