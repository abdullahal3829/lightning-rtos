# Part Public License (PPL)
# Copyright (C) 2026 Abdullah Al Nahian/Raiyan
# View LICENSE.md

.global pit_delay_iterations
.text
# void pit_delay_iterations(uint32_t count)
#   edi = count
pit_delay_iterations:
    xorl    %ecx, %ecx          # i = 0
.Lloop_start:
    cmpl    %ecx, %edi          # compare i (ecx) with count (edi)
    jae     .Lloop_end          # if i >= count, exit loop
    nop                         # the "nop" inside the body
    incl    %ecx                # ++i
    jmp     .Lloop_start        # repeat loop
.Lloop_end:
    ret                         # return to caller
