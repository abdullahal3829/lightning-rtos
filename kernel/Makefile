AS ?=as
CC ?= gcc
LD ?= ld
BUILD_DIR ?= $(abspath ../build)
EDKII_DIR ?= $(abspath ../tools/edk2)
EDK2_INCLUDE_ROOT := $(EDKII_DIR)/MdePkg/Include
KERNEL_ELF=$(BUILD_DIR)/kernel/oskrnl

CFLAGS += -m64 -std=gnu11 -ffreestanding -fno-pie -fno-pic -fno-stack-protector \
	-fno-builtin -fno-asynchronous-unwind-tables -fno-unwind-tables -mno-red-zone \
	-mcmodel=small -Wall -Wextra -fshort-wchar -malign-double \
	-I"$(EDK2_INCLUDE_ROOT)" -I"$(EDK2_INCLUDE_ROOT)/X64" -I"$(EDK2_INCLUDE_ROOT)/Ia32" \
	-I"$(abspath ../include)" -DEFI_BUILD
CFLAGS += $(EXTRA_CFLAGS)
ASFLAGS = --64 \
		  -I"$(EDK2_INCLUDE_ROOT)" -I"$(EDK2_INCLUDE_ROOT)/X64" -I"$(EDK2_INCLUDE_ROOT)/Ia32"
LDFLAGS=-m elf_x86_64 -nostdlib -no-pie -T kernel.ld

OBJ := main.o
include asm/Makefile
include cpu/Makefile
include drivers/Makefile
include mm/Makefile

.PHONY: all
all: $(KERNEL_ELF)

$(KERNEL_ELF): $(OBJ) kernel.ld
	@mkdir -p "$(@D)"
	$(LD) $(LDFLAGS) -o $@ $(addprefix $(BUILD_DIR)/kernel/,$(OBJ))

%.o: %.c
	@mkdir -p "$(@D)"
	@$(CC) $(CFLAGS) -c -o $(BUILD_DIR)/kernel/$@ $<

%.a: %.s
	@mkdir -p "$(@D)"
	@$(AS) $(ASFLAGS) -c -o $(BUILD_DIR)/kernel/$@ $<
