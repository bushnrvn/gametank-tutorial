/*
 * Lesson 10: finishing touches.
 *
 * Two things that make a game feel alive: a treasure to hunt for (a gold bar worth 500 points, in a random cave each game), and
 * animation tied to the music: while they walk, Doug and the Vumpires nod their heads on every beat of the tune.
 *
 * (Lesson 9, game flow, and lesson 8, banks, come before this; their comments follow.)
 * Lesson 9: game flow.
 *
 * A title screen, the game, a short pause when Doug is caught, and a game over screen, as a small state machine. The best score is
 * saved in the cartridge's flash memory, so it is still there the next time the console is switched on.
 *
 * (The text below is the lesson 8 comment about banks, which still applies.)
 * Lesson 8: banks.
 *
 * The same game as lesson 7, laid out differently in the cartridge. The GameTank's processor sees 64K of addresses, but the game is
 * on a cartridge of up to 2M, so the cartridge is cut into 16K "banks" and one of them at a time is switched into the window at
 * $8000-$BFFF. Code can live in any bank; the top 16K ($C000-$FFFF, the "fixed bank") is always visible.
 *
 *   fixed bank   main(), the draw queue and other SDK code, and the small helpers every bank calls
 *   PROG0        the game itself
 *   PROG1        the enemies (their thinking, and finding the way to Doug)
 *
 * Calling a function that lives in another bank needs the right bank switched in first; the compiler does that for us for every
 * function declared in a "wrapped-call" block (see below), using bank_call.s.
 */
#include "gt/gametank.h"
#include "gt/input.h"
#include "gt/gfx/draw_queue.h"
#include "gt/gfx/sprites.h"
#include "gt/gfx/draw_direct.h"
#include "gt/feature/text/text.h"
#include "gt/audio/music.h"
#include "gt/feature/persist/persist.h"   /* clear_save_sector() and save_write(): rewriting a sector of the cartridge's flash memory */
#include "gen/assets/spr.h"
#include "gen/assets/audio.h"          /* made by "make import": the songs and effects in assets/audio, by name */
#include "art.h"
#include "gt/banking.h"
#include "gen/bank_nums.h"        /* BANK_PROG0, BANK_PROG1...: made by "make import" from "progbanks" in project.json */

/* The SDK keeps no count of vsyncs, so lesson 7 adds one (see sdk-patches and tools/sdk_patch.py): the NMI handler adds one to the byte
 * at $3210 on every vsync, whatever the game is doing. music_poll() looks at how many arrived since it last looked and gives the music
 * player that many ticks, so the tune keeps time even when a frame takes longer than planned. It returns the number of vsyncs. */
#define vsync_raw (*(volatile unsigned char *)0x3210)
unsigned char music_seen;
unsigned char music_poll(void)
{
    unsigned char n = vsync_raw - music_seen, k;
    music_seen += n;
    for (k = n; k; --k) tick_music();
    return n;
}

#define COLS 14
#define ROWS 13
#define FX 8
#define FY 16
#define MAXE 3                   /* how many enemies */
#define MAXP 2                   /* how many score popups can be on screen at once */

/* Sound effects all play on one of the console's four sound channels (channel 3), so a new effect cuts off the one before it.
 * The priority (0-15) decides who wins: an effect only starts if its priority is at least that of the one already playing. */
#define SFX_CH 3
#define SFX(id, priority) play_sound_effect(id, SFX_CH | SFX_PRIORITY(priority))

#define COL_SKY 3
#define COL_VOID 136
#define COL_RIM 215
static const unsigned char LAYER[4] = { 61, 91, 114, 146 };

