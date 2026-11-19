/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include <Uefi.h>
#include <stdint.h>
#include <Protocol/SimplePointer.h>
#include <Protocol/BlockIo.h>

extern EFI_SYSTEM_TABLE *gST;

EFI_GUID gEfiSimplePointerProtocolGuid = { 0x31878C87, 0x0B75, 0x11D5, { 0x9A, 0x4F, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D } };
EFI_GUID gEfiBlockIoProtocolGuid = { 0x964E5B21, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
EFI_GUID gEfiSimpleFileSystemProtocolGuid = { 0x964E5B22, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
EFI_GUID gEfiFileInfoGuid = { 0x09576E92, 0x6D3F, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };

volatile uint32_t mmap_count = 0;

uint32_t handleSyscall64(uint32_t syscallCode, uint64_t args, uint64_t argsSize, uint64_t rBuffer, uint64_t rBufferSize) {
    (void)syscallCode;
    (void)args;
    (void)argsSize;
    (void)rBuffer;
    (void)rBufferSize;
    return 0;
}

void die(void) {
    for (;;)
        __asm__ volatile ("cli; hlt");
}

void prepareLongMode(uint64_t entryPoint, uintptr_t stackTop, const void *bootProtocol) {
    /* We are already in x86-64 long mode under UEFI.  Switch to the kernel
     * stack, pass the boot-protocol pointer in RDI (SysV ABI first argument
     * for the large by-value protocol struct), align the stack, and call the
     * kernel entry point.  If the kernel ever returns, halt. */
    __asm__ volatile (
        "mov %0, %%rsp\n"
        "and $-16, %%rsp\n"
        "mov %1, %%rdi\n"
        "call *%2\n"
        : /* no outputs */
        : "r"((uint64_t)stackTop), "r"((uint64_t)bootProtocol), "r"(entryPoint)
        : "rsp", "rdi", "memory"
    );
    for (;;)
        __asm__ volatile ("cli; hlt");
}

uint32_t vesaGetWidth(void) {
    return 0;
}

uint32_t vesaGetHeight(void) {
    return 0;
}

static void uefiOutputChar(CHAR16 ch) {
    CHAR16 buf[2];
    buf[0] = ch;
    buf[1] = 0;
    if (gST != NULL && gST->ConOut != NULL)
        gST->ConOut->OutputString(gST->ConOut, buf);
}

EFI_STATUS EFIAPI Print(const CHAR16 *Format, ...) {
    VA_LIST args;
    VA_START(args, Format);
    while (*Format != 0) {
        if (*Format == '%') {
            ++Format;
            if (*Format == '%') {
                uefiOutputChar('%');
            } else if (*Format == 'a') {
                const char *text = VA_ARG(args, char *);
                while (*text != 0)
                    uefiOutputChar((CHAR16)*(text++));
            } else if (*Format == 's') {
                const CHAR16 *text = VA_ARG(args, CHAR16 *);
                while (*text != 0)
                    uefiOutputChar(*(text++));
            }
            if (*Format != 0)
                ++Format;
        } else {
            uefiOutputChar(*(Format++));
        }
    }
    VA_END(args);
    return EFI_SUCCESS;
}
