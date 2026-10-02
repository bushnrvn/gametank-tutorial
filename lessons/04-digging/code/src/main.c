/*
 * Lesson 4: digging.
 *
 * The field becomes a map of dirt (1) and tunnel (0). Doug digs the dirt he walks into, slowly, and is paid for it:
 * 10 points a cell near the surface, up to 40 at the bottom. The score is drawn with the SDK's text module.
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

/* palette numbers (found with tools/palette.py): four layers of dirt that cool with depth, and the tunnel colour */
#define COL_SKY 3
#define COL_VOID 136
#define COL_RIM 215
static const unsigned char LAYER[4] = { 61, 91, 114, 146 };

enum { DIR_R, DIR_L, DIR_U, DIR_D, DIR_NONE = 255 };
static const signed char DX[4] = { 1, -1, 0, 0 };
static const signed char DY[4] = { 0, 0, -1, 1 };
static const unsigned char LEADX[4] = { 8, 0, 4, 4 };  /* which pixel of Doug's 8x8 is at his front, for each direction */
static const unsigned char LEADY[4] = { 4, 4, 0, 8 };

/* The map: 0 = tunnel, 1 = dirt. Each row is 16 entries wide although the field is 14 cells across, so that the
 * index of the cell at column c, row r is simply (r << 4) | c: a shift and an or, which a 6502 does far faster than a multiply.
 * The extra two columns and the extra row are never dug: they are solid padding, so looking one cell past the edge is safe. */
unsigned char map[(ROWS + 1) * 16];
#define M(c, r) map[(((unsigned char)(r)) << 4) | ((unsigned char)(c))]

SpriteSlot slot_spr, slot_font;
unsigned char frame_ct;
unsigned char px = 6 * 8, py = 2 * 8, pdir = DIR_D, panim, pmoving;
unsigned int score;

void new_map(void)
{
    unsigned char r, c;
    for (r = 0; r < ROWS + 1; ++r)
        for (c = 0; c < 16; ++c)
            M(c, r) = (r == 0 && c < COLS) ? 0 : 1;     /* dirt everywhere except row 0, the surface */
    M(6, 1) = 0; M(6, 2) = 0;                            /* and Doug's shaft */
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
        /* what is in front of him? dirt slows Doug to every other tick */
        c = (px + LEADX[pdir]) >> 3;
        r = (py + LEADY[pdir]) >> 3;
        slow = (M(c, r) == 1);
        if (!slow || (frame_ct & 1)) {
            px += DX[pdir]; py += DY[pdir];
            moved = 1;
        }
    }
    pmoving = moved;

    if (moved) {
        ++panim;
        /* dig the cell under the middle of Doug, and get paid by depth: rows 0-3 pay 10, 4-6 pay 20, 7-9 pay 30, the rest 40 */
        c = (px + 4) >> 3; r = (py + 4) >> 3;
        if (M(c, r) == 1) {
            M(c, r) = 0;
            pay = (r <= 3) ? 1 : (r <= 6) ? 2 : (r <= 9) ? 3 : 4;
            score += pay * 10;
        }
    }
}

/* the dirt layer a row belongs to, 0 to 3 */
unsigned char layer_of(unsigned char r) { return (r <= 3) ? 0 : (r <= 6) ? 1 : (r <= 9) ? 2 : 3; }

void draw_field(void)
{
    unsigned char r, c, start, y;

    /* the layers: four big boxes. Drawing the whole field one cell at a time would use up the draw queue (it holds 250 jobs) */
    queue_draw_box(FX, FY,          COLS * 8, 4 * 8, LAYER[0]);
    queue_draw_box(FX, FY + 4 * 8,  COLS * 8, 3 * 8, LAYER[1]);
    queue_draw_box(FX, FY + 7 * 8,  COLS * 8, 3 * 8, LAYER[2]);
    queue_draw_box(FX, FY + 10 * 8, COLS * 8, 3 * 8, LAYER[3]);

    /* the tunnels: one box for each run of open cells in a row */
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

void draw_score(void)
{
    char digits[6];
    unsigned int n = score;
    unsigned char i;
    for (i = 5; i > 0; --i) { digits[i - 1] = '0' + (n % 10); n /= 10; }
    digits[5] = 0;
    direct_prepare_sprite_mode(slot_font);            /* switch the blitter from the draw queue's way of working to "direct" mode */
    text_cursor_x = 8; text_cursor_y = 8;              /* the top 7 rows of the screen are border: nothing there shows, so start below it */
    text_color = TEXT_COLOR_WHITE;
    text_print_string("SCORE ");
    text_print_string(digits);
}

void main(void)
{
    unsigned char f;

    /* Load the font FIRST. A 128x128 sheet takes one quarter of a sprite page, and the slot number says which quarter; the SDK's
     * text routine copies the font's slot number into a hardware register without masking that off, so it only works when the
     * font is in the first quarter, which is where the first sheet loaded goes. */
    text_init();
    slot_font = text_load_font();
    slot_spr = allocate_sprite(&ASSET__spr__spr_bmp_load_list);
    new_map();

    while (1) {
        ++frame_ct;
        update_player();

        queue_clear_screen(COL_SKY);
        draw_field();
        f = pmoving ? ((panim >> 2) & 1) : 0;
        queue_draw_sprite(FX + px, FY + py, SPR_W, SPR_W, doug_x[pdir * 2 + f], doug_y[pdir * 2 + f], slot_spr);
        queue_clear_border(0);

        await_draw_queue();         /* the SDK's text routine draws straight away, so wait until the queue is done first */
        draw_score();
        await_vsync(2);
        flip_pages();
    }
}