enum { DIR_R, DIR_L, DIR_U, DIR_D, DIR_NONE = 255 };
enum { ES_NONE, ES_WALK, ES_INFL, ES_POP };          /* an enemy's state: gone, walking, stunned by a strike, or just put out */
enum { ST_TITLE, ST_PLAY, ST_DYING, ST_OVER };         /* what the whole game is doing right now */
static const signed char DX[4] = { 1, -1, 0, 0 };
static const signed char DY[4] = { 0, 0, -1, 1 };
static const unsigned char LEADX[4] = { 8, 0, 4, 4 };
static const unsigned char LEADY[4] = { 4, 4, 0, 8 };

unsigned char map[(ROWS + 1) * 16];
#define M(c, r) map[(((unsigned char)(r)) << 4) | ((unsigned char)(c))]

SpriteSlot slot_spr, slot_font;
unsigned char frame_ct;
unsigned char px, py, pdir, panim, pmoving;
unsigned int score;
unsigned char lives;
unsigned char state, state_timer;
unsigned int best_score, saved_best;                   /* the best score this session, and what is in the cartridge */
unsigned char new_best;                                /* did this game beat the best score? */
unsigned char sbuf[5];                                 /* the bytes that get saved */

/* Doug's thrown baseball */
unsigned char ball_on, ball_x, ball_y, ball_dir, ball_dist, throw_cd;

/* score popups: the points scored, shown for a moment where an enemy went out */
unsigned char pop_t[MAXP], pop_x[MAXP], pop_y[MAXP];
unsigned int pop_v[MAXP];

/* The enemies. Instead of an array of structs, one array per field. On a 6502 that is smaller and faster: finding element i of
 * an array of bytes is just "base + i", whereas a struct needs a multiply to get from i to the start of its record. */
unsigned char e_x[MAXE], e_y[MAXE], e_dir[MAXE], e_face[MAXE], e_acc[MAXE];
unsigned char e_state[MAXE], e_infl[MAXE], e_timer[MAXE];     /* state, strikes so far, and a counter for the current state */
unsigned char enemies_left;
unsigned char e_mv[MAXE];        /* ticks left in which an enemy counts as moving (for its head bob) */
unsigned char pmv;               /* the same for Doug */
unsigned char gold_on, gold_c, gold_r;                 /* the gold bar: is it there, and which cell */

/* The beat. The tune is 126 beats a minute and the game counts time in vsyncs (60 a second), so a beat is 3600/126 = 28.57 vsyncs.
 * beat_acc counts how far into the current beat we are, in 256ths of a vsync (a fraction, kept as a whole number: a 6502 has no
 * decimals). The heads drop for the first part of every beat, and start from the top again when the song does. */
#define BEAT_FP 7314u            /* 28.57 * 256 */
#define NOD_FP 1536u             /* the head stays down for 6 vsyncs */
#define SONG_VSYNCS 914u         /* 32 beats: the tune is 8 bars of 4 */
unsigned int beat_acc, song_t;
unsigned char bob;               /* 1 while the heads are down */
unsigned char vs;                /* how many vsyncs went by during the last frame */
static const unsigned char CAVE_C[MAXE] = { 2, 8, 3 };    /* each enemy starts in a sealed cave: first column, row, width */
static const unsigned char CAVE_R[MAXE] = { 5, 8, 11 };
#define CAVE_W 4
#define ESPEED 9                 /* each tick an enemy adds this to its counter, and steps when it reaches 16: about half a pixel a tick */

unsigned int lfsr = 0xACE1u;     /* a tiny random number generator (a 16-bit linear feedback shift register) */



unsigned char save_peek(unsigned int off);              /* reads a byte of the saved data: defined at the end of the file, in the fixed bank */

/* ----------------------------------------------------------------- banks -- */
/* The functions below live in PROG1. Declaring them inside a wrapped-call block makes the compiler send every call to them
 * through bank_call (in bank_call.s, in the fixed bank), which switches PROG1 in, makes the call, and switches back. */
void bank_call(void);
#pragma wrapped-call (push, bank_call, BANK_PROG1)
void update_enemies(void);
unsigned char enemy_touches_doug(void);
#pragma wrapped-call (pop)

