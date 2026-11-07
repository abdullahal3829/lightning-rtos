CC ?= gcc
LD ?= ld
BUILD_DIR ?= $(abspath ../build)
EDKII_DIR ?= $(abspath ../tools/edk2)
EDK2_INCLUDE_ROOT := $(EDKII_DIR)/MdePkg/Include
KERNEL_ELF=$(BUILD_DIR)/kernel/oskrnl

CFLAGS=-m64 -std=gnu11 -ffreestanding -fno-pie -fno-pic -fno-stack-protector \
	-fno-builtin -fno-asynchronous-unwind-tables -fno-unwind-tables -mno-red-zone \
	-mcmodel=small -Wall -Wextra -fshort-wchar -malign-double -I../include -I../lib/POSIX-UEFI/uefi \
	-I"$(EDK2_INCLUDE_ROOT)" -I"$(EDK2_INCLUDE_ROOT)/X64" -I"$(EDK2_INCLUDE_ROOT)/Ia32"
LDFLAGS=-m elf_x86_64 -nostdlib -no-pie -T kernel.ld

.PHONY: all clean

all: $(KERNEL_ELF)

$(KERNEL_ELF): main.c kernel.ld
	@mkdir -p "$(@D)"
	$(CC) $(CFLAGS) -c -o $(BUILD_DIR)/kernel/main.o main.c
	$(LD) $(LDFLAGS) -o $@ $(BUILD_DIR)/kernel/main.o

clean:
	rm -rf "$(BUILD_DIR)/kernel"