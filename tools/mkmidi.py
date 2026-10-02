#!/usr/bin/env python3
"""
Writes the tune used in lesson 7 as a standard MIDI file: lessons/07-sound/code/assets/audio/theme.mid

The SDK's converter (scripts/converters/midiconvert.js) turns a .mid file into the GameTank's own song format. It uses the first
four MIDI channels (0-3) as the console's four sound channels and plays each with an instrument you choose in asset.cfg:
  channel 0 melody (piano)   channel 1 bass (slap bass)   channel 2 drums (snare)   channel 3 harmony (piano)

No libraries: a MIDI file is a header and a list of "at this time, switch this note on or off" events, which is easy to write by hand.
"""
import os, struct, sys

TICKS = 96                 # MIDI ticks per quarter note
BPM = 126


def vlq(n):
    """MIDI writes times as variable-length numbers: 7 bits per byte, high bit set on all but the last"""
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append((n & 0x7F) | 0x80)
        n >>= 7
    return bytes(reversed(out))


class Track:
    def __init__(self):
        self.events = []                                   # (tick, bytes)

    def note(self, tick, length, channel, note, velocity=96):
        self.events.append((tick, bytes([0x90 | channel, note, velocity])))
        self.events.append((tick + length - 1, bytes([0x80 | channel, note, 0])))

    def write(self, path):
        evs = sorted(self.events, key=lambda e: e[0])
        data = b'\x00\xff\x51\x03' + struct.pack('>I', 60000000 // BPM)[1:]      # tempo: microseconds per quarter note
        last = 0
        for tick, ev in evs:
            data += vlq(tick - last) + ev
            last = tick
        data += b'\x00\xff\x2f\x00'                                                 # end of track
        os.makedirs(os.path.dirname(path), exist_ok=True)
        open(path, 'wb').write(b'MThd' + struct.pack('>IHHH', 6, 0, 1, TICKS) + b'MTrk' + struct.pack('>I', len(data)) + data)


def build(path):
    t = Track()
    q = TICKS                                              # one beat
    # eight bars of 4/4. Melody: four quarter notes a bar (MIDI note numbers: 72 is the C above middle C).
    melody = [[76, 76, 79, 76], [74, 74, 77, 74], [72, 76, 79, 84], [79, 77, 76, 74],
              [76, 76, 79, 76], [74, 74, 77, 74], [72, 74, 76, 77], [79, 79, 72, 72]]
    bass = [[48, 55, 48, 55], [43, 50, 43, 50], [48, 55, 48, 55], [43, 50, 43, 50],
            [48, 55, 48, 55], [43, 50, 43, 50], [48, 55, 53, 55], [43, 43, 48, 48]]
    harmony = [64, 62, 64, 62, 64, 62, 65, 67]            # one long note a bar
    for bar in range(8):
        for beat in range(4):
            tick = (bar * 4 + beat) * q
            t.note(tick, q, 0, melody[bar][beat])
            t.note(tick, q, 1, bass[bar][beat])
            if beat in (1, 3):
                t.note(tick, q // 2, 2, 38)                # the snare, on beats 2 and 4
        t.note(bar * 4 * q, 4 * q, 3, harmony[bar], 64)
    t.write(path)


if __name__ == '__main__':
    build(sys.argv[1] if len(sys.argv) > 1 else 'theme.mid')
