MAKEFLAGS += -rR
.SUFFIXES:

ARCH ?= x86_64
BUILD_DIR := $(abspath ./build)

QEMUFLAGS_BASE := -m 2G -display sdl
QEMUFLAGS_DEBUG := -serial mon:stdio -serial file:$(BUILD_DIR)/serial_log.txt -D $(BUILD_DIR)/qemu_log.txt -d int -M smm=off --no-shutdown --no-reboot
QEMUFLAGS ?= $(QEMUFLAGS_BASE) $(QEMUFLAGS_DEBUG)

override IMAGE_NAME := Yerp-$(ARCH)

HOST_CC := cc
HOST_CFLAGS := -g -O2 -pipe
HOST_CPPFLAGS :=
HOST_LDFLAGS :=
HOST_LIBS :=

.PHONY: all
all: $(IMAGE_NAME).iso

.PHONY: all-hdd
all-hdd: $(IMAGE_NAME).hdd

.PHONY: run-x86_64
run-x86_64: ovmf/ovmf-code-$(ARCH).fd $(IMAGE_NAME).iso
	qemu-system-x86_64 \
		-M q35 \
		-drive if=pflash,unit=0,format=raw,file=ovmf/ovmf-code-x86_64.fd,readonly=on \
		-cdrom $(IMAGE_NAME).iso \
		$(QEMUFLAGS)

.PHONY: run-aarch64
run-aarch64: ovmf/ovmf-code-$(ARCH).fd $(IMAGE_NAME).iso
	qemu-system-$(ARCH) \
		-M virt \
		-cpu cortex-a72 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-mouse \
		-drive if=pflash,unit=0,format=raw,file=ovmf/ovmf-code-$(ARCH).fd,readonly=on \
		-cdrom $(IMAGE_NAME).iso \
		$(QEMUFLAGS)

.PHONY: run
run:
ifeq ($(ARCH),x86_64)
	$(MAKE) run-x86_64
else ifeq ($(ARCH),aarch64)
	$(MAKE) run-aarch64
else
	$(error Unsupported ARCH: $(ARCH))
endif

.PHONY: run-x86_64-hdd
run-x86_64-hdd: ovmf/ovmf-code-$(ARCH).fd $(IMAGE_NAME).hdd
	qemu-system-$(ARCH) \
		-M q35 \
		-drive if=pflash,unit=0,format=raw,file=ovmf/ovmf-code-$(ARCH).fd,readonly=on \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)

.PHONY: run-hdd-aarch64
run-hdd-aarch64: ovmf/ovmf-code-$(ARCH).fd $(IMAGE_NAME).hdd
	qemu-system-$(ARCH) \
		-M virt \
		-cpu cortex-a72 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-mouse \
		-drive if=pflash,unit=0,format=raw,file=ovmf/ovmf-code-$(ARCH).fd,readonly=on \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)

.PHONY: run-hdd
run-hdd:
ifeq ($(ARCH),x86_64)
	$(MAKE) run-hdd-x86_64
else ifeq ($(ARCH),aarch64)
	$(MAKE) run-hdd-aarch64
else
	$(error Unsupported ARCH: $(ARCH))
endif

ovmf/ovmf-code-$(ARCH).fd:
	mkdir -p ovmf
	curl -Lo $@ https://github.com/osdev0/edk2-ovmf-nightly/releases/latest/download/ovmf-code-$(ARCH).fd

limine/limine:
	rm -rf limine
	git clone https://github.com/limine-bootloader/limine.git --branch=v9.x-binary --depth=1
	$(MAKE) -C limine \
		CC="$(HOST_CC)" \
		CFLAGS="$(HOST_CFLAGS)" \
		CPPFLAGS="$(HOST_CPPFLAGS)" \
		LDFLAGS="$(HOST_LDFLAGS)" \
		LIBS="$(HOST_LIBS)"

.PHONY: debug
debug:
ifeq ($(ARCH),x86_64)
	@qemu-system-x86_64 \
		-M q35 \
		-drive if=pflash,unit=0,format=raw,file=ovmf/ovmf-code-x86_64.fd,readonly=on \
		-cdrom $(IMAGE_NAME).iso \
		-s -S \
		$(QEMUFLAGS) & \
	QEMU_PID=$$!; \
	sleep 1; \
	gdb build/bin-$(ARCH)/yerp.elf -ex "target remote :1234"; \
	kill $$QEMU_PID || true
else ifeq ($(ARCH),aarch64)
	@qemu-system-aarch64 \
		-M virt \
		-cpu cortex-a72 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-mouse \
		-drive if=pflash,unit=0,format=raw,file=ovmf/ovmf-code-$(ARCH).fd,readonly=on \
		-cdrom $(IMAGE_NAME).iso \
		-s -S \
		$(QEMUFLAGS) & \
	QEMU_PID=$$!; \
	sleep 1; \
	gdb build/bin-$(ARCH)/yerp.elf -ex "target remote :1234"; \
	kill $$QEMU_PID || true
else
	$(error Unsupported ARCH: $(ARCH))
endif



deps:
	./get-deps
	touch deps

.PHONY: kernel
kernel: deps
	@mkdir -p build
	$(MAKE) -C kernel BUILD_DIR=$(BUILD_DIR)

$(IMAGE_NAME).iso: limine/limine kernel
	rm -rf iso_root
	mkdir -p iso_root/boot
	cp -v build/bin-$(ARCH)/yerp.elf iso_root/boot/
	mkdir -p iso_root/boot/limine
	cp -v limine.conf iso_root/boot/limine/
	mkdir -p iso_root/EFI/BOOT

	cp -v limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin iso_root/boot/limine/
	cp -v limine/BOOTX64.EFI iso_root/EFI/BOOT/
	cp -v limine/BOOTIA32.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
	./limine/limine bios-install $(IMAGE_NAME).iso

	rm -rf iso_root

$(IMAGE_NAME).hdd: limine/limine kernel
	rm -f $(IMAGE_NAME).hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=$(IMAGE_NAME).hdd

	PATH=$$PATH:/usr/sbin:/sbin sgdisk $(IMAGE_NAME).hdd -n 1:2048 -t 1:ef00 -m 1
	./limine/limine bios-install $(IMAGE_NAME).hdd

	mformat -i $(IMAGE_NAME).hdd@@1M
	mmd -i $(IMAGE_NAME).hdd@@1M ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M build/bin-$(ARCH)/yerp.elf ::/boot
	mcopy -i $(IMAGE_NAME).hdd@@1M limine.conf ::/boot/limine

	mcopy -i $(IMAGE_NAME).hdd@@1M limine/limine-bios.sys ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/BOOTX64.EFI ::/EFI/BOOT
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/BOOTIA32.EFI ::/EFI/BOOT

.PHONY: clean
clean:
	$(MAKE) -C kernel clean
	rm -rf iso_root $(IMAGE_NAME).iso $(IMAGE_NAME).hdd

.PHONY: distclean
distclean:
	$(MAKE) -C kernel distclean
	rm -rf iso_root *.iso *.hdd deps limine ovmf

