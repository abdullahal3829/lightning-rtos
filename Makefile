CC=gcc
LD=ld
GENFW ?= "$(CURDIR)/tools/edk2/BaseTools/Source/C/bin/GenFw"
DIR=$(CURDIR)
AS=as
TARCH ?= $(shell $(CC) -dumpmachine | cut -d'-' -f1)
BUILD_DIR ?= "$(CURDIR)/build"
EDKII_DIR = "$(CURDIR)/tools/edk2"
EDK2_INCLUDE_ROOT := $(EDKII_DIR)/MdePkg/Include

all:
	@python3 "$(CURDIR)/scripts/confdir.py"
	@mkdir -p "$(BUILD_DIR)"
	@# @echo "Current ARCH: $(TARCH)"
	@$(MAKE) -C "$(EDKII_DIR)/BaseTools"
	@chmod +x "$(EDKII_DIR)/edksetup.sh"
	@cd "$(EDKII_DIR)" && . ./edksetup.sh && cd "$(CURDIR)"; \
		$(MAKE) -C "$(CURDIR)/boot" EDKII_DIR="$(EDKII_DIR)" EDK2_INCLUDE_ROOT="$(EDK2_INCLUDE_ROOT)"; \
		$(MAKE) -C "$(CURDIR)/kernel" BUILD_DIR="$(BUILD_DIR)" EDKII_DIR="$(EDKII_DIR)" EDK2_INCLUDE_ROOT="$(EDK2_INCLUDE_ROOT)";
	@mkdir -p "$(BUILD_DIR)/iso/boot";
	@cp -f "$(BUILD_DIR)/boot/image.img" "$(BUILD_DIR)/iso/boot/bootldr.bin";
	@cp -f "$(BUILD_DIR)/kernel/oskrnl" "$(BUILD_DIR)/iso/oskrnl";
	@truncate -s 1474560 "$(BUILD_DIR)/iso/boot/bootldr.bin";
	@xorriso -as mkisofs \
		-iso-level 3 \
		-full-iso9660-filenames \
		-R -J -l \
		-V "OS" \
		-b boot/bootldr.bin \
		-no-emul-boot \
		-boot-load-size 1024 \
		-c boot.cat \
		-o "$(BUILD_DIR)/output.iso" \
		"$(BUILD_DIR)/iso"

clean:
	@rm -rf "$(BUILD_DIR)"

export CC
export LD
export AS
export DIR
export BUILD_DIR
export TARCH
export EDKII_DIR
export EDK2_INCLUDE_ROOT