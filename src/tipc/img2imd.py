#!/usr/bin/env python3
"""img2imd.py RAW.img OUT.imd [interleave] [cyls] [spt]: ImageDisk copy of a raw 512-byte-sector image
(side 0/1 alternate per cylinder, 2 heads) with sector interleave, so the 5 MHz TIPC can read
consecutive sectors without losing a revolution each."""
import sys
raw = open(sys.argv[1], 'rb').read()
out = sys.argv[2]
il = int(sys.argv[3]) if len(sys.argv) > 3 else 3
spt = int(sys.argv[5]) if len(sys.argv) > 5 else 9
cyls = int(sys.argv[4]) if len(sys.argv) > 4 else len(raw) // (512 * spt * 2)
def order(n, il):
    o = [0] * n; pos = 0
    for s in range(1, n + 1):
        while o[pos]: pos = (pos + 1) % n
        o[pos] = s; pos = (pos + il) % n
    return o
o = order(spt, il)
d = bytearray(b'IMD 1.18: 01/01/2026 00:00:00\r\nMicropolis for TIPC disk, interleave %d\r\n\x1a' % il)
for c in range(cyls):
    for h in range(2):
        d += bytes([5, c, h, spt, 2]) + bytes(o)             # mode 5 = 250 kbps MFM, 512-byte sectors
        for s in o:
            lba = (c * 2 + h) * spt + (s - 1)
            d += b'\x01' + raw[lba * 512:(lba + 1) * 512].ljust(512, b'\0')
open(out, 'wb').write(d)
