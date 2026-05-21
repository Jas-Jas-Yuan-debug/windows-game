#!/usr/bin/env python3
"""Generate a 1024x1024 placeholder PNG for AppIcon using only Python stdlib.
Replace mac/icon-source.png with your own art and re-run make-icon.sh."""
import sys, struct, zlib, math
W = H = 1024
def lerp(a, b, t): return a + (b - a) * t
def shade(x, y):
    # diagonal gradient: deep blue → red, with a soft inner highlight
    t = (x + y) / (W + H - 2)
    r = int(lerp(28,  220, t))
    g = int(lerp(36,   72, t))
    b = int(lerp(86,   72, t))
    # inner circular highlight
    cx, cy, rad = W * 0.35, H * 0.30, W * 0.45
    d = math.hypot(x - cx, y - cy) / rad
    hl = max(0.0, 1.0 - d) * 0.35
    r = min(255, int(r + hl * 220))
    g = min(255, int(g + hl * 200))
    b = min(255, int(b + hl * 180))
    return (r, g, b, 255)
def stamp_cg(buf):
    # crude 'CG' glyph drawn as filled rectangles. Replace with real art for shipping.
    fg = (245, 240, 230, 255)
    def fill(x0, y0, x1, y1):
        for y in range(max(0,y0), min(H,y1)):
            for x in range(max(0,x0), min(W,x1)):
                i = (y * W + x) * 4
                buf[i:i+4] = bytes(fg)
    # C
    fill(220, 360, 280, 720)
    fill(280, 360, 460, 420)
    fill(280, 660, 460, 720)
    # G
    fill(560, 360, 620, 720)
    fill(620, 360, 800, 420)
    fill(620, 660, 800, 720)
    fill(700, 520, 800, 580)
    fill(740, 520, 800, 660)
def make_png():
    raw = bytearray(W * H * 4)
    for y in range(H):
        row_off = y * W * 4
        for x in range(W):
            r, g, b, a = shade(x, y)
            i = row_off + x * 4
            raw[i] = r; raw[i+1] = g; raw[i+2] = b; raw[i+3] = a
    stamp_cg(raw)
    # Pack rows with filter byte 0
    scanlines = bytearray()
    for y in range(H):
        scanlines.append(0)
        off = y * W * 4
        scanlines.extend(raw[off:off + W * 4])
    def chunk(typ, data):
        return (struct.pack('>I', len(data)) + typ + data +
                struct.pack('>I', zlib.crc32(typ + data) & 0xFFFFFFFF))
    sig = b'\x89PNG\r\n\x1a\n'
    ihdr = struct.pack('>IIBBBBB', W, H, 8, 6, 0, 0, 0)
    idat = zlib.compress(bytes(scanlines), 9)
    return sig + chunk(b'IHDR', ihdr) + chunk(b'IDAT', idat) + chunk(b'IEND', b'')
out = sys.argv[1] if len(sys.argv) > 1 else 'mac/icon-source.png'
with open(out, 'wb') as f:
    f.write(make_png())
print(f"wrote {out} ({W}x{H})")
