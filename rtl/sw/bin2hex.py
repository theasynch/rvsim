#!/usr/bin/env python3
"""
bin2hex.py — Convert binary file to Verilog $readmemh format.

Usage: python3 bin2hex.py input.bin output.hex

Each line of the output hex file contains one 32-bit word in hexadecimal.
This is the format expected by $readmemh() in imem.sv.
"""

import sys
import struct

def bin2hex(input_path, output_path):
    with open(input_path, 'rb') as f:
        data = f.read()

    # Pad to word boundary
    while len(data) % 4 != 0:
        data += b'\x00'

    with open(output_path, 'w') as f:
        for i in range(0, len(data), 4):
            word = struct.unpack('<I', data[i:i+4])[0]  # Little-endian
            f.write(f'{word:08X}\n')

    word_count = len(data) // 4
    print(f'  bin2hex: {input_path} → {output_path} ({word_count} words)')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f'Usage: {sys.argv[0]} input.bin output.hex')
        sys.exit(1)
    bin2hex(sys.argv[1], sys.argv[2])
