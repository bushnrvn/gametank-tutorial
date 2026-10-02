# Lesson 9: game flow

**You will build:** a title screen, a pause when Doug is caught, a game over screen, and a best score that survives switching the
console off.

| title | game over | title again |
|---|---|---|
| ![title](shots/title.png) | ![over](shots/over.png) | ![title again](shots/title-again.png) |

## A state machine in a switch

{{lines 09-game-flow ^enum \{ ST_TITLE|;}}

Everything the game can be doing is one value, `state`. The main loop does `switch (state)`: one case per screen, each with its own
update, and the drawing looks at `state` too. A `state_timer` counts frames since the state began, which is all you need for
"stay on this screen for a second", "ignore the button for half a second so the player does not skip the screen by accident" and
"flicker for a moment".

{{fn 09-game-flow play_tick}}

## Saving to the cartridge

A GameTank cartridge is flash memory, and the SDK's `persist` module can rewrite one sector of it. You use `clear_save_sector` and
`save_write`.

{{fn 09-game-flow save_best}}

Flash can only be written after being erased, and erasing takes a noticeable moment, so you save at the one moment nobody is
playing: when the game ends. The five bytes are two marker bytes (a never-written sector reads as `FF`s), the score, and a check
byte, so a damaged or empty save is recognised and ignored rather than shown as a score of 62000.

{{fn 09-game-flow load_best}}

### Reading the save from the right bank

The save sector sits in its own ROM bank. To read it, that bank must be switched into the `$8000` window, which is where the game's code
(PROG0) is. Switching the window out from under running code would crash, so the read is a function in the **fixed bank**:

{{fn 09-game-flow save_peek}}

> **Gotcha.** The persist module's calls also switch banks, so `save_best` and `load_best` must be called from code that is *not*
> in the fixed bank (they are in PROG0 here). Calling them from the fixed bank switches the code away mid-call.

## Testing it

The emulator keeps the save in a file next to the ROM (`<rom>.xor`), so a test can do two runs and prove the score survives:
`test.txt` plays a game, scores 1230, and ends it; `test-second-run.txt` starts the ROM again and checks `best_score`. Lesson 11 is about how
that works.

## Try it

* Save the three best scores instead of one.
* Add a "NEW BEST!" flash when you pass the old best, not only at the end.

Next: [lesson 10, finishing touches](../10-finishing-touches/README.md).
