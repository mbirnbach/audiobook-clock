#!/usr/bin/env python3
"""Set header fields mkbootimg won't write for an empty second stage.

usage: fix-bootimg-header.py <boot.img> [second_addr]
The stock X04G boot image carries second_addr=0x40f00000 even though it has
no second stage; newer mkbootimg writes 0 in that case.
"""
import struct
import sys

path = sys.argv[1]
second_addr = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x40F00000
with open(path, "r+b") as f:
    f.seek(28)  # magic(8) + kernel_size, kernel_addr, ramdisk_size, ramdisk_addr, second_size
    f.write(struct.pack("<I", second_addr))
