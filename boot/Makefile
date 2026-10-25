CFLAGS=-m32 -I"$(DIR)/include" -ffreestanding -fno-pie -c -o
ASFLAGS=--32
BOOT_LDFLAGS=-m elf_i386 -Ttext 0x7C00 -e _start -no-pie
LOADER_LDFLAGS=-m elf_i386 -T loader.ld -e _start -no-pie
LOADER_BIN=$(BUILD_DIR)/boot/loader.bin

build:
	@mkdir -p "$(BUILD_DIR)/boot"
	@$(AS) $(ASFLAGS) -o $(BUILD_DIR)/boot/loader_asm.o loader.s
	@$(CC) $(CFLAGS) $(BUILD_DIR)/boot/loader_c.o loader.c
	@$(CC) $(CFLAGS) $(BUILD_DIR)/boot/vesa.o vesa.c
	@$(LD) $(LOADER_LDFLAGS) -o $(BUILD_DIR)/boot/loader.elf \
		   $(BUILD_DIR)/boot/loader_asm.o $(BUILD_DIR)/boot/loader_c.o \
		   $(BUILD_DIR)/boot/vesa.o
	@objcopy -O binary $(BUILD_DIR)/boot/loader.elf $(LOADER_BIN)
	$(AS) $(ASFLAGS) -o $(BUILD_DIR)/boot/boot.o boot.s
	@$(LD) $(BOOT_LDFLAGS) -o $(BUILD_DIR)/boot/boot.elf $(BUILD_DIR)/boot/boot.o
	@objcopy -O binary $(BUILD_DIR)/boot/boot.elf $(BUILD_DIR)/boot/boot.bin
	
	@$(AS) $(ASFLAGS) -I"$(BUILD_DIR)/boot" -o $(BUILD_DIR)/boot/image.o image.s
	@objcopy -O binary $(BUILD_DIR)/boot/image.o $(BUILD_DIR)/boot/image.img

# Run with QEMU using -hda for the image and -nographic for serial console output
run:
	@qemu-system-i386 -hda /home/first/Documents/osdev/build/boot/image.img -nographic -serial mon:stdio

# Run with QEMU with graphical output and serial to stdio
run-gui:
	@qemu-system-i386 -hda /home/first/Documents/osdev/build/boot/image.img -serial mon:stdio

# Run with QEMU and GDB debugging
debug:
	@qemu-system-i386 -hda /home/first/Documents/osdev/build/boot/image.img -nographic -serial mon:stdio -s -S