/* The helpers that every bank uses stay in the fixed bank (the default for code with no pragma), so they can be called from
 * anywhere without switching. */
unsigned char rng(void)
{
    lfsr = (lfsr >> 1) ^ (-(lfsr & 1) & 0xB400u);
    return (unsigned char)lfsr;
}

unsigned char absdiff(unsigned char a, unsigned char b) { return a > b ? a - b : b - a; }

/* Is this cell on the map and open? The casts matter: a column of -1 must not be mistaken for 255. */
unsigned char open_cell(signed char c, signed char r)
{
    if (c < 0 || c >= COLS || r < 0 || r >= ROWS) return 0;
    return M(c, r) == 0;
}

#pragma code-name (push, "PROG1")      /* from here until the matching pop, functions go into the PROG1 bank */

/* -------------------------------------------------- finding the way: BFS -- */

/* A breadth first search from the enemy's cell: look at every neighbour, then their neighbours, and so on, ring by ring, so the
 * first time the search reaches the goal it has found a shortest way. For every cell it reaches it remembers which step led there
 * (1 to 4), and when it gets to the goal it walks that trail back to find the very first step. */
unsigned char trail[(ROWS + 1) << 4];     /* 0 = not reached yet, otherwise 1 + the direction of the step that reached this cell */
unsigned char queue[(ROWS + 1) << 4];     /* the cells still to look at */

unsigned char bfs_dir(unsigned char i, unsigned char goal)
{
    unsigned char head = 0, tail = 0, cur, c, r, d, nc, nr, n, start, first = 0;
    for (n = 0; n < sizeof trail; ++n) trail[n] = 0;
    start = ((e_y[i] >> 3) << 4) | (e_x[i] >> 3);
    queue[tail++] = start; trail[start] = 5;
    while (head != tail) {
        cur = queue[head++]; c = cur & 15; r = cur >> 4;
        if (cur == goal && cur != start) {
            while (cur != start) {                       /* walk the trail back to the start */
                d = trail[cur] - 1; first = d;
                cur = (unsigned char)(((((signed char)(cur >> 4)) - DY[d]) << 4) | (((signed char)(cur & 15)) - DX[d]));
            }
            e_dir[i] = first;
            return 1;
        }
        for (d = 0; d < 4; ++d) {
            nc = c + DX[d]; nr = r + DY[d];
            if (!open_cell((signed char)nc, (signed char)nr)) continue;
            n = (nr << 4) | nc;
            if (trail[n]) continue;
            trail[n] = d + 1;
            queue[tail++] = n;
        }
    }
    return 0;
}

/* An enemy is exactly on a cell: pick its next direction. */
void choose_dir(unsigned char i)
{
    unsigned char c = e_x[i] >> 3, r = e_y[i] >> 3, d, n = 0, opt[4];
    unsigned char goal = (((py + 4) >> 3) << 4) | ((px + 4) >> 3);

    if (bfs_dir(i, goal)) return;                        /* there is a way to Doug: take it */

    /* no way (the cave is sealed): wander. Pick any open neighbour except the one behind, or turn back at a dead end. */
    for (d = 0; d < 4; ++d)
        if (d != (e_dir[i] ^ 1) && open_cell((signed char)(c + DX[d]), (signed char)(r + DY[d]))) opt[n++] = d;
    if (n) e_dir[i] = opt[rng() % n];
    else   e_dir[i] = e_dir[i] ^ 1;
}

