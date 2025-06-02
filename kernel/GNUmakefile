MAKEFLAGS += -rR
.SUFFIXES:

BUILD_DIR ?= ../build

override OUTPUT := yerp.elf

ARCH := x86_64
PREFIX := /usr/local

CC := cc
CXX := c++

ifeq ($(ARCH),aarch64)
	CC := aarch64-linux-gnu-gcc
	CXX := aarch64-linux-gnu-g++
	LD := aarch64-linux-gnu-ld
	AS := aarch64-linux-gnu-as
endif

CFLAGS := -g -O2 -pipe
CXXFLAGS := -g -O2 -pipe
CPPFLAGS :=

NASMFLAGS := -F dwarf -g

LDFLAGS :=

ifneq ($(shell ( test '$(MAKECMDGOALS)' = clean || test '$(MAKECMDGOALS)' = distclean ); echo $$?),0)
ifeq ($(shell ( ! test -d std || ! test -d std/cc-runtime || ! test -f std/limine.h ); echo $$?),0)
$(error Please run the ./get-deps script first)
endif
endif

override CC_IS_CLANG := $(shell ! $(CC) --version 2>/dev/null | grep 'clang' >/dev/null 2>&1; echo $$?)

override CFLAGS += \
	-Wall \
	-Wextra \
	-Werror \
	$(if $(filter $(ARCH),x86_64),-std=c23 -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel,-std=c23) \
	-nostdinc \
	-ffreestanding \
	-fno-stack-protector \
	-fno-stack-check \
	-fno-PIC \
	-ffunction-sections \
	-fdata-sections	

override CXXFLAGS += \
	-Wall \
	-Wextra \
	-Werror \
	$(if $(filter $(ARCH),x86_64),-std=c++23 -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel,-std=c++23) \
	-nostdinc \
	-ffreestanding \
	-fno-stack-protector \
	-fno-stack-check \
	-fno-PIC \
	-ffunction-sections \
	-fdata-sections

override CPPFLAGS := \
	-isystem std \
	-isystem Source \
	-include Source/constants.h \
	$(CPPFLAGS) \
	-DLIMINE_API_REVISION=3 \
	-MMD \
	-MP

override NASMFLAGS += \
	-Wall

ifeq ($(CC_IS_CLANG),1)
	override CC += \
		-target $(if $(filter $(ARCH),x86_64),x86_64-unknown-none,aarch64-unknown-none)
endif

ifeq ($(ARCH),x86_64)
	override CPPFLAGS += -DC4_X86_64
else ifeq ($(ARCH),aarch64)
	override CPPFLAGS += -DC4_AARCH64
else
	$(error Unsupported ARCH '$(ARCH)'. Only x86_64 or aarch64 are allowed.)
endif

override LDFLAGS += \
	-Wl,-m,elf_$(if $(filter $(ARCH),x86_64),x86_64,aarch64) \
	-Wl,--build-id=none \
	-nostdlib \
	-static \
	-z max-page-size=0x1000 \
	-Wl,--gc-sections \
	-T $(if $(filter $(ARCH),x86_64),x86_64,aarch64)_linker.ld
	
override NASMFLAGS += \
	-f elf64
	
override SRCFILES := $(shell find -L . -type f | sed 's|^\./||' | LC_ALL=C sort)
override CFILES := $(filter %.c,$(SRCFILES))
override CXXFILES := $(filter %.cc,$(SRCFILES))
override ASFILES := $(filter %.S,$(SRCFILES))
override NASMFILES := $(filter %.asm,$(SRCFILES))

OBJDIR := $(BUILD_DIR)/obj-$(ARCH)
BINDIR := $(BUILD_DIR)/bin-$(ARCH)

override OBJ := $(addprefix $(OBJDIR)/,$(CFILES:.c=.c.o) $(CXXFILES:.cc=.cc.o) $(ASFILES:.S=.S.o))
override OBJ += $(addprefix $(OBJDIR)/,$(NASMFILES:.asm=.asm.o))

override HEADER_DEPS := $(addprefix $(OBJDIR)/,$(CFILES:.c=.c.d) $(CXXFILES:.cc=.cc.d) $(ASFILES:.S=.S.d))

.PHONY: all
all: $(BINDIR)/$(OUTPUT)

-include $(HEADER_DEPS)

$(BINDIR)/$(OUTPUT): GNUmakefile $(OBJ)
	mkdir -p "$$(dirname $@)"
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $@

$(OBJDIR)/%.c.o: %.c GNUmakefile
	mkdir -p "$$(dirname $@)"
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJDIR)/%.S.o: %.S GNUmakefile
	mkdir -p "$$(dirname $@)"
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJDIR)/%.asm.o: %.asm GNUmakefile
	mkdir -p "$$(dirname $@)"
	nasm $(NASMFLAGS) $< -o $@

$(OBJDIR)/%.cc.o: %.cc GNUmakefile
	mkdir -p "$$(dirname $@)"
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(BINDIR) $(OBJDIR)

.PHONY: distclean
distclean:
	rm -rf bin-* obj-* std std/cc-runtime std/limine.h

.PHONY: install
install: all
	install -d "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)"
	install -m 644 bin-$(ARCH)/$(OUTPUT) "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)/$(OUTPUT)-$(ARCH)"

.PHONY: uninstall
uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)/$(OUTPUT)-$(ARCH)"
	-rmdir "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)"
