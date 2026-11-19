#
# UEFI + BIOS bootloader build.
# No GRUB, no Limine.
#

BOOT_ROOT := $(abspath $(CURDIR)/..)
EDKII_DIR ?= $(BOOT_ROOT)/tools/edk2
EDK2_INCLUDE_ROOT := $(EDKII_DIR)/MdePkg/Include
GENFW ?= $(BOOT_ROOT)/tools/edk2/BaseTools/Source/C/bin/GenFw
BUILD_DIR=$(BOOT_ROOT)/build
CC=gcc
LD=ld
AS=as

# ---------------------------------------------------------------------------
# Common include paths
# ---------------------------------------------------------------------------
INCLUDES := -I"$(BOOT_ROOT)/include" \
            -I"$(EDK2_INCLUDE_ROOT)" \
            -I"$(EDK2_INCLUDE_ROOT)/X64" \
            -I"$(EDK2_INCLUDE_ROOT)/Ia32" \
            -I"$(BOOT_ROOT)/lib/lvgl/include/lvgl" \
            -I"$(BOOT_ROOT)/lib/lvgl" \
            -I"$(BOOT_ROOT)/boot" \
            -I"$(BOOT_ROOT)/lib/elfParse"

# ---------------------------------------------------------------------------
# UEFI build (x86-64)
# ---------------------------------------------------------------------------
UEFI_CFLAGS := -m64 -DUEFI_BUILD -DEFI_BUILD \
    -DEFIAPI='__attribute__((ms_abi))' \
    -mno-red-zone -fcf-protection=none -fshort-wchar \
    -ffreestanding -fno-pie -fno-builtin -fno-stack-protector \
    $(INCLUDES)

UEFI_LDFLAGS := -m elf_x86_64 -T loader_efi.ld -e efi_main \
    -no-pie -z noreloc

UEFI_C_SRCS := uefi_main.c
UEFI_OBJ := $(patsubst %.c,$(BUILD_DIR)/boot/uefi_%.o,$(UEFI_C_SRCS))

UEFI_ELF := $(BUILD_DIR)/boot/loader_efi.elf
UEFI_EFI := $(BUILD_DIR)/boot/BOOTX64.EFI

# ---------------------------------------------------------------------------
# BIOS build (i386)
# ---------------------------------------------------------------------------
BIOS_CFLAGS := -m32 -DBOOTLDR_BUILD \
    -ffreestanding -fno-pie -fno-builtin -fno-stack-protector \
    $(INCLUDES)

BIOS_ASFLAGS := --32

BIOS_C_SRCS := $(CURDIR)/loader.c $(CURDIR)/mm.c $(CURDIR)/vesa.c \
               $(CURDIR)/drivers/disk.c $(CURDIR)/drivers/keyboard.c $(CURDIR)/drivers/mouse.c \
               $(CURDIR)/fs/toritofs.c $(CURDIR)/kloader/kloader.c \
               $(BOOT_ROOT)/lib/elfParse/elfparse.c

BIOS_ASM_SRCS := $(CURDIR)/loader.s $(CURDIR)/drivers/x86/disk.s $(CURDIR)/kloader/khal_x86.s

# LVGL sources for BIOS build (compiled as 32-bit)
LVGL_SRC_DIR := $(BOOT_ROOT)/lib/lvgl/src
BIOS_LVGL_SRCS := $(shell find $(LVGL_SRC_DIR) -name '*.c' 2>/dev/null)

# Object names: replace / with __ to avoid directory issues in flat object dir
BIOS_C_OBJ   := $(foreach src,$(BIOS_C_SRCS) $(BIOS_LVGL_SRCS),$(BUILD_DIR)/boot/bios-c_$(subst /,__,$(src:.c=.o)))
BIOS_ASM_OBJ := $(foreach src,$(BIOS_ASM_SRCS),$(BUILD_DIR)/boot/bios-asm_$(subst /,__,$(src:.s=.o)))

