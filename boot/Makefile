CFLAGS=-m32 -ffreestanding -fno-pie -c -o
ASFLAGS=--32
BOOT_LDFLAGS=-m elf_i386 -Ttext 0x7C00 -e _start -no-pie
LOADER_LDFLAGS=-m elf_i386 -Ttext 0x8000 -e _start -no-pie
LOADER_BIN=$(BUILD_DIR)/boot/loader.bin

build:
	@$(AS) $(ASFLAGS) -o $(BUILD_DIR)/boot/loader.o loader.s
	@$(CC) $(CFLAGS) $(BUILD_DIR)/boot/loader.a loader.c
	@$(LD) $(LOADER_LDFLAGS) -o $(BUILD_DIR)/boot/loader.elf \
		   $(BUILD_DIR)/boot/loader.o $(BUILD_DIR)/boot/loader.a
	@objcopy -O binary $(BUILD_DIR)/boot/loader.elf $(LOADER_BIN)
	@LOADER_SECTORS=$$(expr \( $$(stat -c%s "$(LOADER_BIN)" 2>/dev/null || echo 0) + 511 \) / 512 ); \
	$(AS) $(ASFLAGS) --defsym LOADER_SECTORS=$$LOADER_SECTORS -o $(BUILD_DIR)/boot/boot.o boot.s
	@$(LD) $(BOOT_LDFLAGS) -o $(BUILD_DIR)/boot/boot.elf $(BUILD_DIR)/boot/boot.o
	@objcopy -O binary $(BUILD_DIR)/boot/boot.elf $(BUILD_DIR)/boot/boot.bin
	
	@$(AS) $(ASFLAGS) -I"$(BUILD_DIR)/boot" -o $(BUILD_DIR)/boot/image.o image.s
	@objcopy -O binary $(BUILD_DIR)/boot/image.o $(BUILD_DIR)/boot/image.img