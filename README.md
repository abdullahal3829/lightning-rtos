# Lightning RTOS
A Fully Fine-gained Real Time Operating System.

## Requirements
- `gcc 14`: To compile the EDK2 project (UEFI support will be added)
- `python3`: For automation
- `cmake`: For LVGL compilation (for bootloader interface and high-level GUI rendering instead of low-level GUI rendering)
- `websockify`: For testing in noVNC (in case you develop this OS from a remote server)
- `xorriso`: For ISO creation
- `util-linux`/`dosfstools`: Required For Create The FAT Filesystem

## Features
- Basic bootloader
- ~~UEFI support~~
- ~~CSS styling support on bootloader~~
- ~~Basic kernel~~
- ~~Fine-gained operation~~
- ~~Ring privilege support~~
- ~~Dynamic (external) executable/driver parsing and execution support~~
- ~~JIT compiler~~
- ~~Executable embedding support~~
- ~~Physical disk filesystem support~~
- ~~Standard CLI utilities~~
- ~~GUI and CSS theming support~~
- ~~Standard desktop environment and GUI applications~~

## Contribute

```bash
git submodule update --init --recursive
make -C tools/edk2/BaseTools    # Required To Compile The UEFI Base Tools
make -j$(nproc)
qemu-system-x86_64 -cdrom ./build/output.iso -m 128M
# Ignore Above Commands Of Emulation (Not Building Commands) In Case If You Develop This OS From Remote Server. And Execute Command (Located On Below)

# websockify --web=/usr/share/novnc/ 6080 127.0.0.1:5901 # Start In Another Terminal In Case noVNC Server Started

# qemu-system-x86_64 -m 2048 -cdrom build/output.iso -m 128M -vnc 127.0.0.1:1 -nographic
```