void update_enemies(void)
{
    unsigned char i, d;
    for (i = 0; i < MAXE; ++i) {
        if (e_mv[i]) --e_mv[i];
        if (e_state[i] == ES_INFL) {                     /* stunned: frozen, until the strike wears off */
            if (++e_timer[i] > 90) {
                e_timer[i] = 0;
                if (--e_infl[i] == 0) e_state[i] = ES_WALK;
            }
            continue;
        }
        if (e_state[i] == ES_POP) {                      /* just put out: show it for a few ticks, then it is gone */
            if (++e_timer[i] > 8) e_state[i] = ES_NONE;
            continue;
        }
        if (e_state[i] != ES_WALK) continue;
        e_acc[i] += ESPEED;
        if (e_acc[i] < 16) continue;                     /* not this tick */
        e_acc[i] -= 16;
        if (!((e_x[i] | e_y[i]) & 7)) choose_dir(i);     /* only on a whole cell can an enemy choose where to go */
        d = e_dir[i];
        e_x[i] += DX[d]; e_y[i] += DY[d];
        e_mv[i] = 6;                                     /* it moved: it counts as walking for the next 6 ticks (a step comes only every other tick) */
        if (d < 2) e_face[i] = d;
    }
}

/* Does any enemy overlap Doug? */
unsigned char enemy_touches_doug(void)
{
    unsigned char i;
    for (i = 0; i < MAXE; ++i)
        if ((e_state[i] == ES_WALK || e_state[i] == ES_INFL) && absdiff(e_x[i], px) < 6 && absdiff(e_y[i], py) < 6) return 1;
    return 0;
}

#pragma code-name (pop)

#pragma code-name (push, "PROG0")      /* ...and these go into PROG0 */

/* ------------------------------------------------------------------ map -- */
void new_map(void)
{
    unsigned char r, c, i;
    for (r = 0; r < ROWS + 1; ++r)
        for (c = 0; c < 16; ++c)
            M(c, r) = (r == 0 && c < COLS) ? 0 : 1;
    M(6, 1) = 0; M(6, 2) = 0;
    for (i = 0; i < MAXE; ++i)                          /* carve the caves */
        for (c = 0; c < CAVE_W; ++c)
            M(CAVE_C[i] + c, CAVE_R[i]) = 0;
    i = rng() % MAXE;                                   /* the gold bar: a random cell of a random cave */
    gold_c = CAVE_C[i] + rng() % CAVE_W; gold_r = CAVE_R[i]; gold_on = 1;
}

/* Is this cell on the map and open? The casts matter: a column of -1 must not be mistaken for 255. */

/* Sets e_dir[i] to the first step of a shortest way from enemy i to the cell `goal` (row * 16 + column) and returns 1,
 * or returns 0 if the tunnels do not lead there. */

/* An enemy is exactly on a cell: pick its next direction. */


/* Does any enemy overlap Doug? Sprites are 8x8; counting a difference of less than 6 pixels in both directions as a hit
 * means they must overlap by 3 pixels or more, which feels fair. */

/* ------------------------------------------------- throwing and striking -- */
void add_popup(unsigned char x, unsigned char y, unsigned int v)
{
    unsigned char i, best = 0, oldest = 255;
    for (i = 0; i < MAXP; ++i)                           /* use a free slot, or else replace the oldest popup */
        if (pop_t[i] == 0) { best = i; break; }
        else if (pop_t[i] < oldest) { oldest = pop_t[i]; best = i; }
    pop_x[best] = x; pop_y[best] = y; pop_v[best] = v; pop_t[best] = 28;
}

/* A baseball hit enemy k. Three strikes and it is out; the points depend on how deep it was. */
void strike(unsigned char k)
{
    unsigned char row = e_y[k] >> 3;
    unsigned int pts;
    e_state[k] = ES_INFL;                                /* stunned */
    SFX(ASSET__audio__hit_sfx_ID, 2);
    e_timer[k] = 0;
    ++e_infl[k];
    if (e_infl[k] >= 3) {
        pts = (row <= 3) ? 200 : (row <= 6) ? 300 : (row <= 9) ? 400 : 500;
        e_state[k] = ES_POP; e_timer[k] = 0;
        SFX(ASSET__audio__out_sfx_ID, 3);                /* louder than the hit it follows, so this one wins */
        score += pts;
        add_popup(e_x[k], e_y[k], pts);
        --enemies_left;
    }
}

