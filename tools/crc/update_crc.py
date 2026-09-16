#!/usr/bin/env python3
"""
update_crc.py: Calculates CRC32 of the firmware image, patches the trailing 4 bytes of the .bin file,
in-place patches the 4 bytes of the .crc32 section in .elf.
"""

import sys
import struct
from pathlib import Path

def stm32f103_crc32(data: bytes) -> int:
    """
    Simulates the STM32F103 hardware CRC peripheral:
    - Polynomial: 0x04C11DB7
    - Initial Value: 0xFFFFFFFF
    - Input: 32-bit words, Little-Endian
    - Final XOR: None
    """
    assert len(data) % 4 == 0, f"Payload size ({len(data)} bytes) must be a multiple of 4!"

    crc = 0xFFFFFFFF
    POLY = 0x04C11DB7

    for i in range(0, len(data), 4):
        word = struct.unpack('<I', data[i:i+4])[0]
        crc ^= word
        for _ in range(32):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ POLY) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF
    return crc

def patch_elf_crc(elf_path: Path, crc_val: int):
    """Finds section .crc32 offset in ELF file and patches 4 bytes in-place."""
    with open(elf_path, 'r+b') as f:
        data = f.read()

        if data[:4] != b'\x7fELF':
            raise ValueError(f"{elf_path} is not a valid ELF file")

        # 32-bit little-endian ELF header parsing
        e_shoff = struct.unpack('<I', data[32:36])[0]      # Section header table offset
        e_shentsize = struct.unpack('<H', data[46:48])[0]  # Section header entry size
        e_shnum = struct.unpack('<H', data[48:50])[0]      # Number of section headers
        e_shstrndx = struct.unpack('<H', data[50:52])[0]   # String table index

        # Read section header string table offset
        shstr_hdr = e_shoff + e_shstrndx * e_shentsize
        shstr_offset = struct.unpack('<I', data[shstr_hdr + 16 : shstr_hdr + 20])[0]

        crc_offset = None
        for i in range(e_shnum):
            entry = e_shoff + i * e_shentsize
            name_idx, _, _, _, sec_offset, sec_size = struct.unpack('<IIIIII', data[entry : entry + 24])

            # Read section name from string table
            name_end = data.find(b'\x00', shstr_offset + name_idx)
            name = data[shstr_offset + name_idx : name_end].decode('ascii', errors='ignore')

            if name == '.crc32':
                if sec_size != 4:
                    raise ValueError(f"Section .crc32 size is {sec_size}, expected 4 bytes!")
                crc_offset = sec_offset
                break

        if crc_offset is None:
            raise RuntimeError("Section '.crc32' not found in ELF! Check linker script.")

        # Write the 4-byte CRC directly into the ELF file
        f.seek(crc_offset)
        f.write(struct.pack('<I', crc_val))
        print(f"[CRC] Successfully patched .crc32 in {elf_path.name} at file offset 0x{crc_offset:X}")

def main():
    if len(sys.argv) < 3:
        print("Usage: patch_crc.py <firmware.bin> <firmware.elf>")
        sys.exit(1)

    bin_path = Path(sys.argv[1])
    elf_path = Path(sys.argv[2])

    if not bin_path.is_file():
        print(f"Error: {bin_path} not found!")
        sys.exit(1)

    if not elf_path.is_file():
        print(f"Error: {elf_path} not found!")
        sys.exit(1)

    # Patch .bin
    with open(bin_path, 'r+b') as f:
        data = f.read()
        payload = data[:-4]
        crc_val = stm32f103_crc32(payload)

        f.seek(-4, 2)
        f.write(struct.pack('<I', crc_val))

    print(f"[CRC] Image payload: {len(payload)} bytes")
    print(f"[CRC] Computed CRC32: 0x{crc_val:08X}")
    print(f"[CRC] Successfully patched {bin_path.name}")

    # Patch .elf
    patch_elf_crc(elf_path, crc_val)

if __name__ == '__main__':
    main()
