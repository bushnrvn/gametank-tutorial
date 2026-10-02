#!/usr/bin/env python3
"""
Writes each lesson's README.md from README.src.md, pasting real code into it, so the text can never drift from the programs.

  python3 tools/snippets.py              expand every lesson's README.src.md (and the top-level one, if it has any)
  python3 tools/snippets.py --check      the same, but only say whether any README.md is out of date

In a README.src.md:
  {{fn NN-lesson name}}          the whole C function called `name` from that lesson's src/main.c
  {{lines NN-lesson FIRST|LAST}} from the first line matching regex FIRST to the first later line matching LAST, inclusive
  {{file NN-lesson path}}        a whole file from the lesson's code/ (or the lesson folder: test.txt), with a language by extension
  {{tour GAME_DIR name}}         (the tour only) the function `name` from any .c file in the Dug Out source folder given by $DUGOUT_SRC
"""
import glob, os, re, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
LANG = {'.c': 'c', '.h': 'c', '.s': 'asm', '.json': 'json', '.txt': 'text', '.py': 'python', '.sh': 'sh', '.cfg': 'text'}


def block(text, lang):
    return '```%s\n%s\n```' % (lang, text.rstrip('\n'))


def read(path):
    return open(path).read().split('\n')


def function(lines, name):
    start = None
    for i, l in enumerate(lines):
        if re.match(r'^[A-Za-z_][\w \*]*\b%s\s*\(' % re.escape(name), l) and not l.rstrip().endswith(';'):
            start = i
            break
    if start is None:
        raise SystemExit('no function %s' % name)
    j = start
    while not lines[j].startswith('{'):
        j += 1
    k = j
    while not lines[k].startswith('}'):
        k += 1
    return '\n'.join(lines[start:k + 1])


def expand(m, readme):
    kind, rest = m.group(1), m.group(2).strip()
    here = os.path.dirname(readme)
    if kind == 'fn':
        les, name = rest.split()
        return block(function(read(os.path.join(ROOT, 'lessons', les, 'code/src/main.c')), name), 'c')
    if kind == 'lines':
        les, pat = rest.split(None, 1)
        a, b = pat.split('|')
        ls = read(os.path.join(ROOT, 'lessons', les, 'code/src/main.c'))
        i = next(n for n, l in enumerate(ls) if re.search(a, l))
        j = next(n for n in range(i, len(ls)) if re.search(b, ls[n]))
        return block('\n'.join(ls[i:j + 1]), 'c')
    if kind == 'file':
        les, path = rest.split()
        p = os.path.join(ROOT, 'lessons', les, 'code', path)
        if not os.path.exists(p):
            p = os.path.join(ROOT, 'lessons', les, path)
        return block(open(p).read(), LANG.get(os.path.splitext(p)[1], ''))
    if kind == 'tour':
        name = rest
        src = os.environ.get('DUGOUT_SRC') or os.path.join(ROOT, 'dugout', 'src')
        for f in sorted(glob.glob(os.path.join(src, '*.c'))):
            try:
                return block(function(read(f), name), 'c')
            except SystemExit:
                continue
        raise SystemExit('no function %s in %s' % (name, src))
    raise SystemExit('unknown directive ' + kind)


def main():
    check = '--check' in sys.argv
    stale = 0
    for srcf in sorted(glob.glob(os.path.join(ROOT, 'lessons', '*', 'README.src.md')) + [os.path.join(ROOT, 'tour', 'README.src.md')]):
        if not os.path.exists(srcf):
            continue
        tour_src = os.environ.get('DUGOUT_SRC') or os.path.join(ROOT, 'dugout', 'src')
        if srcf.endswith(os.path.join('tour', 'README.src.md')) and not os.path.isdir(tour_src):
            print('skipping the tour: no Dug Out source at %s (git clone --branch v1.4.0 https://github.com/bushnrvn/dug-out dugout, or set DUGOUT_SRC)' % tour_src)
            continue
        out = re.sub(r'\{\{(fn|lines|file|tour) (.*?)\}\}', lambda m: expand(m, srcf), open(srcf).read())
        dst = srcf.replace('.src.md', '.md')
        if check:
            if not os.path.exists(dst) or open(dst).read() != out:
                print('out of date:', os.path.relpath(dst, ROOT)); stale += 1
        else:
            open(dst, 'w').write(out)
    if check and stale:
        sys.exit(1)


main()
