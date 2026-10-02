/*
 * Lesson 6: throwing.
 *
 * Doug throws baseballs (button A). A ball flies straight until it hits dirt, runs out of range, or hits a Vumpire. Each hit is a
 * strike: the Vumpire is stunned (it freezes, and its strike wears off after a while), and on the third strike it is out, which
 * scores by depth. A stunned Vumpire is still dangerous to touch.
 */
#include "gt/gametank.h"
#include "gt/input.h"
#include "gt/gfx/draw_queue.h"
#include "gt/gfx/sprites.h"
#include "gt/gfx/draw_direct.h"
#include "gt/feature/text/text.h"
#include "gen/assets/spr.h"
#include "art.h"

#define COLS 14
#define ROWS 13
#define FX 8
#define FY 16
#define MAXE 3                   /* how many enemies */
#define MAXP 2                   /* how many score popups can be on screen at once */

#define COL_SKY 3
#define COL_VOID 136
static const unsigned char LAYER[4] = { 61, 91, 114, 146 };

enum { DIR_R, DIR_L, DIR_U, DIR_D, DIR_NONE = 255 };
enum { ES_NONE, ES_WALK, ES_INFL, ES_POP };           /* an enemy's state: gone, walking, stunned by a strike, or just put out */
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
static const unsigned char CAVE_C[MAXE] = { 2, 8, 3 };    /* each enemy starts in a sealed cave: first column, row, width */
static const unsigned char CAVE_R[MAXE] = { 5, 8, 11 };
#define CAVE_W 4
#define ESPEED 9                 /* each tick an enemy adds this to its counter, and steps when it reaches 16: about half a pixel a tick */

unsigned int lfsr = 0xACE1u;     /* a tiny random number generator (a 16-bit linear feedback shift register) */
unsigned char rng(void)
{
    lfsr = (lfsr >> 1) ^ (-(lfsr & 1) & 0xB400u);
    return (unsigned char)lfsr;
}

unsigned char absdiff(unsigned char a, unsigned char b) { return a > b ? a - b : b - a; }

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
}

/* Is this cell on the map and open? The casts matter: a column of -1 must not be mistaken for 255. */
unsigned char open_cell(signed char c, signed char r)
{
    if (c < 0 || c >= COLS || r < 0 || r >= ROWS) return 0;
    return M(c, r) == 0;
}

/* -------------------------------------------------- finding the way: BFS -- */
/* A breadth first search from the enemy's cell: look at every neighbour, then their neighbours, and so on, ring by ring, so the
 * first time the search reaches the goal it has found a shortest way. For every cell it reaches it remembers which step led there
 * (1 to 4), and when it gets to the goal it walks that trail back to find the very first step. */
unsigned char trail[(ROWS + 1) << 4];     /* 0 = not reached yet, otherwise 1 + the direction of the step that reached this cell */
unsigned char queue[(ROWS + 1) << 4];     /* the cells still to look at */

/* Sets e_dir[i] to the first step of a shortest way from enemy i to the cell `goal` (row * 16 + column) and returns 1,
 * or returns 0 if the tunnels do not lead there. */
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
        if (d < 2) e_face[i] = d;
    }
}

/* Does any enemy overlap Doug? Sprites are 8x8; counting a difference of less than 6 pixels in both directions as a hit
 * means they must overlap by 3 pixels or more, which feels fair. */
