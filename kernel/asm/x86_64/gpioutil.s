.global outb
.global inb
.text
# -----------------------------------------------------------------------------
# void outb(uint16_t port, uint8_t value)
#
# Arguments (System V AMD64 ABI):
#   rdi/di = port (16-bit)
#   rsi/sil = value (8-bit)
# -----------------------------------------------------------------------------
outb:
    movw    %di, %dx        # Move port to DX (outb requires DX for variable ports)
    movb    %sil, %al       # Move value to AL
    outb    %al, (%dx)      # Output byte in AL to I/O port in DX
    ret

# -----------------------------------------------------------------------------
# uint8_t inb(uint16_t port)
#
# Arguments (System V AMD64 ABI):
#   rdi/di = port (16-bit)
# Return value:
#   al = value (8-bit)
# -----------------------------------------------------------------------------
inb:
    movw    %di, %dx        # Move port to DX (inb requires DX for variable ports)
    inb     (%dx), %al      # Input byte from I/O port in DX into AL
    ret