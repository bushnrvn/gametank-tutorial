# Lesson 10: finishing touches

**You will build:** a treasure to hunt (a gold bar worth 500, hidden in a random cave), and Doug and the Vumpires nodding their
heads on every beat of the music while they walk.

![nodding](shots/nodding.png)

## The gold bar

It is placed in a random cell of a random cave when the field is built, drawn under everything that moves (a Vumpire can walk over
it; only Doug can pick it up), and collected when Doug's middle is in its cell:

```c
    i = rng() % MAXE;                                   /* the gold bar: a random cell of a random cave */
    gold_c = CAVE_C[i] + rng() % CAVE_W; gold_r = CAVE_R[i]; gold_on = 1;
```

```c
    /* the gold bar: enemies walk over it, but only Doug can pick it up */
    if (gold_on && ((px + 4) >> 3) == gold_c && ((py + 4) >> 3) == gold_r) {
        gold_on = 0;
        score += 500;
        add_popup(px, py, 500);
        SFX(ASSET__audio__out_sfx_ID, 3);
    }
```

## The beat

The tune is 126 beats a minute. The game counts time in vsyncs (60 a second), so a beat is 3600 / 126 = **28.57** vsyncs: not a whole
number. If you count whole vsyncs and wrap at 28 (or 29) the head nod slides out of time with the music within a few seconds.
So the beat is kept as a **fixed-point number**: `beat_acc` counts 256ths of a vsync, a whole vsync adds 256, and a beat is
`28.57 * 256 = 7314` of them. The 6502 has no decimals, but a count of 256ths is a perfectly good one.

```c
#define BEAT_FP 7314u            /* 28.57 * 256 */
#define NOD_FP 1536u             /* the head stays down for 6 vsyncs */
#define SONG_VSYNCS 914u         /* 32 beats: the tune is 8 bars of 4 */
unsigned int beat_acc, song_t;
unsigned char bob;               /* 1 while the heads are down */
```

Every frame, add the vsyncs that *really* went by (`music_poll()` counts them, lesson 7), wrap at a beat, and nod for the first
6 vsyncs of every beat. When the song loops (every 914 vsyncs, 32 beats) it starts the beat again from the exact point the song is at.

```c
        vs = music_poll();

        /* move the beat on by the vsyncs that really went by, and wrap it when a beat (or the whole song) is over */
        beat_acc += vs * 256u; song_t += vs;
        while (beat_acc >= BEAT_FP) beat_acc -= BEAT_FP;
        if (song_t >= SONG_VSYNCS) { song_t -= SONG_VSYNCS; beat_acc = song_t * 256u; while (beat_acc >= BEAT_FP) beat_acc -= BEAT_FP; }
        bob = (state == ST_PLAY && beat_acc < NOD_FP);
```

## Nodding heads

A nod is a one-pixel drop of the top five rows of the sprite. That is two blits (body, then head a pixel lower, over it) instead of one:

```c
void sprite_nod(unsigned char x, unsigned char y, unsigned char gx, unsigned char gy, unsigned char nod)
{
    if (bob && nod) {
        queue_draw_sprite(x, y + 5, SPR_W, 3, gx, gy + 5, slot_spr);
        queue_draw_sprite(x, y + 1, SPR_W, 5, gx, gy, slot_spr);
    } else {
        queue_draw_sprite(x, y, SPR_W, SPR_W, gx, gy, slot_spr);
    }
}
```

And it should only happen while the sprite is moving: each walker has a "ticks left in which it counts as moving" counter that
`sprite_nod` is told about.

> **Gotcha.** You cannot get both "keeps strict time with the music" *and* "stops the moment the walker stops". The head's
> position is a function of the music, so when the walker stops, the head is either mid-nod (looks like a glitch) or has to jump to
> the top (looks like a twitch). The compromise here is a short grace period: a walker counts as moving for a few ticks after each step
(`e_mv`, `pmv`), so the nod does not flicker between steps.

## Testing the beat

`test-beat.txt` reads the `bob` variable every frame for a few hundred frames; you should see it switch on about every 28 or 29 frames while
the game is in play. (`tools/drive.py` is lesson 11.)

## Try it

* Make the enemies nod on the off-beat.
* Make the gold bar glitter on the beat.

Next: [lesson 11, testing](../11-testing/README.md).
