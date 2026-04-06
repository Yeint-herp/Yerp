.SUFFIXES:
MAKEFLAGS += --no-builtin-rules --no-builtin-variables --no-print-directory
SHELL := /bin/sh
.SHELLFLAGS := -eu -c

DRIVER_NAME := template

MODE ?= debug
ARCH ?= x86_64
BUILD_DIR ?= $(abspath ../../build/$(ARCH)/$(MODE)/driver/$(DRIVER_NAME))
TC_INST ?= $(abspath ../../build/toolchain/install)
LIB_DIR ?= $(abspath ../../lib)
LIB_BUILD_DIR ?= $(abspath ../../build/$(ARCH)/$(MODE)/lib)
SUPERVISOR_DIR ?= $(abspath ../../supervisor)

OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
OUTPUT := $(BIN_DIR)/$(DRIVER_NAME)

CC := $(TC_INST)/bin/clang
LD := $(TC_INST)/bin/ld.lld

ifeq ($(ARCH),x86_64)
    TARGET := x86_64-unknown-yerp
    ARCH_CFLAGS := -m64 -march=x86-64 -mcmodel=kernel -mno-red-zone \
                   -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-avx -msoft-float
else
    $(error Unsupported ARCH: $(ARCH))
endif

CFLAGS := -std=c23 -pipe -Wall -Wextra -Werror \
          --target=$(TARGET) \
          -ffreestanding -fno-stack-check -fno-pic -fno-omit-frame-pointer \
          -fwrapv -fdollars-in-identifiers \
          -Wvla -Wshadow \
          -isystem$(SUPERVISOR_DIR) -isystem$(SUPERVISOR_DIR)/source -isystem$(LIB_DIR) \
          -include $(SUPERVISOR_DIR)/const.h \
          $(ARCH_CFLAGS)

ifeq ($(MODE),debug)
    CFLAGS += -O0 -g3
else ifeq ($(MODE),release)
    CFLAGS += -O3 -g -DNDEBUG -flto
endif

LDFLAGS := -nostdlib -static -r

C_SRCS := $(shell find source -type f -name '*.c' 2>/dev/null)
ASM_SRCS := $(shell find source -type f -name '*.asm' 2>/dev/null)

C_OBJS := $(patsubst source/%.c, $(OBJ_DIR)/%.o, $(C_SRCS))
ASM_OBJS := $(patsubst source/%.asm, $(OBJ_DIR)/%.o, $(ASM_SRCS))
OBJS := $(C_OBJS) $(ASM_OBJS)
DEPS := $(C_OBJS:.o=.d)

.PHONY: all clean

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	@mkdir -p $(dir $@)
	@echo "[DRV-LD] $(DRIVER_NAME) ($(MODE))"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(OBJ_DIR)/%.o: source/%.c
	@mkdir -p $(dir $@)
	@echo "[DRV-CC] $(DRIVER_NAME)/$<"
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%.o: source/%.asm
	@mkdir -p $(dir $@)
	@echo "[DRV-AS] $(DRIVER_NAME)/$<"
	@$(CC) -x assembler-with-cpp --target=$(TARGET) -g -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR)

-include $(DEPS)