/* Move the ball. It goes 4 pixels a tick, as two steps of 2 so that it cannot jump clean over an enemy. */
void ball_step(void)
{
    unsigned char s, k;
    for (s = 0; s < 2 && ball_on; ++s) {
        ball_x += DX[ball_dir] * 2; ball_y += DY[ball_dir] * 2;
        ball_dist += 2;
        if (ball_dist > 40 || ball_x >= COLS * 8 || ball_y >= ROWS * 8) { ball_on = 0; break; }   /* out of range, or off the field */
        if (M(ball_x >> 3, ball_y >> 3) != 0) { ball_on = 0; break; }                              /* the ball stops at dirt */
        for (k = 0; k < MAXE; ++k) {
            if (e_state[k] != ES_WALK && e_state[k] != ES_INFL) continue;
            /* ball_x/ball_y are the middle of the ball; an enemy's middle is 4 pixels in from its corner */
            if (absdiff(e_x[k] + 4, ball_x) < 6 && absdiff(e_y[k] + 4, ball_y) < 6) {
                strike(k);
                ball_on = 0;
                break;
            }
        }
    }
}

/* ---------------------------------------------------------------- Doug -- */
void reset_positions(void)
{
    unsigned char i;
    px = 6 * 8; py = 2 * 8; pdir = DIR_D; panim = 0;
    for (i = 0; i < MAXE; ++i) {
        e_x[i] = (CAVE_C[i] + 1) << 3; e_y[i] = CAVE_R[i] << 3;
        e_dir[i] = DIR_R; e_face[i] = DIR_R; e_acc[i] = 0;
        e_state[i] = ES_WALK; e_infl[i] = 0; e_timer[i] = 0; e_mv[i] = 0;
    }
    enemies_left = MAXE;
    ball_on = 0; throw_cd = 0;
}

unsigned char can_move(unsigned char d)
{
    switch (d) {
    case DIR_R: return px < (COLS - 1) * 8;
    case DIR_L: return px > 0;
    case DIR_U: return py > 0;
    default:    return py < (ROWS - 1) * 8;
    }
}

void update_player(void)
{
    unsigned char want = DIR_NONE, moved = 0, misal, d, c, r, slow, pay;

    update_inputs();
    if (player1_buttons & INPUT_MASK_LEFT) want = DIR_L;
    else if (player1_buttons & INPUT_MASK_RIGHT) want = DIR_R;
    else if (player1_buttons & INPUT_MASK_UP) want = DIR_U;
    else if (player1_buttons & INPUT_MASK_DOWN) want = DIR_D;

    if (want != DIR_NONE) {
        if ((want >> 1) != (pdir >> 1)) {
            misal = (want < 2) ? (py & 7) : (px & 7);
            if (misal) {
                if (want < 2) d = (misal <= 4) ? DIR_U : DIR_D;
                else          d = (misal <= 4) ? DIR_L : DIR_R;
                if (can_move(d)) { px += DX[d]; py += DY[d]; moved = 1; }
                want = DIR_NONE;
            } else {
                pdir = want;
            }
        } else {
            pdir = want;
        }
    }
    if (want != DIR_NONE && can_move(pdir)) {
        c = (px + LEADX[pdir]) >> 3;
        r = (py + LEADY[pdir]) >> 3;
        slow = (M(c, r) == 1);
        if (!slow || (frame_ct & 1)) { px += DX[pdir]; py += DY[pdir]; moved = 1; }
    }
    pmoving = moved;
    if (moved) pmv = 4; else if (pmv) --pmv;          /* digging is half speed, so Doug steps every other tick: hold "moving" a few ticks */
    if (moved) {
        ++panim;
        c = (px + 4) >> 3; r = (py + 4) >> 3;
        if (M(c, r) == 1) {
            M(c, r) = 0;
            pay = (r <= 3) ? 1 : (r <= 6) ? 2 : (r <= 9) ? 3 : 4;
            score += pay * 10;
            SFX(ASSET__audio__dig_sfx_ID, 0);
        }
    }

    /* the gold bar: enemies walk over it, but only Doug can pick it up */
    if (gold_on && ((px + 4) >> 3) == gold_c && ((py + 4) >> 3) == gold_r) {
        gold_on = 0;
        score += 500;
        add_popup(px, py, 500);
        SFX(ASSET__audio__out_sfx_ID, 3);
    }

    /* throw: player1_new_buttons has only the buttons that went down THIS frame, so holding A does not machine-gun balls */
    if (throw_cd) --throw_cd;
    if ((player1_new_buttons & INPUT_MASK_A) && !ball_on && !throw_cd) {
        ball_on = 1; ball_dir = pdir; ball_dist = 0;
        ball_x = px + 4; ball_y = py + 4;                /* from the middle of Doug */
        throw_cd = 4;
        SFX(ASSET__audio__throw_sfx_ID, 1);
    }
    if (ball_on) ball_step();
}