unsigned char enemy_touches_doug(void)
{
    unsigned char i;
    for (i = 0; i < MAXE; ++i)
        if ((e_state[i] == ES_WALK || e_state[i] == ES_INFL) && absdiff(e_x[i], px) < 6 && absdiff(e_y[i], py) < 6) return 1;
    return 0;
}

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
    e_timer[k] = 0;
    ++e_infl[k];
    if (e_infl[k] >= 3) {
        pts = (row <= 3) ? 200 : (row <= 6) ? 300 : (row <= 9) ? 400 : 500;
        e_state[k] = ES_POP; e_timer[k] = 0;
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
        e_state[i] = ES_WALK; e_infl[i] = 0; e_timer[i] = 0;
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
    if (moved) {
        ++panim;
        c = (px + 4) >> 3; r = (py + 4) >> 3;
        if (M(c, r) == 1) {
            M(c, r) = 0;
            pay = (r <= 3) ? 1 : (r <= 6) ? 2 : (r <= 9) ? 3 : 4;
            score += pay * 10;
        }
    }

    /* throw: player1_new_buttons has only the buttons that went down THIS frame, so holding A does not machine-gun balls */
    if (throw_cd) --throw_cd;
    if ((player1_new_buttons & INPUT_MASK_A) && !ball_on && !throw_cd) {
        ball_on = 1; ball_dir = pdir; ball_dist = 0;
        ball_x = px + 4; ball_y = py + 4;                /* from the middle of Doug */
        throw_cd = 4;
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

void draw_hud(void)
{
    char digits[6];
    unsigned int n = score;
    unsigned char i;
    for (i = 5; i > 0; --i) { digits[i - 1] = '0' + (n % 10); n /= 10; }
    digits[5] = 0;
    direct_prepare_sprite_mode(slot_font);
    text_color = TEXT_COLOR_WHITE;
    text_cursor_x = 8; text_cursor_y = 8;
    text_print_string("SCORE ");
    text_print_string(digits);
    for (i = 0; i < MAXP; ++i) {                          /* the popups, drifting up a pixel every 4 ticks */
        if (!pop_t[i]) continue;
        n = pop_v[i];
        digits[0] = '0' + n / 100; digits[1] = '0'; digits[2] = '0'; digits[3] = 0;
        text_cursor_x = FX + pop_x[i] - 4; text_cursor_y = FY + pop_y[i] - 8 - (28 - pop_t[i]) / 4;
        text_print_string(digits);
        --pop_t[i];
    }
}

void main(void)
{
    unsigned char i, d, f;

    text_init();
    slot_font = text_load_font();                        /* the font first: see lesson 4 */
    slot_spr = allocate_sprite(&ASSET__spr__spr_bmp_load_list);
    new_map();
    lives = 3;
    reset_positions();

    while (1) {
        ++frame_ct;
        update_player();
        update_enemies();
        if (enemy_touches_doug()) {                      /* caught: one life less, and everyone goes back to where they started */
            if (--lives == 0) { lives = 3; score = 0; new_map(); }
            reset_positions();
        }
        if (enemies_left == 0) {                         /* every Vumpire is out: a fresh field (the score carries over) */
            new_map();
            reset_positions();
        }

        queue_clear_screen(COL_SKY);
        draw_field();
        f = pmoving ? ((panim >> 2) & 1) : 0;
        queue_draw_sprite(FX + px, FY + py, SPR_W, SPR_W, doug_x[pdir * 2 + f], doug_y[pdir * 2 + f], slot_spr);
        /* The braces are not optional. queue_draw_sprite is a macro that expands to FOUR statements; without braces only the first would
         * be repeated by the loop and the rest would run once, afterwards. (Dug Out wraps its own version in do { ... } while (0).) */
        for (i = 0; i < MAXE; ++i) {
            if (e_state[i] == ES_WALK || e_state[i] == ES_INFL) {
                f = e_face[i] * 2 + ((frame_ct >> 3) & 1);
                queue_draw_sprite(FX + e_x[i], FY + e_y[i], SPR_W, SPR_W, vamp_x[f], vamp_y[f], slot_spr);
                for (d = 0; d < e_infl[i]; ++d) {         /* a little white box above its head for each strike */
                    queue_draw_box(FX + e_x[i] + d * 3, FY + e_y[i] - 3, 2, 2, 7);
                }
            } else if (e_state[i] == ES_POP) {             /* put out: a flash of gold */
                queue_draw_box(FX + e_x[i] + 1, FY + e_y[i] + 1, 6, 6, 63);
            }
        }
        if (ball_on) {                                    /* the ball is not a sprite at all: two boxes, a white ball with a red seam */
            queue_draw_box(FX + ball_x - 2, FY + ball_y - 2, 4, 4, 7);
            queue_draw_box(FX + ball_x - 1, FY + ball_y - 1, 2, 1, 91);
        }
        for (i = 0; i < lives; ++i) {                     /* one little Doug for each life left */
            queue_draw_sprite(98 + i * 9, 8, SPR_W, SPR_W, doug_x[6], doug_y[6], slot_spr);
        }
        queue_clear_border(0);

        await_draw_queue();
        draw_hud();
        await_vsync(2);
        flip_pages();
    }
}
