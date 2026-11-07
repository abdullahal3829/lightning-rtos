/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#ifndef KHAL_H
#define KHAL_H

#include <protocol.h>

extern unsigned int handleSyscall(SyscallCode syscallCode, unsigned long args,
                                  unsigned long argsSize, unsigned long rBuffer,
                                  unsigned long rBufferSize);

extern unsigned int handleSyscall64(SyscallCode syscallCode, unsigned long long args,
                                    unsigned long long argsSize, unsigned long long rBuffer,
                                    unsigned long long rBufferSize);

#endif // KHAL_H