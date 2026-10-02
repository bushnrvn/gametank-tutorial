#!/usr/bin/env python3
"""
Draws the sprites the lessons use (Doug, a Vumpire, a gold bar) and writes them where the SDK's asset pipeline expects them:

  spr.bmp   a 128x128 8-bit picture: one "page" of GameTank sprite memory (the SDK turns every .bmp in assets/ into a sprite sheet)
  art.h     where each frame sits on the page, as C arrays, so the game can say "frame 3 of Doug" instead of "x=24, y=0"

  python3 art/make_art.py <folder>      writes <folder>/assets/spr/spr.bmp and <folder>/src/art.h
  ./art/install.sh                      does that for every lesson from 02 on

The pictures are ASCII art: one letter per pixel, "." is see-through. Each letter stands for a colour in the tables below, and every
colour is snapped to the nearest of the GameTank's 256 palette colours (colour 0 is reserved: it means "transparent").
"""
import os, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools'))
import palette

COLOURS = {
    'ink': (26, 22, 64),
    'cap': (60, 60, 130),
    'cap_d': (27, 15, 143),
    'brim': (200, 60, 40),
    'skin': (203, 183, 159),
    'jersey': (185, 185, 185),
    'pin': (94, 86, 172),
    'sock': (200, 60, 40),
    'uni': (132, 132, 152),
    'vskin': (172, 150, 200),
    'vred': (230, 40, 60),
    'white': (185, 185, 185),
}

def idx(name):
    """palette number for a named colour (never 0: that is transparent)"""
    best, bi = 1e9, 1
    r, g, b = COLOURS[name]
    for i, p in enumerate(palette.PAL):
        if i == 0:
            continue
        d = 0.30 * (p[0] - r) ** 2 + 0.59 * (p[1] - g) ** 2 + 0.11 * (p[2] - b) ** 2
        if d < best:
            best, bi = d, i
    return bi

# --- Doug: a ballplayer in a cap, facing right, down or up (two walking frames each). Facing left is the right frames mirrored.
DOUG_LEGEND = dict(k='ink', c='cap', C='cap_d', r='brim', s='skin', e='ink', w='jersey', p='pin', b='sock')
DOUG_R0 = [
    "..kkkk..",
    ".kccccr.",
    ".kCCCCrr",
    "..kssek.",
    "..ksssk.",
    ".kwpwpwk",
    ".kwwwwk.",
    "..kb.bk.",
]
DOUG_R1 = [
    "..kkkk..",
    ".kccccr.",
    ".kCCCCrr",
    "..kssek.",
    "..ksssk.",
    ".kwpwpwk",
    ".kwwwwk.",
    "...kbbk.",
]
DOUG_D0 = [
    "..kkkk..",
    ".kccccck",
    ".krrrrrk",
    ".ksesesk",
    ".kssssk.",
    "kwpwwpwk",
    ".kwwwwk.",
    "..kb.bk.",
]
DOUG_D1 = [
    "..kkkk..",
    ".kccccck",
    ".krrrrrk",
    ".ksesesk",
    ".kssssk.",
    "kwpwwpwk",
    ".kwwwwk.",
    "...kbbk.",
]
DOUG_U0 = [
    "..kkkk..",
    ".kcccccC",
    ".kcccccC",
    ".kCCCCCk",
    ".kwpwwpk",
    "kwwppwwk",
    ".kwwwwk.",
    "..kb.bk.",
]
DOUG_U1 = [
    "..kkkk..",
    ".kcccccC",
    ".kcccccC",
    ".kCCCCCk",
    ".kwpwwpk",
    "kwwppwwk",
    ".kwwwwk.",
    "...kbbk.",
]

# --- a Vumpire (a vampiric umpire), facing right, two walking frames. Facing left is mirrored.
VAMP_LEGEND = dict(k='ink', K='uni', v='vskin', R='vred', f='white', r='vred', w='white')
VAMP_R0 = [
    "..kkkk..",
    ".kKKKKk.",
    ".kKKKKkk",
    "..kvvRk.",
    "..kvfvk.",
    ".krwwwrk",
    ".kKKKKk.",
    "..kk.kk.",
]
VAMP_R1 = [
    "..kkkk..",
    ".kKKKKk.",
    ".kKKKKkk",
    "..kvvRk.",
    "..kvfvk.",
    ".krwwwrk",
    ".kKKKKk.",
    ".kk..kk.",
]

