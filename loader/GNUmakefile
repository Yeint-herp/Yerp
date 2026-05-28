.SUFFIXES:
MAKEFLAGS += --no-builtin-rules --no-print-directory

MODE ?= debug

BUILD_DIR ?= $(abspath ../build)
LOADER_DIR ?= $(abspath ../loader)

LOADER_BUILD_DIR := $(BUILD_DIR)/loader
OBJ_DIR := $(LOADER_BUILD_DIR)/obj

LOADER_SOURCE_DIR := $(LOADER_DIR)/source

OUTPUT := $(BUILD_DIR)/BOOTX64.EFI

DCC := dcc
LD := lld-link

TARGET := x86_64-coff

DCFLAGS := -I$(LOADER_SOURCE_DIR) \
	-fno-simd -fno-x87 -fno-red-zone

LDFLAGS := /subsystem:efi_application \
	/machine:x64 \
	/entry:efi_main \
	/nodefaultlib \
	/dll \
	/fixed:no /dynamicbase:no /nxcompat:no

ifeq ($(MODE),debug)
DCFLAGS += -fbounds-check -fno-omit-frame-pointer -gdwarf
else
DCFLAGS += -gnone
endif

DC_SRCS := $(shell find source -type f -name '*.dc')
DC_OBJS := $(patsubst source/%.dc, $(OBJ_DIR)/%.o, $(DC_SRCS))

all: $(OUTPUT)

.PHONY: all clean

$(OUTPUT): $(DC_OBJS)
	@mkdir -p $(dir $@)
	@echo "[LD] $@"
	@$(LD) $(LDFLAGS) /out:$@ $(DC_OBJS)

$(OBJ_DIR)/%.o: source/%.dc
	@mkdir -p $(dir $@)
	@echo "[DCC] $<"
	@$(DCC) -target $(TARGET) $(DCFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(OUTPUT)
