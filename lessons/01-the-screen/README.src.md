# Lesson 1: the screen

**You will build:** a box that bounces around a grey screen. It is five lines of game and a loop, and every later lesson sits on top of it.

**Run it:** `./build.sh lessons/01-the-screen`, then open `bin/01-the-screen.gtr` in a GameTank emulator (or put it on a cartridge).

![the box, early and later](shots/later.png)

## The program

{{file 01-the-screen src/main.c}}

## What is going on

**The screen is 128 x 128 pixels, but you only see 113 rows.** The top 7 and bottom 8 rows fall outside the part a TV shows, so the
game draws a black border over them (`queue_clear_border`) and keeps everything that matters inside rows 8 to 119. The boundaries
in the bounce (`box_y == 8`, `== 112`) come from that.

**Colours are one byte.** There are 256 of them and colour 0 means "transparent" when a sprite is drawn. You will not need to
memorise the numbers: `python3 tools/palette.py --grid` prints the whole palette as a chart, and `python3 tools/palette.py 237,178,98`
finds the nearest palette colour to any RGB value you give it. The lessons only use a handful.

**You do not draw directly; you queue jobs.** The GameTank has a *blitter*, a second chip that fills boxes and copies sprites
into the picture much faster than the 6502 could. The SDK's *draw queue* is a list of jobs for it: `queue_clear_screen`,
`queue_draw_box` and later `queue_draw_sprite` only add to the list, and the blitter works through it in the background while your
code carries on. Two rules follow:

1. Call `await_draw_queue()` before you rely on the picture being finished.
2. Never queue so much that the list overflows. It holds 250 jobs (`QUEUE_MAX`), which sounds like plenty until the playfield is a
   grid of sprites. Lesson 4 has a way around it.

**Two pictures, one shown.** The console has two frame buffers. While the TV shows one, you draw the next on the other, and
`flip_pages()` swaps them. That is why `flip_pages` comes last, and why `await_vsync` (wait for the TV to finish its frame) comes
just before it: flipping in the middle of a frame would tear the picture.

**Order matters inside the frame.** Clear first, draw things in the order they should overlap (later jobs cover earlier ones),
border last.

## Try it

* Change the box size or colour. Which numbers make the bounce stop working, and why?
* `await_vsync(2)` instead of `(1)` halves the speed. The game you are building runs at 30 frames a second, which is that.
* Add a second box that bounces the other way.

Next: [lesson 2, sprites](../02-sprites/README.md).
