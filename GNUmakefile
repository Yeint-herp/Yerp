.SUFFIXES:
MAKEFLAGS += --no-builtin-rules --no-builtin-variables --no-print-directory
SHELL := /bin/sh
.SHELLFLAGS := -eu -c

export ROOT := $(abspath .)
export PROJECT_NAME := Yerp

BUILD_DIR ?= $(abspath build)

MODE ?= debug

all: loader hdd run

.PHONY: all loader hdd run clean

LOADER_DIR ?= $(abspath loader)

loader:
	@$(MAKE) -C $(LOADER_DIR) \
		BUILD_DIR=$(BUILD_DIR) LOADER_DIR=$(LOADER_DIR) \
		MODE=$(MODE)

HDD_IMAGE := $(BUILD_DIR)/$(PROJECT_NAME).hdd
HDD_SIZE_MIB := 64

hdd: loader
	@mkdir -p $(BUILD_DIR)
	@rm -rf $(HDD_IMAGE)

	@dd if=/dev/zero of=$(HDD_IMAGE) bs=1M count=$(HDD_SIZE_MIB) status=none
	
	@sgdisk $(HDD_IMAGE) -n 1:2048 -t 1:ef00 -c 1:"EFI System" > /dev/null
	@mformat -i $(HDD_IMAGE)@@1M
	@mmd -i $(HDD_IMAGE)@@1M ::/EFI ::/EFI/BOOT

	@mcopy -i $(HDD_IMAGE)@@1M $(BUILD_DIR)/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
	@echo "[HDD] $(HDD_IMAGE)"

OVMF_CODE := $(BUILD_DIR)/OVMF_CODE.fd
OVMF_VARS := $(BUILD_DIR)/OVMF_VARS.fd

QEMUFLAGS := \
	-M q35 -cpu max,+x2apic -M accel=tcg,smm=off \
	-m 2G -smp 4 \
	-display sdl \
	-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
	-drive if=pflash,format=raw,readonly=on,file=$(OVMF_VARS) \
	-debugcon mon:stdio -no-reboot \
	-d int,cpu_reset -D $(BUILD_DIR)/qemu_log.txt \
	-s

run: $(OVMF_CODE) hdd
	@qemu-system-x86_64 \
		$(QEMUFLAGS) \
		-drive file=$(HDD_IMAGE),format=raw,if=none,id=nvme0 -device nvme,drive=nvme0,serial=deadbeef

$(OVMF_CODE):
	@curl -Lo $(OVMF_CODE) https://retrage.github.io/edk2-nightly/bin/RELEASEX64_OVMF_CODE.fd
	@curl -Lo $(OVMF_VARS) https://retrage.github.io/edk2-nightly/bin/RELEASEX64_OVMF_VARS.fd

clean:
	@$(MAKE) -C $(LOADER_DIR) clean
	@rm -rf $(HDD_IMAGE)
