#!/usr/bin/env python3
"""
Run a ROM in the GameTank emulator with scripted buttons and screenshots (see lessons/11-testing for how it works).

  python3 tools/drive.py lessons/03-moving-doug/test.txt            # builds nothing: run ./build.sh first
  GTE=/path/to/GameTankEmulator python3 tools/drive.py <scenario>   # where your emulator is

A scenario is a text file, one event per line:
    30 RIGHT          from frame 30, hold RIGHT (combine buttons with +, e.g. RIGHT+A; 0 releases everything)
    60 shot after     at frame 60, save a screenshot named "after"
    60 peek score     at frame 60, print the byte(s) of the C global "score" (name from the map file)
    60 peek 0x3030 16 at frame 60, print 16 bytes at memory address $3030 (a name or a 0x address, both work)
    60 poke score 7   at frame 60, set the C global "score" to 7
    90 quit

It needs the emulator patched with tools/emulator-script.patch (the stock emulator has no scripting).
Screenshots are written to the "shots" folder next to the scenario as PNG files.
"""
import os, re, struct, subprocess, sys, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
EMU = os.environ.get('GTE') or os.path.join(ROOT, 'GameTankEmulator', 'bin', 'GameTankEmulator')


def symbols(mapfile):
    txt = open(mapfile).read()
    sec = txt[txt.index('Exports list by name'):txt.index('Exports list by value')]
    return {m.group(1): int(m.group(2), 16) for m in re.finditer(r'\b_(\w+)\s+([0-9A-F]{6})\s+\w+', sec)}


def addr(syms, name):
    """a C global's address from the map file, or a literal 0x... address"""
    return int(name, 16) if name.lower().startswith('0x') else syms[name]


def bmp_to_png(src, dst):
    d = open(src, 'rb').read()
    off = struct.unpack('<I', d[10:14])[0]
    w, h = struct.unpack('<ii', d[18:26])
    bpp = struct.unpack('<H', d[28:30])[0]
    ch = bpp // 8
    stride = (w * bpp + 31) // 32 * 4
    rows = []
    for y in range(abs(h)):
        r = off + (abs(h) - 1 - y if h > 0 else y) * stride
        line = d[r:r + w * ch]
        rows.append(b'\x00' + b''.join(line[i * ch + 2:i * ch + 3] + line[i * ch + 1:i * ch + 2] + line[i * ch:i * ch + 1] for i in range(w)))
    def chunk(t, c):
        return struct.pack('>I', len(c)) + t + c + struct.pack('>I', zlib.crc32(t + c) & 0xffffffff)
    png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, abs(h), 8, 2, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress(b''.join(rows), 9)) + chunk(b'IEND', b''))
    open(dst, 'wb').write(png)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    scenario = os.path.abspath(sys.argv[1])
    lesson = os.path.dirname(scenario)
    name = os.path.basename(lesson)
    rom = os.environ.get('GT_ROM') or os.path.join(ROOT, 'bin', name + '.gtr')
    mapfile = os.path.join(ROOT, 'build', name, 'build', 'out.map')
    shots_dir = os.path.join(lesson, 'shots')
    os.makedirs(shots_dir, exist_ok=True)
    syms = symbols(mapfile)
    out, shots = [], []
    for line in open(scenario):
        line = line.split('#')[0].strip()
        if not line:
            continue
        p = line.split()
        if len(p) >= 3 and p[1] == 'peek':
            out.append('%s peek %04X %s' % (p[0], addr(syms, p[2]), p[3] if len(p) > 3 else '1'))
        elif len(p) >= 4 and p[1] == 'poke':
            out.append('%s poke %04X %s' % (p[0], addr(syms, p[2]) + (int(p[4]) if len(p) > 4 else 0), p[3]))
        elif len(p) >= 3 and p[1] == 'shot':
            out.append('%s shot %s' % (p[0], os.path.join('/tmp', 'gt_%s_%s.bmp' % (name, p[2]))))
            shots.append(p[2])
        else:
            out.append(line)
    tmp = os.path.join('/tmp', 'gt_%s.script' % name)
    open(tmp, 'w').write('\n'.join(out) + '\n')
    env = dict(os.environ, GT_FAST='1', GT_SCRIPT=tmp)
    r = subprocess.run([EMU, rom], env=env, capture_output=True, text=True, timeout=300, cwd=ROOT)
    for l in r.stdout.splitlines():
        if l.startswith('PEEK') or l.startswith('vframe'):
            print(l)
    for s in shots:
        bmp = os.path.join('/tmp', 'gt_%s_%s.bmp' % (name, s))
        if os.path.exists(bmp):
            bmp_to_png(bmp, os.path.join(shots_dir, s + '.png'))
            print('screenshot', os.path.join(shots_dir, s + '.png'))
        else:
            print('no screenshot for', s)


if __name__ == '__main__':
    main()
