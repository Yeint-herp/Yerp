.SUFFIXES:
MAKEFLAGS += --no-builtin-rules --no-print-directory

MODE ?= debug
ARCH ?= x86_64

TARGET_ASM := x86_64-unknown-none
TARGET_CC := x86_64-unknown-none-elf

BUILD_DIR ?= ../build/supervisor
BIN_DIR := $(BUILD_DIR)/bin
OBJ_DIR := $(BUILD_DIR)/obj

OUTPUT := $(BIN_DIR)/supervisor
MAP_FILE := $(BIN_DIR)/supervisor.map

CC := clang
LD := ld.lld

ifeq ($(ARCH),x86_64)
    TARGET_ASM  := x86_64-unknown-none
    TARGET_CC   := x86_64-unknown-none-elf
    ARCH_CFLAGS := -m64 -march=x86-64 -mcmodel=kernel -mno-red-zone \
                   -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-avx -msoft-float
else
    $(error Unsupported ARCH: $(ARCH))
endif

CFLAGS_COMMON := -std=c23 -pipe -Wall -Wextra -Werror \
                 --target=$(TARGET_CC) \
                 -ffreestanding -fno-stack-check -fstack-protector-strong \
                 -fno-pic -fno-omit-frame-pointer -fwrapv -fdollars-in-identifiers \
                 -Wvla -Wshadow \
                 -I. -Isource -include const.h \
                 $(ARCH_CFLAGS) -DkArch=$(ARCH)

ifeq ($(MODE),debug)
    CFLAGS_MODE := -O0 -g3
    LTO_FLAGS   :=
else ifeq ($(MODE),release)
    CFLAGS_MODE := -O3 -g -DNDEBUG
    LTO_FLAGS   := -flto
else
    $(error Invalid MODE: $(MODE))
endif

CFLAGS := $(CFLAGS_COMMON) $(CFLAGS_MODE) $(LTO_FLAGS)

ASMFLAGS := -x assembler-with-cpp --target=$(TARGET_ASM) -g
LDFLAGS := -nostdlib -static -z max-page-size=0x1000 \
            -T link.ld \
            --orphan-handling=error --no-dynamic-linker \
            -Map=$(MAP_FILE)

ALL_C_SRCS := $(shell find source -type f -name '*.c')
ALL_ASM_SRCS := $(shell find source -type f -name '*.asm')

C_SRCS := $(filter-out source/arch/%, $(ALL_C_SRCS))   $(filter source/arch/$(ARCH)/%, $(ALL_C_SRCS))
ASM_SRCS := $(filter-out source/arch/%, $(ALL_ASM_SRCS)) $(filter source/arch/$(ARCH)/%, $(ALL_ASM_SRCS))

C_OBJS := $(patsubst source/%.c, $(OBJ_DIR)/%.o, $(C_SRCS))
ASM_OBJS := $(patsubst source/%.asm, $(OBJ_DIR)/%.o, $(ASM_SRCS))

OBJS := $(C_OBJS) $(ASM_OBJS)
DEPS      := $(OBJS:.o=.d)

.PHONY: all clean

all: $(OUTPUT)

$(OUTPUT): $(OBJS) link.ld
	@mkdir -p $(dir $@)
	@echo "[LD] $@ ($(MODE))"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(OBJ_DIR)/%.o: source/%.c
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%.o: source/%.asm
	@mkdir -p $(dir $@)
	@echo "[AS] $<"
	@$(CC) $(ASMFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