# --- a gold bar. Its legend uses palette numbers directly (63 is the palette's brightest gold; 59 its darkest).
GOLD_LEGEND = dict(k='ink', t=63, h=55, g=61, d=59)
GOLD = [
    "........",
    "..kkkk..",
    ".kthttk.",
    "khggggdk",
    "kggggggk",
    "kddddddk",
    ".kkkkkk.",
    "........",
]


class Sheet:
    def __init__(self):
        self.px = [[0] * 128 for _ in range(128)]
        self.x = self.y = self.rowh = 0

    def place(self, rows, legend, flip=False):
        """draw an ASCII picture at the next free spot; returns (x, y)"""
        w, h = len(rows[0]), len(rows)
        if self.x + w > 128:
            self.x, self.y, self.rowh = 0, self.y + self.rowh, 0
        x0, y0 = self.x, self.y
        for j, row in enumerate(rows):
            if flip:
                row = row[::-1]
            for i, ch in enumerate(row):
                if ch != '.':
                    v = legend[ch]
                    self.px[y0 + j][x0 + i] = idx(v) if isinstance(v, str) else v
        self.x += w
        self.rowh = max(self.rowh, h)
        return x0, y0

    def write_bmp(self, path):
        pal = b''.join(bytes([c[2], c[1], c[0], 0]) for c in palette.PAL)
        rows = b''.join(bytes(self.px[y]) for y in range(127, -1, -1))
        hdr = struct.pack('<2sIHHI', b'BM', 14 + 40 + 1024 + len(rows), 0, 0, 14 + 40 + 1024)
        dib = struct.pack('<IiiHHIIiiII', 40, 128, 128, 1, 8, 0, len(rows), 2835, 2835, 256, 256)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        open(path, 'wb').write(hdr + dib + pal + rows)


def build(folder):
    s = Sheet()
    # frame order everywhere in the lessons: direction * 2 + walking frame, directions RIGHT, LEFT, UP, DOWN
    doug = [s.place(DOUG_R0, DOUG_LEGEND), s.place(DOUG_R1, DOUG_LEGEND),
            s.place(DOUG_R0, DOUG_LEGEND, flip=True), s.place(DOUG_R1, DOUG_LEGEND, flip=True),
            s.place(DOUG_U0, DOUG_LEGEND), s.place(DOUG_U1, DOUG_LEGEND),
            s.place(DOUG_D0, DOUG_LEGEND), s.place(DOUG_D1, DOUG_LEGEND)]
    vamp = [s.place(VAMP_R0, VAMP_LEGEND), s.place(VAMP_R1, VAMP_LEGEND),
            s.place(VAMP_R0, VAMP_LEGEND, flip=True), s.place(VAMP_R1, VAMP_LEGEND, flip=True)]
    gold = s.place(GOLD, GOLD_LEGEND)
    s.write_bmp(os.path.join(folder, 'assets', 'spr', 'spr.bmp'))
    def arr(name, pts, axis):
        return 'static const unsigned char %s_%s[%d] = { %s };\n' % (name, 'xy'[axis], len(pts), ', '.join(str(p[axis]) for p in pts))
    h = ['/* generated by art/make_art.py: where each sprite frame sits on the 128x128 sprite page */\n#ifndef ART_H\n#define ART_H\n',
         '/* frame = direction * 2 + walking frame; directions: RIGHT 0, LEFT 1, UP 2, DOWN 3 */\n',
         arr('doug', doug, 0), arr('doug', doug, 1), arr('vamp', vamp, 0), arr('vamp', vamp, 1),
         '#define GOLD_GX %d\n#define GOLD_GY %d\n' % gold,
         '#define SPR_W 8   /* every sprite is 8x8 */\n#endif\n']
    os.makedirs(os.path.join(folder, 'src'), exist_ok=True)
    open(os.path.join(folder, 'src', 'art.h'), 'w').write(''.join(h))
    return s


if __name__ == '__main__':
    build(sys.argv[1] if len(sys.argv) > 1 else '.')
    print('wrote', os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else '.'))
