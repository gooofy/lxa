#!/usr/bin/env python3
"""Finish the lxa ROM image: pad it to its size and store the size in the
longword at offset size-20 ($FFFFEC for a 512K ROM at $F80000), where
programs (SysInfo) read the Kickstart size, as on AmigaOS 3.1 (probe
exec/machine on the reference, Phase 240)."""
import struct
import sys


def main():
    src, dst, size = sys.argv[1], sys.argv[2], int(sys.argv[3])
    data = bytearray(open(src, 'rb').read())
    if len(data) > size - 20:
        sys.exit('romfinish: ROM code (%d bytes) overlaps the ROM trailer' % len(data))
    data.extend(b'\0' * (size - len(data)))
    data[size - 20:size - 16] = struct.pack('>I', size)
    open(dst, 'wb').write(data)


if __name__ == '__main__':
    main()
