/*
 * Lesson 3: moving Doug.
 *
 * Read the d-pad and walk Doug around a field that is a grid of 8x8 cells, one pixel per tick. He may only turn
 * at the middle of a cell, so a sideways push while he is between cells slides him onto the next lane first.
 */
#include "gt/gametank.h"
#include "gt/input.h"
#include "gt/gfx/draw_queue.h"
#include "gt/gfx/sprites.h"
#include "gen/assets/spr.h"
#include "art.h"

#define COLS 14                  /* the playfield is 14 cells across and 13 down, each 8x8 pixels */
#define ROWS 13
#define FX 8                     /* where the field starts on the screen (the screen is 128x128) */
#define FY 16

#define COL_SKY 3
#define COL_DIRT 61              /* an amber from the palette */

enum { DIR_R, DIR_L, DIR_U, DIR_D, DIR_NONE = 255 };
static const signed char DX[4] = { 1, -1, 0, 0 };     /* the step for each direction */
static const signed char DY[4] = { 0, 0, -1, 1 };

SpriteSlot slot_spr;
unsigned char frame_ct;

/* Doug's position is in pixels, measured from the top left of the field: (48, 16) is the middle of the top row's shaft. */
unsigned char px = 6 * 8, py = 2 * 8, pdir = DIR_D, panim, pmoving;

/* Can Doug take one more step this way? Only the edge of the field stops him. */
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
    unsigned char want = DIR_NONE, moved = 0, misal, d;

    update_inputs();                                   /* reads both gamepads into player1_buttons and so on */
    /* one direction at a time: the d-pad can report two at once, and the first one listed wins */
    if (player1_buttons & INPUT_MASK_LEFT) want = DIR_L;
    else if (player1_buttons & INPUT_MASK_RIGHT) want = DIR_R;
    else if (player1_buttons & INPUT_MASK_UP) want = DIR_U;
    else if (player1_buttons & INPUT_MASK_DOWN) want = DIR_D;

    if (want != DIR_NONE) {
        if ((want >> 1) != (pdir >> 1)) {
            /* turning a corner (left/right <-> up/down). Doug can only do that in the middle of a cell, so how far is he
             * from the lane he wants? (py & 7 is how many pixels he is below the top of a cell.) */
            misal = (want < 2) ? (py & 7) : (px & 7);
            if (misal) {
                /* not lined up yet: keep walking to the nearer lane, and do not turn this tick */
                if (want < 2) d = (misal <= 4) ? DIR_U : DIR_D;
                else          d = (misal <= 4) ? DIR_L : DIR_R;
                if (can_move(d)) { px += DX[d]; py += DY[d]; moved = 1; }
                want = DIR_NONE;
            } else {
                pdir = want;                           /* lined up: turn */
            }
        } else {
            pdir = want;                               /* straight on, or turning right round */
        }
    }

    if (want != DIR_NONE && can_move(pdir)) {
        px += DX[pdir]; py += DY[pdir];
        moved = 1;
    }
    pmoving = moved;
    if (moved) ++panim;
}

void draw(void)
{
    unsigned char f = pmoving ? ((panim >> 2) & 1) : 0;      /* swap the walking frame every 4 steps */

    queue_clear_screen(COL_SKY);
    queue_draw_box(FX, FY, COLS * 8, ROWS * 8, COL_DIRT);   /* the field */
    queue_draw_sprite(FX + px, FY + py, SPR_W, SPR_W, doug_x[pdir * 2 + f], doug_y[pdir * 2 + f], slot_spr);
    queue_clear_border(0);
}

void main(void)
{
    slot_spr = allocate_sprite(&ASSET__spr__spr_bmp_load_list);

    while (1) {
        ++frame_ct;
        update_player();
        draw();
        await_draw_queue();
        await_vsync(2);        /* 30 frames a second: Dug Out runs at this pace, so everything here moves at its speed */
        flip_pages();
    }
}
