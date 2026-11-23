#ifndef GPIO_UTIL
#define GPIO_UTIL

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
extern uint8_t inb(uint16_t port);
extern void outb(uint16_t port, uint8_t value);
#else
#error "Kernel Doesn't Support Compilation For This Architecture"
#endif

extern void die();

#endif