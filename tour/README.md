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

```c
static void music_poll(void)
{
    unsigned char n = (unsigned char)(vsync_raw - last_tick);
    if (n > 4) { last_tick = vsync_raw - 4; n = 4; }
    while (n) { ++last_tick; tick_music(); --n; }
}
```

```c
static void frame_end(void)
{
    unsigned char v;
    queue_clear_border(COL_INK);
    await_draw_queue();
    music_poll();
    for (;;) {
        v = vsync_raw;
        while (vsync_raw == v) { }                                   /* wait for the next vsync */
        if ((unsigned char)(vsync_raw - last_flip) >= FRAME_VSYNCS) break;
        music_poll();                                                /* the in-between vsync */
    }
    flip_pages();
    last_flip = vsync_raw;
    music_poll();
}
```

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

```c
static void patch_cell(unsigned char idx)
{
    unsigned char c = idx & 15, r = idx >> 4, m = 0, i, j;
    const unsigned char* src;
    unsigned char* dst;
    if (M(c, r) != 0) return;
    if (M(c, r - 1)) m |= 1;
    if (M(c + 1, r)) m |= 2;
    if (M(c, r + 1)) m |= 4;
    if (c == 0 || M(c - 1, r)) m |= 8;
    src = tunnel_px[m];
    dst = vram + ((unsigned)r << 10) + BG_FIELD_OX + ((unsigned)c << 3);
    for (j = 0; j < 8; ++j) {
        for (i = 0; i < 8; ++i) dst[i] = src[i];
        src += 8; dst += 128;
    }
}
```

The edge shape is chosen from four bits: is there dirt above, right, below, left? Sixteen shapes cover every case (`tunnel_px[m]`).

```c
static void field_flush(void)
{
    unsigned char i, r, c;
    if (!refresh_all && !dirty_n && !rest_n) return;
    if (rest_n && !refresh_all) {
        for (i = 0; i < rest_n; ++i) restore_cell(rest_list[i]);
    }
    rest_n = 0;
    direct_prepare_sprite_ram_array_mode(slot_bg);
    if (refresh_all) {
        for (r = 1; r < ROWS; ++r)
            for (c = 0; c < COLS; ++c)
                patch_cell((r << 4) | c);
        refresh_all = 0;
        for (i = 0; i < (ROWS + 1) * 16; ++i) dirty_flag[i] = 0;
        dirty_n = 0;
        return;
    }
    for (i = 0; i < dirty_n; ++i) {
        patch_cell(dirty_list[i]);
        dirty_flag[dirty_list[i]] = 0;
    }
    dirty_n = 0;
}
```

The Groundskeeper can also *refill* a cell. For that there is a second, untouched copy of the dirt page, and `restore_cell` copies the
original pixels back.

## Enemies that hunt, wait, and flee

Lesson 5's search is in the game as `bfs_dir`, with two additions. `choose_dir` takes a shortcut sometimes (a Vumpire will go to a
fallen friend's headstone to raise it, a little randomness keeps the routes from being identical every time) and falls back to the
steering you would write before you knew about BFS:

```c
static void choose_dir(unsigned char i)
{
    signed char c = e_x[i] >> 3, r = e_y[i] >> 3;
    signed char tc = (px + 4) >> 3, tr = (py + 4) >> 3;
    unsigned char cur = e_dir[i], rev = cur ^ 1, d, best = 255, bd = rev, n = 0, dist, opt[4];
    signed char nc, nr, dc, dr;

    if (e_type[i] == 0 && rng() < 110) {          /* a Vumpire: go and raise a fallen friend */
        for (d = 0; d < MAXC; ++d)
            if (c_on[d]) { tc = (c_x[d] + 4) >> 3; tr = (c_y[d] + 4) >> 3; break; }
    }
    if (tc >= 0 && tc < COLS && tr >= 0 && tr < ROWS && rng() < 235 && bfs_dir(i, (unsigned char)((tr << 4) | tc))) return;     /* a way there along the tunnels: take it */
    dist = rng();                                /* no way (or a sudden whim): aim a few cells off Doug so the routes are not the same every time */
    tc += (dist & 7) - 3;
    tr += ((dist >> 3) & 7) - 3;
    for (d = 0; d < 4; ++d) {
        if (d == rev) continue;
        nc = c + DX[d]; nr = r + DY[d];
        if (!open_cell(nc, nr)) continue;
        opt[n++] = d;
        dc = nc - tc; dr = nr - tr;
        if (dc < 0) dc = -dc;
        if (dr < 0) dr = -dr;
        dist = dc + dr;
        if (dist < best) { best = dist; bd = d; }
    }
    if (n == 0) {
        /* dead end: turn around if we can, else stay put */
        nc = c + DX[rev]; nr = r + DY[rev];
        e_dir[i] = open_cell(nc, nr) ? rev : cur;
        return;
    }
    if (n > 1 && rng() < (e_type[i] ? 70 : 50)) bd = opt[rng() % n];      /* mostly they head straight for Doug */
    e_dir[i] = bd;
}
```

