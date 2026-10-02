# Lesson 12: a tour of the finished game

The lessons built a small game from nothing. **Dug Out** is the big one: nine innings, five kinds of enemy, a boss, a digging field
that reshapes itself, and a soundtrack. It is one 1,800-line C file, written with exactly the tools in lessons 1 to 11, and this
chapter walks through how it does the things the lessons only hinted at. Read it with the source open:

* the game: <https://github.com/bushnrvn/dug-out> (this chapter is checked against tag `v1.4.0`)
* its MEGA65 port: <https://github.com/bushnrvn/dug-out-mega65>

The code below is pulled straight from `src/main.c` at that tag.

## The shape of the program

`main` is tiny and in the fixed bank; everything else is in `PROG0` (gameplay) or `PROG1` (the whole-screen scenes: title, game over,
intros, the win). That is lesson 8 exactly. The game code calls `title_scene()` and friends through `bank_call` the same way lesson 8
calls `update_enemies`.

The game starts at `game_main`, which is the lesson 9 state machine with ten states:
title, ready, play, dying, clear, over, pause, intro (a new enemy is introduced), win, and attract (a demo that plays when you leave the
title alone).

## The frame

{{tour music_poll}}

{{tour frame_end}}

This is lesson 7's `music_poll` and lesson 10's problem, grown up. `frame_end` waits for vsyncs by watching the raw counter in the
shared audio RAM (the engine change from lesson 7 is in this game's copy of the SDK: the NMI handler bumps the byte at `$3210` on
every vsync, and `crt0.s` marks RAM bank 1 so that the other counters are skipped when the draw queue has it switched in).
The game flips pages only once two vsyncs have passed, so it runs at 30 frames a second without ever losing a music tick, and the
in-between vsync gets a tick too, so a slow frame does not make the notes bunch up and then rush.

> **Tip.** Anything that must keep real time (music, a beat, a timer) should count vsyncs from a counter that nothing else can stall.
> Anything that only needs to look right can count frames.

## A field that is patched, not redrawn

Lesson 4 drew the field as boxes. Dug Out's field is a picture of dirt with pixel-perfect tunnel edges, too big to draw per frame.
So the picture is **baked into a page of sprite RAM**, and digging *patches* it: when Doug clears a cell, that cell and its four
neighbours are marked dirty, and the next `field_flush` rewrites those 8 x 8 tiles with the right edge shape. Drawing the field
is then a single blit of the whole page.

{{tour patch_cell}}

The edge shape is chosen from four bits: is there dirt above, right, below, left? Sixteen shapes cover every case (`tunnel_px[m]`).

{{tour field_flush}}

The Groundskeeper can also *refill* a cell. For that there is a second, untouched copy of the dirt page, and `restore_cell` copies the
original pixels back.

## Enemies that hunt, wait, and flee

Lesson 5's search is in the game as `bfs_dir`, with two additions. `choose_dir` takes a shortcut sometimes (a Vumpire will go to a
fallen friend's headstone to raise it, a little randomness keeps the routes from being identical every time) and falls back to the
steering you would write before you knew about BFS:

{{tour choose_dir}}

If there is no path at all, a walking enemy *stays in its cave*: that is how the game keeps the sealed caves sealed.

When only two enemies are left, they run for the top. It is the same search, with `goal == 255` meaning "any cell of the top row". One that gets off the screen costs Doug what it would have paid:

{{tour enemy_escape}}

## The Groundskeeper

The Groundskeeper can dig, so it needs a different idea of where it can go: it walks through dirt, only refusing home plates (`gk_ok`).
Its target is the nearest open cell that is *not joined to where it already is*, which means the nearest sealed cave. To
know what is joined to what, it floods the open cells from its own position:

{{tour region_mark}}

{{tour choose_dir_g}}

## The beat

Lesson 10's beat, a little more general: the real song is much longer (`SONG_LEN`), the beat is still a count of 256ths of a vsync
(`BEAT_FP`), and the head nods twice per beat: once on the beat and once halfway, where the snare is.

{{tour beat_tick}}

## Saving

{{tour save_peek}}

The same fixed-bank read as lesson 9, because the save lives in its own bank.

## Testing and release

* `tests/drive.py` is the ancestor of `tools/drive.py` in this tutorial, and `tests/s_*.txt` are about thirty scenarios, one per feature
  (the Groundskeeper digging, the bats, the boss, the escape rule).
* `tests/regress.sh` builds the game with a **fixed random seed** and `tests/baseline.py` records screenshots and variables from a run,
  then compares later runs against it: when you change enemy movement, you want to know *only* that changed.
* A release is a tag, the ROM copied to `releases/`, and the browser build in `docs/` rebuilt from it. See `CHANGELOG.md`.

## The MEGA65 port

The same game runs on a MEGA65 (a modern 6502-family computer, a 45GS02 at 40 MHz with a VIC-IV video chip): the *game logic* is the
same C, the drawing, sound and input are rewritten for that machine, with the GameTank's blitter replaced by the CPU and DMA. If you
liked lesson 8's "the hardware decides how your program is laid out", read `docs/PORT-NOTES.md` in that repository: it is a list of
the places where real hardware disagreed with the emulator, which is the most reliable teacher there is.

## Where to go from here

* Add a tenth state to the lesson 9 machine (a pause screen, a high score table).
* Add a new enemy: a sprite, a `choose_dir` of its own, an intro scene.
* Read the SDK's `src/gt/` headers: they are short, and now you know what they are for.
* Credit where it is due: everything here stands on the GameTank SDK and emulator by Clyde Shaffer.
