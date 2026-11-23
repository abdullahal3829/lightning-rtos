#
# Top-level build for Lightning RTOS.
# Builds BIOS + UEFI bootloaders, kernel, and hybrid ISO.
#

CC=gcc
LD=ld
AS=as
GENFW ?= "$(CURDIR)/tools/edk2/BaseTools/Source/C/bin/GenFw"
DIR=$(CURDIR)
TARCH ?= $(shell $(CC) -dumpmachine | cut -d'-' -f1)
BUILD_DIR ?= "$(CURDIR)/build"
EDKII_DIR = "$(CURDIR)/tools/edk2"
EDK2_INCLUDE_ROOT := $(EDKII_DIR)/MdePkg/Include
UEFI_APP_PATH := "$(BUILD_DIR)/boot/BOOTX64.EFI"
BIOS_BIN_PATH := "$(BUILD_DIR)/boot/bootldr.bin"

.PHONY: all clean

all:
	@python3 "$(CURDIR)/scripts/confdir.py"
	@mkdir -p "$(BUILD_DIR)"
	@$(MAKE) -C "$(CURDIR)/boot" EDKII_DIR="$(EDKII_DIR)" EDK2_INCLUDE_ROOT="$(EDK2_INCLUDE_ROOT)"
	@$(MAKE) -C "$(CURDIR)/kernel" BUILD_DIR="$(BUILD_DIR)" EDKII_DIR="$(EDKII_DIR)" EDK2_INCLUDE_ROOT="$(EDK2_INCLUDE_ROOT)"
	@mkdir -p "$(BUILD_DIR)/iso/boot"
	@mkdir -p "$(BUILD_DIR)/iso/EFI/BOOT"
	@cp -f "$(BIOS_BIN_PATH)" "$(BUILD_DIR)/iso/boot/bootldr.bin"
	@cp -f "$(BUILD_DIR)/kernel/oskrnl" "$(BUILD_DIR)/iso/oskrnl"
	@cp -f "$(UEFI_APP_PATH)" "$(BUILD_DIR)/iso/EFI/BOOT/BOOTX64.EFI"
	@rm -f "$(BUILD_DIR)/iso/EFI/BOOT/efiboot.img"
	@mkfs.fat -F 12 -C "$(BUILD_DIR)/iso/EFI/BOOT/efiboot.img" 4096 >/dev/null
	@mmd -i "$(BUILD_DIR)/iso/EFI/BOOT/efiboot.img" ::EFI >/dev/null
	@mmd -i "$(BUILD_DIR)/iso/EFI/BOOT/efiboot.img" ::EFI/BOOT >/dev/null
	@mcopy -i "$(BUILD_DIR)/iso/EFI/BOOT/efiboot.img" "$(UEFI_APP_PATH)" ::EFI/BOOT/BOOTX64.EFI >/dev/null
	@mcopy -i "$(BUILD_DIR)/iso/EFI/BOOT/efiboot.img" "$(BUILD_DIR)/kernel/oskrnl" ::oskrnl >/dev/null
	@xorriso -as mkisofs \
		-iso-level 3 \
		-full-iso9660-filenames \
		-R -J -l \
		-V "OS" \
		-b boot/bootldr.bin \
		-no-emul-boot \
		-boot-load-size 1024 \
		-eltorito-alt-boot \
		-e EFI/BOOT/efiboot.img \
		-no-emul-boot \
		-boot-load-size 2880 \
		-c boot.cat \
		-o "$(BUILD_DIR)/output.iso" \
		"$(BUILD_DIR)/iso"

clean:
	@rm -rf "$(BUILD_DIR)"

export CC LD AS DIR BUILD_DIR TARCH EDKII_DIR EDK2_INCLUDE_ROOT UEFI_APP_PATH BIOS_BIN_PATH