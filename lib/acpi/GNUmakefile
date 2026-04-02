.SUFFIXES:
MAKEFLAGS += --no-builtin-rules --no-print-directory

MODE ?= debug
ARCH ?= x86_64
BUILD_DIR ?= ../../build/$(ARCH)/$(MODE)/lib/acpi

ifeq ($(ARCH),x86_64)
    TARGET := x86_64-unknown-yerp
    ARCH_CFLAGS := -m64 -march=x86-64 -mcmodel=kernel -mno-red-zone \
                   -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-avx -msoft-float
else
    $(error Unsupported ARCH: $(ARCH))
endif

TC_INST ?= $(abspath ../../build/toolchain/install)
CC := $(TC_INST)/bin/clang
AR := $(TC_INST)/bin/llvm-ar

OBJ_DIR := $(BUILD_DIR)/obj
OUTPUT := $(BUILD_DIR)/libacpi.a

CFLAGS_COMMON := -std=c23 -pipe -Wall -Wextra -Werror \
                 --target=$(TARGET) \
                 -ffreestanding -fno-stack-check -fno-pic -fwrapv \
                 -fno-omit-frame-pointer \
                 $(ARCH_CFLAGS) \
                 -isystem.. \
                 $(EXTRA_CFLAGS)

ifeq ($(MODE),debug)
    CFLAGS_MODE := -O0 -g3
else ifeq ($(MODE),release)
    CFLAGS_MODE := -O3 -g -DNDEBUG
else
    $(error Invalid MODE: $(MODE))
endif

CFLAGS := $(CFLAGS_COMMON) $(CFLAGS_MODE)

SRCS := $(shell find . -type f -name '*.c')
OBJS := $(patsubst ./%.c, $(OBJ_DIR)/%.o, $(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	@mkdir -p $(dir $@)
	@echo "[AR] $@"
	@$(AR) rcs $@ $^

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR)

-include $(DEPS)