If there is no path at all, a walking enemy *stays in its cave*: that is how the game keeps the sealed caves sealed.

When only two enemies are left, they run for the top. It is the same search, with `goal == 255` meaning "any cell of the top row". One that gets off the screen costs Doug what it would have paid:

```c
static void enemy_escape(unsigned char i)
{
    if (score_h > e_pts[i]) score_h -= e_pts[i]; else { score_h = 0; score_t = 0; }
    score_dirty = 1;
    add_popup(e_x[i], 0, e_pts[i] | 0x8000);
    SFXP(ASSET__audio__thud_sfx_ID, 2);
    e_state[i] = ES_NONE;
    --enemies_left;
}
```

## The Groundskeeper

The Groundskeeper can dig, so it needs a different idea of where it can go: it walks through dirt, only refusing home plates (`gk_ok`).
Its target is the nearest open cell that is *not joined to where it already is*, which means the nearest sealed cave. To
know what is joined to what, it floods the open cells from its own position:

```c
static void region_mark(unsigned char start)
{
    unsigned char head = 0, tail = 0, cur, d, nc, nr, n;
    for (n = 0; n < sizeof fl_par; ++n) fl_par[n] = 0;
    fl_q[tail++] = start; fl_par[start] = 1;
    while (head != tail) {
        cur = fl_q[head++];
        for (d = 0; d < 4; ++d) {
            nc = (cur & 15) + DX[d]; nr = (cur >> 4) + DY[d];
            if (!open_cell((signed char)nc, (signed char)nr)) continue;
            n = (nr << 4) | nc;
            if (fl_par[n]) continue;
            fl_par[n] = 1; fl_q[tail++] = n;
        }
    }
}
```

```c
static void choose_dir_g(unsigned char i)
{
    signed char c = e_x[i] >> 3, r = e_y[i] >> 3;
    signed char tc = (px + 4) >> 3, tr = (py + 4) >> 3;
    unsigned char rev = e_dir[i] ^ 1, d, best = 255, bd = rev, n = 0, dist, opt[4], rr, cc, pass, any = 0;
    signed char nc, nr, dc, dr;

    if (e_type[i] == 3) {
        /* Groundskeeper: go straight (digging, see enemy_update) for the nearest open cell that is not joined to where it is, which is
         * a sealed cave, or failing that the nearest open cell. Never the surface. */
        unsigned char bestd = 255;
        region_mark((unsigned char)((r << 4) | c));
        for (pass = 0; pass < 2 && !any; ++pass)
        for (rr = 1; rr < ROWS; ++rr)
            for (cc = 0; cc < COLS; ++cc) {
                if (M(cc, rr) != 0 || (cc == c && rr == r)) continue;
                if (!pass && fl_par[(rr << 4) | cc]) continue;
                dc = cc - c; dr = rr - r;
                if (dc < 0) dc = -dc;
                if (dr < 0) dr = -dr;
                dist = dc + dr;
                if (dist < bestd) { bestd = dist; tc = cc; tr = rr; any = 1; }
            }
    }
    for (d = 0; d < 4; ++d) {
        if (d == rev) continue;
        nc = c + DX[d]; nr = r + DY[d];
        if (!gk_ok(nc, nr)) continue;
        opt[n++] = d;
        dc = nc - tc; dr = nr - tr;
        if (dc < 0) dc = -dc;
        if (dr < 0) dr = -dr;
        dist = dc + dr;
        if (dist < best) { best = dist; bd = d; }
    }
    if (n == 0) { e_dir[i] = rev; return; }
    if (e_type[i] == 4 && n > 1 && rng() < 24) bd = opt[rng() % n];
    e_dir[i] = bd;
}
```

## The beat

Lesson 10's beat, a little more general: the real song is much longer (`SONG_LEN`), the beat is still a count of 256ths of a vsync
(`BEAT_FP`), and the head nods twice per beat: once on the beat and once halfway, where the snare is.

```c
static void beat_tick(void)
{
    if (beat_on) {
        beat_acc += 256; song_t += 2;
        if (beat_acc >= BEAT_FP) beat_acc -= BEAT_FP;
        if (song_t >= SONG_LEN) { song_t -= SONG_LEN; beat_acc = BEAT_START + song_t * 128u; }      /* the song starts again, and so does the grid */
    }
    bob = (beat_on && (state == ST_PLAY || state == ST_PAUSE) && (beat_acc < BEAT_WIN || (beat_acc >= BEAT_FP / 2 && beat_acc < BEAT_FP / 2 + BEAT_WIN)));
}
```

