BOOT_ROOT=$(abspath $(CURDIR)/..)
BUILD_DIR=$(BOOT_ROOT)/build
CFLAGS=-m32 -I"$(BOOT_ROOT)/include" -ffreestanding -fno-pie -fno-builtin -ffunction-sections -fdata-sections
ASFLAGS=--32
BOOT_LDFLAGS=-m elf_i386 -Ttext 0x7C00 -e _start -no-pie
LOADER_LDFLAGS=-m elf_i386 -T loader.ld -e _start -no-pie --gc-sections
LOADER_BIN=$(BUILD_DIR)/boot/loader.bin
LIBGCC=$(shell $(CC) -m32 -print-libgcc-file-name)
LVGL_ROOT=$(abspath $(CURDIR)/../lib/lvgl)
LVGL_SRC=$(LVGL_ROOT)/src
LVGL_CFLAGS=$(CFLAGS) -DLV_CONF_INCLUDE_SIMPLE -I"$(CURDIR)" -I"$(LVGL_ROOT)" -I"$(LVGL_ROOT)/include"
LVGL_SRCS=$(shell find $(LVGL_SRC)/core $(LVGL_SRC)/misc $(LVGL_SRC)/stdlib/builtin $(LVGL_SRC)/tick $(LVGL_SRC)/widgets/label $(LVGL_SRC)/widgets/span $(LVGL_SRC)/draw/sw $(LVGL_SRC)/indev -type f -name '*.c') \
	$(shell find $(LVGL_SRC)/draw -maxdepth 1 -type f -name '*.c') \
	$(LVGL_SRC)/lv_init.c $(LVGL_SRC)/display/lv_display.c $(LVGL_SRC)/osal/lv_os.c $(LVGL_SRC)/osal/lv_os_none.c \
	$(LVGL_SRC)/stdlib/lv_mem.c $(LVGL_SRC)/font/lv_font.c $(LVGL_SRC)/font/fmt_txt/lv_font_fmt_txt.c \
	$(LVGL_SRC)/layouts/lv_layout.c $(LVGL_SRC)/layouts/flex/lv_flex.c \
	$(LVGL_SRC)/themes/lv_theme.c $(LVGL_SRC)/draw/convert/lv_draw_buf_convert.c \
	$(LVGL_SRC)/fs/lv_fs.c $(LVGL_SRC)/image/lv_image_decoder.c $(LVGL_SRC)/image/lv_bin_decoder.c \
	$(LVGL_SRC)/font/lv_font_montserrat_14.c $(LVGL_SRC)/font/lv_font_montserrat_20.c $(LVGL_SRC)/font/lv_font_montserrat_28.c
LVGL_OBJS=$(patsubst $(LVGL_SRC)/%.c,$(BUILD_DIR)/boot/lvgl/%.o,$(LVGL_SRCS))

build:
	@mkdir -p "$(BUILD_DIR)/boot"
	@$(AS) $(ASFLAGS) -o $(BUILD_DIR)/boot/loader_asm.o loader.s
	@$(CC) $(CFLAGS) -I"$(LVGL_ROOT)" -I"$(LVGL_ROOT)/include" -DLV_CONF_INCLUDE_SIMPLE -I"$(CURDIR)" -c -o $(BUILD_DIR)/boot/loader_c.o loader.c
	@$(CC) $(CFLAGS) -c -o $(BUILD_DIR)/boot/vesa.o vesa.c
	@$(CC) $(CFLAGS) -c -o $(BUILD_DIR)/boot/keyboard.o drivers/keyboard.c
	@$(CC) $(CFLAGS) -c -o $(BUILD_DIR)/boot/mouse.o drivers/mouse.c
	@$(MAKE) --no-print-directory $(LVGL_OBJS)
	@$(LD) $(LOADER_LDFLAGS) -o $(BUILD_DIR)/boot/loader.elf \
		   $(BUILD_DIR)/boot/loader_asm.o $(BUILD_DIR)/boot/loader_c.o \
		   $(BUILD_DIR)/boot/vesa.o $(BUILD_DIR)/boot/keyboard.o \
		   $(BUILD_DIR)/boot/mouse.o $(LVGL_OBJS) $(LIBGCC)
	@objcopy -O binary $(BUILD_DIR)/boot/loader.elf $(LOADER_BIN)
	$(AS) $(ASFLAGS) -o $(BUILD_DIR)/boot/boot.o boot.s
	@$(LD) $(BOOT_LDFLAGS) -o $(BUILD_DIR)/boot/boot.elf $(BUILD_DIR)/boot/boot.o
	@objcopy -O binary $(BUILD_DIR)/boot/boot.elf $(BUILD_DIR)/boot/boot.bin
	
	@$(AS) $(ASFLAGS) -I"$(BUILD_DIR)/boot" -o $(BUILD_DIR)/boot/image.o image.s
	@objcopy -O binary $(BUILD_DIR)/boot/image.o $(BUILD_DIR)/boot/image.img

$(BUILD_DIR)/boot/lvgl/%.o: $(LVGL_SRC)/%.c $(CURDIR)/lv_conf.h
	@mkdir -p "$(@D)"
	@$(CC) $(LVGL_CFLAGS) -c -o $@ $<

# Run with QEMU using -hda for the image and -nographic for serial console output
run:
	@qemu-system-i386 -hda /home/first/Documents/osdev/build/boot/image.img -nographic -serial mon:stdio

# Run with QEMU with graphical output and serial to stdio
run-gui:
	@qemu-system-i386 -hda /home/first/Documents/osdev/build/boot/image.img -serial mon:stdio

# Run with QEMU and GDB debugging
debug:
	@qemu-system-i386 -hda /home/first/Documents/osdev/build/boot/image.img -nographic -serial mon:stdio -s -S