/* ---------------------------------------------------------------- draw -- */
void draw_field(void)
{
    unsigned char r, c, start, y;
    queue_draw_box(FX, FY,          COLS * 8, 4 * 8, LAYER[0]);
    queue_draw_box(FX, FY + 4 * 8,  COLS * 8, 3 * 8, LAYER[1]);
    queue_draw_box(FX, FY + 7 * 8,  COLS * 8, 3 * 8, LAYER[2]);
    queue_draw_box(FX, FY + 10 * 8, COLS * 8, 3 * 8, LAYER[3]);
    for (r = 0; r < ROWS; ++r) {
        y = FY + r * 8;
        c = 0;
        while (c < COLS) {
            if (M(c, r) == 0) {
                start = c;
                while (c < COLS && M(c, r) == 0) ++c;
                queue_draw_box(FX + start * 8, y, (c - start) * 8, 8, COL_VOID);
            } else {
                ++c;
            }
        }
    }
}


/* ------------------------------------------------------- the best score -- */
/* The GameTank's cartridge is flash memory, and the SDK's "persist" module can rewrite one sector of it. What we save is five
 * bytes: two marker bytes (so that a cartridge that has never been saved to is recognised), the score, and a check byte.
 * Reading it back is save_peek, which is in the fixed bank (see the bottom of this file for why). */
void load_best(void)
{
    unsigned char lo, hi;
    saved_best = 0;
    if (save_peek(0) != 0x44 || save_peek(1) != 0x55) return;      /* never saved: there is no best score */
    lo = save_peek(2); hi = save_peek(3);
    if (save_peek(4) != (unsigned char)(lo ^ hi ^ 0xA5)) return;   /* damaged: ignore it */
    saved_best = ((unsigned int)hi << 8) | lo;
    best_score = saved_best;
}

void save_best(void)
{
    if (best_score <= saved_best) return;                          /* nothing new to save */
    await_draw_queue();                                            /* the blitter must be idle: the flash write takes a moment */
    sbuf[0] = 0x44; sbuf[1] = 0x55;
    sbuf[2] = (unsigned char)(best_score & 0xFF);
    sbuf[3] = (unsigned char)(best_score >> 8);
    sbuf[4] = sbuf[2] ^ sbuf[3] ^ 0xA5;
    clear_save_sector();                                           /* flash must be erased before it is written */
    save_write(sbuf, (void*)0x8000, 5);                            /* the save sector appears at $8000 */
    saved_best = best_score;
}

/* ------------------------------------------------------------ the states -- */
void new_game(void)
{
    score = 0; lives = 3; new_best = 0;
    new_map();
    reset_positions();
    play_song(ASSET__audio__theme_mid, REPEAT_LOOP);
    beat_acc = 0; song_t = 0;                          /* the tune starts now, and so does the beat */
    state = ST_PLAY; state_timer = 0;
}

