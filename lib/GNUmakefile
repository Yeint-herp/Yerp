.SUFFIXES:
MAKEFLAGS += --no-builtin-rules --no-print-directory

MODE ?= debug
ARCH ?= x86_64
BUILD_DIR ?= ../build/$(ARCH)/$(MODE)/lib

SUBDIRS := acpi

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	@$(MAKE) -C $@ MODE=$(MODE) ARCH=$(ARCH) BUILD_DIR=$(abspath $(BUILD_DIR)/$@) \
		TC_INST=$(TC_INST)

clean:
	@for d in $(SUBDIRS); do \
		$(MAKE) -C $$d clean BUILD_DIR=$(abspath $(BUILD_DIR)/$$d); \
	done
