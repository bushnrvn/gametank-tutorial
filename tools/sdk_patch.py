#!/usr/bin/env python3
"""
Small changes to the SDK's own engine files, applied to the build copy of the SDK (never to your checkout).

  python3 tools/sdk_patch.py <build folder> vsync-counter

vsync-counter: the SDK has no way to count vsyncs. await_vsync() only waits for the NEXT one, so when a frame takes longer than you
meant (say it needs 3 vsyncs when you asked for 2) the extra one passes unseen, and anything that keeps time by counting calls to
it (the music!) runs slow. This change makes the NMI handler (which runs on every vsync) add one to a byte at $3210 in the audio
RAM (the part the main CPU can see, at $3000-$3FFF), which every part of the program can see whatever RAM bank is switched in. A lesson that asks for the change lists
"vsync-counter" in a file called sdk-patches next to its code/ folder.

Why $3210 and not an ordinary variable: the draw queue keeps its lists in RAM bank 1 and switches that bank in for a moment, many
times a frame. If the NMI lands in that moment, an ordinary variable would be written in the wrong bank. That RAM is the same
in every bank. (The SDK's NMI handler already skips its own counter when bank 1 is switched in, which is why it has a marker byte at
$1FFF; the change below also puts the marker where the SDK forgot to.)
"""
import os, re, sys


def edit(path, old, new, count=1):
    s = open(path).read()
    assert re.search(old, s, re.S), 'could not find the place to change in %s:\n%s' % (path, old)
    s = re.sub(old, lambda m: new, s, count=count, flags=re.S)
    open(path, 'w').write(s)


def vsync_counter(work):
    gt = os.path.join(work, 'src', 'gt')
    edit(os.path.join(gt, 'interrupt.s'), r'\.import\s+_frameflag\n', '.import   _frameflag\n.import   _vsync_ctr\n')
    edit(os.path.join(gt, 'interrupt.s'),
         r'_nmi_int:\n\s+PHA\n\s+LDA \$1FFF\n\s+BNE nmi_done\n\s+STZ _frameflag\nnmi_done:',
         '''; VSYNC_RAW lives in the audio RAM, which the main CPU sees at $3000-$3FFF, and which is visible whichever
; RAM bank is mapped. It is counted on every vsync; the other two variables are skipped when the draw queue's RAM bank is mapped.
VSYNC_RAW = $3210

_nmi_int:
        PHA
        INC VSYNC_RAW
        LDA $1FFF
        BNE nmi_done
        STZ _frameflag
        INC _vsync_ctr
nmi_done:''')
    # (crt0 uses a plain text replace: the pieces contain regex-special characters)
    p = os.path.join(gt, 'crt0.s')
    s = open(p).read()
    old = '\tSTZ BankReg\n\tSTZ $1FFF\n\n\tLDA #%00000111'
    assert old in s, 'could not find the place to change in crt0.s'
    s = s.replace(old, '\tSTZ BankReg\n\tSTZ $1FFF\n\t; mark RAM bank 1 (the draw queue\'s) so the NMI handler can tell when it is mapped in\n\tLDA #$40\n\tSTA BankReg\n\tLDA #1\n\tSTA $1FFF\n\tSTZ BankReg\n\n\tLDA #%00000111', 1)
    open(p, 'w').write(s)
    edit(os.path.join(gt, 'gfx', 'gfx_sys.c'), r'char draw_busy;\n', 'char draw_busy;\nvolatile unsigned char vsync_ctr;\n')
    edit(os.path.join(gt, 'gfx', 'gfx_sys.h'), r'extern char draw_busy;\n', 'extern char draw_busy;\nextern volatile unsigned char vsync_ctr; /* increments every vsync (NMI) */\n')


PATCHES = {'vsync-counter': vsync_counter}

if __name__ == '__main__':
    work = sys.argv[1]
    for name in sys.argv[2:]:
        PATCHES[name](work)
        print('applied', name)