## Saving

```c
unsigned char save_peek(unsigned int off);   /* fixed-bank helper, defined at the end */

/* every vsync, counted by the NMI handler in shared audio RAM so none are lost while the draw queue's RAM bank is mapped */
#define vsync_raw (*(volatile unsigned char*)0x3210)

/* Code banks: PROG0 = gameplay, PROG1 = scenes. Calls between them go through bank_call (fixed bank),
 * which the compiler inserts for every function declared inside a wrapped-call block. */
void bank_call(void);
#pragma wrapped-call (push, bank_call, BANK_PROG1)
static void title_scene(void);
static void over_scene(void);
static void win_scene(void);
static void attract_scene(void);
static void intro_scene(void);
static unsigned char intro_for_level(unsigned char lv);
#pragma wrapped-call (pop)
#pragma wrapped-call (push, bank_call, BANK_PROG2)       /* PROG2 = the enemies: behaviour, contact and drawing */
static void enemies_update_all(void);
static unsigned char enemies_touch_player(void);
static void enemies_draw_all(void);
#pragma wrapped-call (pop)
#pragma wrapped-call (push, bank_call, BANK_PROG0)
static void draw_field(void);
static void need_page(SpriteSlot* slot, const SpritePage* page);
#pragma wrapped-call (pop)

/* 0 = tunnel, 1 = dirt, 2 = boulder cell.  16-wide rows so index = r<<4|c.
 * Columns 14/15 and row 13 are permanent solid padding. */
unsigned char map[(ROWS + 1) * 16];
unsigned char paid[(ROWS + 1) * 16];         /* tens of points Doug was paid for digging each cell (0 = none), so the Groundskeeper takes back exactly that */
#define M(c, r) map[(((unsigned char)(r)) << 4) | ((unsigned char)(c))]

unsigned char state, state_timer, frame_ct;
unsigned char level, lives;
unsigned int score_h, hi_h, next_life_h;   /* score in hundreds */
unsigned char score_t, hi_t;               /* the tens digit of the score and of the best score */
char score_str[9], hi_str[9];
unsigned char score_dirty;
unsigned int lfsr = 0xACE1u;
unsigned int run_seed = 0x1357u;      /* new every game: the cave layouts differ from run to run */

/* player */
unsigned char px, py, pdir, panim, pmoving;
unsigned char ball_on, ball_x, ball_y, ball_dir, ball_dist, throw_cd, ball_dirt;   /* Doug's thrown baseball */

/* enemies (structure of arrays: cheaper on a 6502) */
unsigned char e_state[MAXE], e_type[MAXE], e_x[MAXE], e_y[MAXE], e_dir[MAXE], e_face[MAXE];
unsigned char e_infl[MAXE], e_timer[MAXE], e_acc[MAXE], e_homec[MAXE], e_homer[MAXE], e_flen[MAXE];
unsigned char pmv;                           /* ticks left in which Doug counts as moving (for his head bob) */
unsigned char e_mv[MAXE];                    /* ticks left in which an enemy counts as moving (for its head bob) */
unsigned char e_flee[MAXE], e_pts[MAXE];      /* running for the top (the last two enemies), and what it would cost if they get there (hundreds) */
unsigned char e_prevc[MAXE], e_prevr[MAXE];

/* headstones of struck-out Vumpires; a living Vumpire that reaches one raises it again */
#define MAXC 3
unsigned char c_on[MAXC], c_slot[MAXC], c_x[MAXC], c_y[MAXC];
unsigned char enemies_left, espeed;

/* boulders */
unsigned char r_on[MAXR], r_c[MAXR], r_r[MAXR], r_y[MAXR], r_state[MAXR], r_timer[MAXR], r_kills[MAXR];

/* score popups */
#define MAXP 4
unsigned char pop_t[MAXP], pop_x[MAXP], pop_y[MAXP];
unsigned int pop_v[MAXP];
unsigned int beat_acc, song_t;                /* where the music's beat is (see beat_tick) */
unsigned char beat_on, bob;                   /* bob: the player and the walkers nod their heads while it is 1 */
unsigned char gold_on, gold_c, gold_r;      /* a gold bar lying in one enemy cave: 500 points */

static void add_popup(unsigned char x, unsigned char y, unsigned int v)
{
    unsigned char i, best = 0, oldest = 255;
    for (i = 0; i < MAXP; ++i)
        if (pop_t[i] == 0) { best = i; break; }
        else if (pop_t[i] < oldest) { oldest = pop_t[i]; best = i; }
    pop_x[best] = x; pop_y[best] = y; pop_v[best] = v; pop_t[best] = 28;
}
```

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