BIOS_ELF := $(BUILD_DIR)/boot/bios_loader.elf
BIOS_BIN := $(BUILD_DIR)/boot/bootldr.bin

# Boot sector (built separately, 512 bytes)
BOOTSEC_OBJ := $(BUILD_DIR)/boot/bootsec.o
BOOTSEC_ELF := $(BUILD_DIR)/boot/bootsec.elf
BOOTSEC_BIN := $(BUILD_DIR)/boot/boot.bin

# Loader (without boot sector)
LOADER_BIN := $(BUILD_DIR)/boot/loader.bin

.PHONY: build clean

build: $(UEFI_EFI) $(BIOS_BIN)

# ===========================================================================
# UEFI build rules
# ===========================================================================

$(BUILD_DIR)/boot/uefi_%.o: %.c
	@mkdir -p "$(@D)"
	@$(CC) $(UEFI_CFLAGS) -c -o $@ $<

$(UEFI_ELF): $(UEFI_OBJ) loader_efi.ld
	@mkdir -p "$(@D)"
	@$(LD) $(UEFI_LDFLAGS) -o $@ $(UEFI_OBJ) \
	    $(shell $(CC) -m64 -print-libgcc-file-name)

$(UEFI_EFI): $(UEFI_ELF)
	@$(GENFW) -e UEFI_APPLICATION -o $@ $<

# ===========================================================================
# BIOS build rules
# ===========================================================================

# Boot sector: assemble and link at 0x7C00, then strip leading VMA zeros
$(BOOTSEC_OBJ): $(CURDIR)/boot.s
	@mkdir -p "$(@D)"
	@$(AS) $(BIOS_ASFLAGS) -o $@ $(CURDIR)/boot.s

$(BOOTSEC_ELF): $(BOOTSEC_OBJ)
	@mkdir -p "$(@D)"
	@$(LD) -m elf_i386 -Ttext 0x7C00 -o $@ $<

$(BOOTSEC_BIN): $(BOOTSEC_ELF)
	@objcopy -O binary --adjust-vma=-0x7C00 $< $@

# Generic C compile rule for BIOS (flat object names)
define BIOSS_C_RULE
$(BUILD_DIR)/boot/bios-c_$(subst /,__,$(1:.c=.o)): $(1)
	@mkdir -p "$$(@D)"
	@$$(CC) $$(BIOS_CFLAGS) -c -o $$@ $$<
endef
$(foreach src,$(BIOS_C_SRCS) $(BIOS_LVGL_SRCS),$(eval $(call BIOSS_C_RULE,$(src))))

# Generic asm compile rule for BIOS
define BIOSS_ASM_RULE
$(BUILD_DIR)/boot/bios-asm_$(subst /,__,$(1:.s=.o)): $(1)
	@mkdir -p "$$(@D)"
	@$$(AS) $$(BIOS_ASFLAGS) -o $$@ $$<
endef
$(foreach src,$(BIOS_ASM_SRCS),$(eval $(call BIOSS_ASM_RULE,$(src))))

# Link loader (entry at 0x7E00 per loader.ld)
# ASM objects first so 16-bit boot code stays below 64 KiB
$(BIOS_ELF): $(BIOS_C_OBJ) $(BIOS_ASM_OBJ) loader.ld
	@mkdir -p "$(@D)"
	@$(LD) -m elf_i386 -T loader.ld -e _start -no-pie --gc-sections -o $@ $(BIOS_ASM_OBJ) $(BIOS_C_OBJ) \
	    $(shell $(CC) -m32 -print-libgcc-file-name)

# Convert loader to flat binary
$(LOADER_BIN): $(BIOS_ELF)
	@objcopy -O binary $< $@

# Combine boot sector + loader into final boot image
$(BIOS_BIN): $(BOOTSEC_BIN) $(LOADER_BIN)
	@cat $(BOOTSEC_BIN) $(LOADER_BIN) > $@

clean:
	@rm -rf "$(BUILD_DIR)/boot"

export CC LD AS BOOT_ROOT EDKII_DIR EDK2_INCLUDE_ROOT