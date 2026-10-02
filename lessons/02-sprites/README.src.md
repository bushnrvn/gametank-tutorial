# Lesson 2: sprites

**You will build:** the cast of the game on screen: Doug facing every way, a Vumpire, and a gold bar, with walking animation.

![the cast](shots/a.png)

## Drawing the art

The game's art is made by a short script, `art/make_art.py`, which writes `spr.bmp` (a 128 x 128, 256-colour bitmap, one 8 x 8 cell
for every picture) and `art.h` (where each picture sits on the sheet). Look at the script: each picture is a few rows of text,
one character per pixel, mapped to palette colours. Editing a character and re-running the script is the whole art pipeline, and
needs no paint program. If you prefer a paint program, draw the sheet by hand and write `art.h` yourself.

The sheet goes in the project as `assets/spr/spr.bmp`. `make import` (which `build.sh` runs for you) converts it, with the
SDK's tools, into a file in the cartridge and a C name for it (`ASSET__spr__spr_bmp_load_list`).

## The program

{{file 02-sprites src/main.c}}

## What is going on

The blitter copies from "graphics RAM" to the screen. Before you can
draw a sprite you have to copy the sheet into graphics RAM, which is `allocate_sprite`: it returns a *slot* that you pass to every
`queue_draw_sprite`. A 128 x 128 sheet takes a quarter of one page, and the slot remembers which quarter. That copy takes a
moment (it is a few thousand bytes), so it is done before the loop, not in it.

**`queue_draw_sprite(x, y, w, h, gx, gy, slot)`** copies a `w` x `h` rectangle whose top left corner is at `(gx, gy)` on the sheet
to `(x, y)` on the screen. Colour 0 is transparent: that is how a round sprite sits on a background.

**Animation is arithmetic.** Each direction has two walking frames on the sheet, laid out one after the other, so frame number is
`direction * 2 + step`, and `step` flips every 8 frames: `(frame_ct >> 3) & 1`. The tables `doug_x`, `doug_y`, `vamp_x`,
`vamp_y` in `art.h` say where each frame is.

## Gotchas

* `queue_draw_sprite` is a *macro* that expands to several statements. In a `for` or `if` without braces only the first of them is
  repeated or skipped, and the symptom is a stray sprite and missing ones, not an error. Always give it braces, or (as here) wrap
  it in a function of your own.
* Draw order is still queue order. The border goes last.

## Try it

* Draw a different enemy in `make_art.py` and put it on the sheet next to the others.
* Make the Vumpire walk across the screen. Remember the flip between left and right.

Next: [lesson 3, moving Doug](../03-moving-doug/README.md).
