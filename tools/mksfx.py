#!/usr/bin/env python3
"""
Writes the sound effects used in lesson 7 into lessons/07-sound/code/assets/audio/*.sfx

A .sfx file is what the SDK's sound player reads, byte for byte:
    byte 0       the number of ticks the effect lasts
    byte 1       "feedback" (how much each of the four FM operators modulates the next: more is harsher, 0 is a plain tone)
    then 8 bytes per tick (the player plays one tick per vsync, 60 a second):
       4 bytes   the volume of operators 0-3 (0-15)
       4 bytes   the note number of operators 0-3 (the same numbers MIDI uses; 60 is middle C)
The sound is the fourth operator (the "carrier") heard through the others, so these effects keep the first three silent and use
the first one, an octave up, as a modulator that gives the tone some edge.
"""
import os, sys


def sfx(path, notes, volumes, feedback=0x80):
    """notes and volumes: one entry per tick (the carrier's note and volume)"""
    assert len(notes) == len(volumes)
    data = bytes([len(notes), feedback])
    for n, v in zip(notes, volumes):
        data += bytes([0, 0, 0, v, n + 12, n, n, n])
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, 'wb').write(data)


def ramp(a, b, n):
    return [a + (b - a) * i // max(1, n - 1) for i in range(n)]


def build(folder):
    sfx(folder + '/throw.sfx', ramp(64, 50, 6), ramp(7, 2, 6), 0x40)            # a quick falling swoosh
    sfx(folder + '/hit.sfx', ramp(48, 40, 5), [9, 9, 8, 6, 3], 0xA0)              # a flat thump when a ball finds a Vumpire
    sfx(folder + '/out.sfx', ramp(52, 76, 10), ramp(9, 3, 10), 0x60)              # rising: a Vumpire is out
    sfx(folder + '/dig.sfx', [40, 40, 38, 36], [5, 5, 4, 2], 0x80)                # a short crunch
    sfx(folder + '/die.sfx', ramp(60, 32, 14), ramp(10, 1, 14), 0x60)             # falling away: Doug is caught


if __name__ == '__main__':
    build(sys.argv[1] if len(sys.argv) > 1 else '.')
