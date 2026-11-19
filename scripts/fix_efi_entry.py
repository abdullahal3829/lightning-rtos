#!/usr/bin/env python3
"""Post-process GenFw PE/COFF image to put a jump stub at offset 0.

OVMF with El Torito no-emulation boot jumps to offset 0 of the loaded image
instead of respecting AddressOfEntryPoint. GenFw places the PE header at
offset 0, which is not executable code. This script overwrites the first
bytes with a near jump to the PE entry point.
"""

import struct
import sys

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.efi> <output.efi>", file=sys.stderr)
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    with open(input_path, "rb") as f:
        data = bytearray(f.read())

    # Find PE signature
    pe_off = data.find(b"PE\x00\x00")
    if pe_off < 0:
        print("ERROR: No PE signature found", file=sys.stderr)
        sys.exit(1)

    # Read AddressOfEntryPoint from optional header
    # Optional header starts at pe_off + 24 (after 4-byte signature + 20-byte COFF header)
    opt_hdr_off = pe_off + 24
    magic = struct.unpack_from("<H", data, opt_hdr_off)[0]

    if magic == 0x20B:  # PE32+
        # AddressOfEntryPoint is at offset 16 within optional header
        entry_point = struct.unpack_from("<I", data, opt_hdr_off + 16)[0]
    elif magic == 0x10B:  # PE32
        entry_point = struct.unpack_from("<I", data, opt_hdr_off + 16)[0]
    else:
        print(f"ERROR: Unknown PE magic 0x{magic:04X}", file=sys.stderr)
        sys.exit(1)

    # Write a near jump (E9 rel32) at offset 0
    # Jump from offset 5 to entry_point: rel32 = entry_point - 5
    rel32 = entry_point - 5
    data[0] = 0xE9  # JMP rel32
    struct.pack_into("<I", data, 1, rel32 & 0xFFFFFFFF)

    with open(output_path, "wb") as f:
        f.write(data)

    print(f"Patched: entry_point=0x{entry_point:X}, rel32=0x{rel32:08X}", file=sys.stderr)

if __name__ == "__main__":
    main()
