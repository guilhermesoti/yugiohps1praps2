#!/usr/bin/env python3
"""Converte host:shot.bin (u16 largura, u16 altura, pixels PSMCT16) em PNG."""
import struct, sys, zlib

src, dst = sys.argv[1], sys.argv[2]
data = open(src, "rb").read()
w, h = struct.unpack_from("<HH", data, 0)
pix = struct.unpack_from("<%dH" % (w * h), data, 4)
rows = []
for y in range(h):
    row = bytearray([0])
    for x in range(w):
        p = pix[y * w + x]
        r, g, b = p & 31, (p >> 5) & 31, (p >> 10) & 31
        row += bytes(((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2)))
    rows.append(bytes(row))
# 2x para enxergar melhor (e 16:9 mostrado com proporção certa só como referência)
def chunk(t, d):
    c = struct.pack(">I", len(d)) + t + d
    return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")
open(dst, "wb").write(png)
print(w, h)
