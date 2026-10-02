# Lesson 11: testing without a controller

**You will build:** nothing in the game. You will learn to drive it from a script: press buttons on a schedule, look inside its memory,
change its memory, and take screenshots, so that "does it still work?" takes seconds and does not need hands.

## Why bother

By lesson 9 the game has a title, a state machine, a save, three banks and a search that runs on a 6502. Most bugs in a game like
this are not "it crashes": they are "after I do X then Y, the score is wrong" and "the second time I run it the save is missing".
You find those by playing, and you stop finding them once you have played the same thirty seconds fifty times. A script plays the
thirty seconds for you.

## What you need

The stock emulator has no scripting, so there is a small patch for it (`tools/emulator-script.patch`: it reads a script from an
environment variable, can run as fast as the computer allows, and exits when told). To use it, get the
[GameTank emulator](https://github.com/clydeshaffer/GameTankEmulator), check out commit `9896544` (the version the patch was made
for), apply the patch from inside it with `patch -p1 < ../tools/emulator-script.patch`, and build it as its README says. Put the
result at `./GameTankEmulator/bin/GameTankEmulator` next to `build.sh`, or set `GTE` to its path. It is only needed for testing:
the lessons run on any GameTank emulator or on hardware.

## A scenario

A scenario is a text file: a frame number, then something to do at that frame (the emulator frame is a vsync, 60 a second).

{{file 05-enemies test.txt}}

The verbs:

| line | does |
|---|---|
| `300 RIGHT` | from frame 300, hold the d-pad right (`RIGHT+A` for two buttons, `0` to let go) |
| `300 shot name` | save a screenshot into the lesson's `shots/` folder |
| `300 peek score` | print the C global `score` (any global, by name) |
| `300 peek score 2` | print two bytes (a 16-bit number is printed low byte first, so `206 4` means 4*256+206 = 1230) |
| `300 poke lives 1` | set a global to a value |
| `300 poke map 0 54` | set byte 54 of the array `map` to 0 |
| `700 quit` | stop the emulator |

Names are looked up in the **map file** the linker wrote (`build/<lesson>/build/out.map`), which is why `tools/drive.py` is told
about a lesson, not a ROM. A C global is at a fixed address, which is what makes this possible. (`static` variables inside functions and
register variables are not globals and are not found.)

Run one:

```bash
./build.sh lessons/05-enemies
python3 tools/drive.py lessons/05-enemies/test.txt
```

and the output is the peeks and the screenshots that were taken.

## Testing by poking

`poke` is the useful verb. You do not want a test that spends ten simulated minutes digging to a cave; you want to say "there is a
tunnel to the cave now" and see what happens. In the scenario above, the three `poke map` lines open a path, and 80 frames later
`peek e_x` says the Vumpire has moved. Compare the screenshots:

| sealed | hunting |
|---|---|
| ![sealed](../05-enemies/shots/sealed.png) | ![hunting](../05-enemies/shots/hunting.png) |

## Testing the save

A flash write is the easiest thing to break and the hardest to notice (everything looks fine until the next power cycle).
The emulator keeps the save in `<rom>.xor` next to the ROM, so two runs of the same ROM are a power cycle:

{{file 09-game-flow test.txt}}

{{file 09-game-flow test-second-run.txt}}

Run the first, then the second. If `best_score` in the second run is not what the first run scored, the save is broken. (Delete
`bin/09-game-flow.xor` between rounds if you want a clean start.)

## Testing time

The beat in lesson 10 is a number that must come out right: the ear is a harsh judge of a head nod that slides off the beat.
`lessons/10-finishing-touches/test-beat.txt` peeks `bob` every second frame and you can read off the pattern: one on-phase of
about 6 vsyncs, every 28.6.

## Gotchas

* **A screenshot is not a test.** Peek the *state* (a score, a position, a lives count) and check the numbers; use pictures to find
  out *why* a number is wrong.
* **Frame numbers drift.** If you add a feature that makes a screen last a second longer, every later frame number in your scenarios
  moves. Keep scenarios short and set things with `poke` instead of waiting for them.
* **The emulator is not the console.** A test that passes here says nothing about real timing, audio or a real flash chip. Run the
  finished ROM on hardware before you believe it.

## Try it

* Write a scenario that digs the first cave by walking (no pokes), and compare it to the poke version.
* Write one that proves a stunned Vumpire still kills Doug (lesson 6): `poke` a Vumpire's state and put it on Doug.

Next: [lesson 12, a tour of the finished game](../../tour/README.md).
