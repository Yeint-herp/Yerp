.SUFFIXES:
MAKEFLAGS += --no-builtin-rules --no-builtin-variables --no-print-directory
SHELL := /bin/sh
.SHELLFLAGS := -eu -c

OS_NAME := yerp
MODE ?= debug
ARCH ?= x86_64

ifeq ($(filter $(MODE),debug release),)
	$(error Mode must be 'debug' or 'release'.)
endif

ifeq ($(filter $(ARCH),x86_64),)
    $(error Arch must be set to 'x86_64'.)
endif

BUILD_ROOT := $(abspath build)/$(ARCH)
TOOLS_DIR := $(abspath tools)
LIB_DIR := $(abspath lib)
BUILD_DIR := $(BUILD_ROOT)/$(MODE)
LIB_BUILD_DIR := $(BUILD_DIR)/lib
ISO_DIR := $(BUILD_DIR)/iso_root
HDD_DIR := $(BUILD_DIR)/hdd_root

HDD_IMG := $(BUILD_DIR)/$(OS_NAME).hdd
ISO_IMG := $(BUILD_DIR)/$(OS_NAME).iso

SUPERVISOR_ELF := $(BUILD_DIR)/supervisor/bin/supervisor
LIBACPI := $(LIB_BUILD_DIR)/acpi/libacpi.a

QEMU := qemu-system-x86_64
XORRISO := xorriso
DD := dd
SGDISK := sgdisk
MFORMAT := mformat
MMD := mmd
MCOPY := mcopy
GIT := git
CURL := curl

DEPS_DIR := deps
LIMINE_DIR := $(DEPS_DIR)/limine
LIMINE_REPO := https://github.com/limine-bootloader/limine.git
LIMINE_BRANCH := v10.x-binary

OVMF_DIR := $(DEPS_DIR)/ovmf
OVMF_CODE := $(OVMF_DIR)/OVMF_CODE.fd
OVMF_VARS := $(OVMF_DIR)/OVMF_VARS.fd

QEMUFLAGS := -M q35 -m 2G -cpu max -smp 4 -display sdl \
			 -M accel=tcg,smm=off \
			 -drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
			 -drive if=pflash,format=raw,readonly=on,file=$(OVMF_VARS) \
			 -debugcon mon:stdio -no-reboot -no-shutdown \
			 -d int -D $(BUILD_ROOT)/qemu_log.txt

.PHONY: all clean distclean run-hdd run-iso hdd iso libs supervisor deps

all: run-hdd

libs:
	@$(MAKE) -C lib MODE=$(MODE) ARCH=$(ARCH) BUILD_DIR=$(abspath $(LIB_BUILD_DIR)) EXTRA_CFLAGS="$(EXTRA_CFLAGS)"

deps: $(LIMINE_DIR)/limine $(OVMF_CODE)

supervisor: libs
	@$(MAKE) -C supervisor MODE=$(MODE) ARCH=$(ARCH) BUILD_DIR=$(abspath $(BUILD_DIR)/supervisor) TOOLS_DIR=$(TOOLS_DIR) \
		LIB_DIR=$(LIB_DIR) LIB_BUILD_DIR=$(abspath $(LIB_BUILD_DIR)) \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)"

hdd: supervisor deps
	@mkdir -p $(BUILD_DIR)
	@rm -f $(HDD_IMG)
	@$(DD) if=/dev/zero bs=1M count=64 of=$(HDD_IMG) status=none
	@$(SGDISK) $(HDD_IMG) -n 1:2048 -t 1:ef00 -c 1:"EFI System" > /dev/null
	@$(MFORMAT) -i $(HDD_IMG)@@1M
	@$(MMD) -i $(HDD_IMG)@@1M ::/EFI ::/EFI/BOOT ::/boot
	@$(MCOPY) -i $(HDD_IMG)@@1M $(SUPERVISOR_ELF) ::/boot/supervisor
	@$(MCOPY) -i $(HDD_IMG)@@1M limine.conf ::/boot/limine.conf
	@$(MCOPY) -i $(HDD_IMG)@@1M $(LIMINE_DIR)/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
	@echo "[HDD] $(HDD_IMG) ($(MODE))"

iso: supervisor deps
	@mkdir -p $(ISO_DIR)/boot/limine $(ISO_DIR)/EFI/BOOT
	@cp $(SUPERVISOR_ELF) $(ISO_DIR)/boot/supervisor
	@cp limine.conf $(ISO_DIR)/boot/
	@cp $(LIMINE_DIR)/limine-bios.sys $(LIMINE_DIR)/limine-bios-cd.bin \
		$(LIMINE_DIR)/limine-uefi-cd.bin $(ISO_DIR)/boot/limine/
	@cp $(LIMINE_DIR)/BOOTX64.EFI $(ISO_DIR)/EFI/BOOT/

	@$(XORRISO) -as mkisofs -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISO_DIR) -o $(ISO_IMG) > /dev/null 2>&1

	@$(LIMINE_DIR)/limine bios-install $(ISO_IMG)
	@rm -rf $(ISO_DIR)
	@echo "[ISO] $(ISO_IMG) ($(MODE))"

run-hdd: hdd
	@$(QEMU) $(QEMUFLAGS) -drive file=$(HDD_IMG),format=raw

run-iso: iso
	@$(QEMU) $(QEMUFLAGS) -cdrom $(ISO_IMG)

$(LIMINE_DIR)/limine:
	@mkdir -p $(DEPS_DIR)
	@if [ ! -d "$(LIMINE_DIR)" ]; then \
		$(GIT) clone --branch $(LIMINE_BRANCH) --depth 1 $(LIMINE_REPO) $(LIMINE_DIR); \
	fi
	@$(MAKE) -C $(LIMINE_DIR)

$(OVMF_CODE):
	@mkdir -p $(OVMF_DIR)
	@$(CURL) -Lo $(OVMF_CODE) https://retrage.github.io/edk2-nightly/bin/RELEASEX64_OVMF_CODE.fd
	@$(CURL) -Lo $(OVMF_VARS) https://retrage.github.io/edk2-nightly/bin/RELEASEX64_OVMF_VARS.fd

clean:
	@rm -rf $(BUILD_DIR)

distclean:
	@rm -rf $(BUILD_ROOT) $(DEPS_DIR)
