#!/usr/bin/env python3
"""
The GameTank's 256-colour palette, read from the SDK's own sprite sheet (sdk/assets/sdk_default/bios8.bmp has it in its header),
and a nearest-colour lookup. A colour on the GameTank is just a number, 0-255; this turns "the orange I want" into that number.

  python3 tools/palette.py 237,178,98 100,60,30     prints the nearest palette number for each RGB colour
  python3 tools/palette.py --grid                   prints the whole palette as a 32x8 chart of numbers
"""
import os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SDK = os.environ.get('SDK_DIR') or os.path.join(HERE, '..', 'sdk')


def load_palette():
    d = open(os.path.join(SDK, 'assets', 'sdk_default', 'bios8.bmp'), 'rb').read()
    off = 14 + struct.unpack('<I', d[14:18])[0]            # the palette follows the two headers
    ncol = struct.unpack('<I', d[46:50])[0] or 256
    return [(d[off + 4 * i + 2], d[off + 4 * i + 1], d[off + 4 * i]) for i in range(ncol)]


PAL = load_palette()


def nearest(rgb):
    best, bd = 0, 1 << 30
    for i, c in enumerate(PAL):
        d = sum((c[k] - rgb[k]) ** 2 for k in range(3))
        if d < bd:
            best, bd = i, d
    return best


if __name__ == '__main__':
    if '--grid' in sys.argv:
        for row in range(8):
            print(' '.join('%3d' % (row * 32 + col) for col in range(32)))
    else:
        for a in sys.argv[1:]:
            rgb = tuple(int(x) for x in a.split(','))
            n = nearest(rgb)
            print('%s -> %d  (%s)' % (a, n, ','.join(str(x) for x in PAL[n])))