void play_tick(void)
{
    update_player();
    update_enemies();
    if (score > best_score) { best_score = score; new_best = 1; }  /* beating the best score counts from the moment you pass it */
    if (enemy_touches_doug()) {
        SFX(ASSET__audio__die_sfx_ID, 4);
        state = ST_DYING; state_timer = 0;
    }
    if (enemies_left == 0) { new_map(); reset_positions(); }       /* every Vumpire is out: a fresh field */
}

/* ---------------------------------------------------------------- drawing -- */
/* A sprite whose head (its top 5 rows) drops one pixel while `nod` is set: draw the body where it belongs, then the head a pixel
 * lower, over it. Two blits instead of one. */
void sprite_nod(unsigned char x, unsigned char y, unsigned char gx, unsigned char gy, unsigned char nod)
{
    if (bob && nod) {
        queue_draw_sprite(x, y + 5, SPR_W, 3, gx, gy + 5, slot_spr);
        queue_draw_sprite(x, y + 1, SPR_W, 5, gx, gy, slot_spr);
    } else {
        queue_draw_sprite(x, y, SPR_W, SPR_W, gx, gy, slot_spr);
    }
}

void draw_scene(void)
{
    unsigned char i, d, f;
    queue_clear_screen(COL_SKY);
    draw_field();
    f = pmoving ? ((panim >> 2) & 1) : 0;
    if (gold_on && M(gold_c, gold_r) == 0)                         /* the gold bar, under everything that moves */
        queue_draw_sprite(FX + gold_c * 8, FY + gold_r * 8, SPR_W, SPR_W, GOLD_GX, GOLD_GY, slot_spr);
    if (state != ST_DYING || (state_timer & 2))                    /* Doug flickers while he is caught */
        sprite_nod(FX + px, FY + py, doug_x[pdir * 2 + f], doug_y[pdir * 2 + f], pmv);
    for (i = 0; i < MAXE; ++i) {
        if (e_state[i] == ES_WALK || e_state[i] == ES_INFL) {
            f = e_face[i] * 2 + ((frame_ct >> 3) & 1);
            sprite_nod(FX + e_x[i], FY + e_y[i], vamp_x[f], vamp_y[f], e_mv[i] && e_state[i] == ES_WALK);
            for (d = 0; d < e_infl[i]; ++d) {
                queue_draw_box(FX + e_x[i] + d * 3, FY + e_y[i] - 3, 2, 2, 7);
            }
        } else if (e_state[i] == ES_POP) {
            queue_draw_box(FX + e_x[i] + 1, FY + e_y[i] + 1, 6, 6, 63);
        }
    }
    if (ball_on) {
        queue_draw_box(FX + ball_x - 2, FY + ball_y - 2, 4, 4, 7);
        queue_draw_box(FX + ball_x - 1, FY + ball_y - 1, 2, 1, 91);
    }
    for (i = 0; i < lives; ++i) {
        queue_draw_sprite(98 + i * 9, 8, SPR_W, SPR_W, doug_x[6], doug_y[6], slot_spr);
    }
    if (state == ST_TITLE || state == ST_OVER) {                   /* a dark panel for the words, drawn over everything else */
        queue_draw_box(12, 22, 104, 90, COL_VOID);
        queue_draw_box(12, 22, 104, 1, COL_RIM);
        queue_draw_box(12, 111, 104, 1, COL_RIM);
    }
}

/* five digits for a number, into a buffer of at least six characters */
void number(char* digits, unsigned int n)
{
    unsigned char i;
    for (i = 5; i > 0; --i) { digits[i - 1] = '0' + (n % 10); n /= 10; }
    digits[5] = 0;
}

void put(unsigned char x, unsigned char y, const char* s)
{
    text_cursor_x = x; text_cursor_y = y;
    text_print_string((char*)s);
}

