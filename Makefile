CC=gcc
LD=ld
DIR="$(CURDIR)"
AS=as
BUILD_DIR="$(CURDIR)/build"

all:
	@python3 "$(CURDIR)/scripts/confdir.py"
	@$(MAKE) -C "$(CURDIR)/boot"
	@mkdir -p "$(BUILD_DIR)/iso/boot"
	@cp -f "$(BUILD_DIR)/boot/image.img" "$(BUILD_DIR)/iso/boot/bootldr.bin"
	@truncate -s 1474560 "$(BUILD_DIR)/iso/boot/bootldr.bin"
	@xorriso -as mkisofs \
		-iso-level 3 \
		-full-iso9660-filenames \
		-R -J -l \
		-V "OS" \
		-b boot/bootldr.bin \
		-c boot.cat \
		-o "$(BUILD_DIR)/output.iso" \
		"$(BUILD_DIR)/iso"

clean:
	@rm -rf build

export CC
export LD
export AS
export DIR
export BUILD_DIR

.PHONY: all clean