/* All the text goes on after the draw queue has finished (the SDK's text routine draws immediately). */
void draw_texts(void)
{
    char digits[6];
    unsigned char i;
    direct_prepare_sprite_mode(slot_font);
    text_color = TEXT_COLOR_WHITE;
    switch (state) {
    case ST_TITLE:
        put(36, 28, "DUG OUT");
        put(24, 56, "BEST ");  number(digits, best_score); text_print_string(digits);
        if (frame_ct & 16) put(20, 88, "PRESS START");       /* blinks: on for 16 frames, off for 16 */
        break;
    case ST_OVER:
        put(28, 40, "GAME OVER");
        put(16, 60, "SCORE "); number(digits, score); text_print_string(digits);
        if (new_best) put(28, 76, "NEW BEST!");
        if (frame_ct & 16) put(20, 96, "PRESS START");
        break;
    default:                                                    /* playing, or caught: the score along the top */
        put(8, 8, "SCORE "); number(digits, score); text_print_string(digits);
        for (i = 0; i < MAXP; ++i) {
            if (!pop_t[i]) continue;
            digits[0] = '0' + pop_v[i] / 100; digits[1] = '0'; digits[2] = '0'; digits[3] = 0;
            put(FX + pop_x[i] - 4, FY + pop_y[i] - 8 - (28 - pop_t[i]) / 4, digits);
            --pop_t[i];
        }
    }
}

/* ------------------------------------------------------------- main loop -- */
void game_main(void)
{
    text_init();
    slot_font = text_load_font();                        /* the font first: see lesson 4 */
    slot_spr = allocate_sprite(&ASSET__spr__spr_bmp_load_list);
    load_best();
    new_map();
    reset_positions();
    state = ST_TITLE;

    while (1) {
        ++frame_ct;
        update_inputs();

        switch (state) {
        case ST_TITLE:
            if (player1_new_buttons & INPUT_MASK_START) new_game();
            break;
        case ST_PLAY:
            play_tick();
            break;
        case ST_DYING:                                   /* a short pause, then either another life or the end */
            if (++state_timer > 40) {
                if (--lives == 0) {
                    state = ST_OVER; state_timer = 0;
                    stop_music();
                    save_best();                         /* does nothing unless this game beat the best score */
                } else {
                    reset_positions();
                    state = ST_PLAY;
                }
            }
            break;
        case ST_OVER:
            if (state_timer < 255) ++state_timer;
            if (state_timer > 30 && (player1_new_buttons & INPUT_MASK_START)) { state = ST_TITLE; new_map(); reset_positions(); }
            break;
        }

        draw_scene();
        queue_clear_border(0);
        await_draw_queue();
        draw_texts();
        await_vsync(2);
        vs = music_poll();

        /* move the beat on by the vsyncs that really went by, and wrap it when a beat (or the whole song) is over */
        beat_acc += vs * 256u; song_t += vs;
        while (beat_acc >= BEAT_FP) beat_acc -= BEAT_FP;
        if (song_t >= SONG_VSYNCS) { song_t -= SONG_VSYNCS; beat_acc = song_t * 256u; while (beat_acc >= BEAT_FP) beat_acc -= BEAT_FP; }
        bob = (state == ST_PLAY && beat_acc < NOD_FP);
        flip_pages();
    }
}

#pragma code-name (pop)

/* The only code that has to be in the fixed bank to get things going: switch the game's bank in, and run it. */
void main(void)
{
    change_rom_bank(BANK_PROG0);
    game_main();
}

/* Reads one byte of the saved data. The save sector is in its own ROM bank (BANK_SAVE), which has to be switched into the $8000 window
 * to be read, and that window is where the game's own code (PROG0) is: so this has to run from the fixed bank, which is always there. */
unsigned char save_peek(unsigned int off)
{
    unsigned char v;
    push_rom_bank();
    change_rom_bank(BANK_SAVE);
    v = *((volatile unsigned char*)(0x8000u + off));
    pop_rom_bank();
    return v;
}
