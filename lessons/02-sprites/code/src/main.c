/*
 * Lesson 2: sprites.
 *
 * Draws Doug (facing right, left, up and down), a Vumpire and a gold bar from a sprite sheet, with the walking frames animated.
 */
#include "gt/gametank.h"
#include "gt/gfx/draw_queue.h"
#include "gt/gfx/sprites.h"
#include "gen/assets/spr.h"      /* made by "make import" from assets/spr/spr.bmp: the name of the sheet in C */
#include "art.h"                 /* made by art/make_art.py: where each frame sits on the sheet */

#define COL_BG 3                 /* a mid grey from the palette: (hue 0, brightness 3) */

SpriteSlot slot_spr;             /* which of the GameTank's sprite memory pages holds our sheet */
unsigned char frame_ct;          /* counts frames, so that things can animate */

/* Draw the 8x8 piece of the sheet at (gx, gy) with its top left corner at (x, y) on the screen. */
void sprite(unsigned char x, unsigned char y, unsigned char gx, unsigned char gy)
{
    queue_draw_sprite(x, y, SPR_W, SPR_W, gx, gy, slot_spr);
}

void main(void)
{
    unsigned char d, f;

    /* copy the sheet from the cartridge into sprite memory (this takes a moment, so do it before the loop) */
    slot_spr = allocate_sprite(&ASSET__spr__spr_bmp_load_list);

    while (1) {
        ++frame_ct;
        queue_clear_screen(COL_BG);

        /* Doug facing each way. The walking frame flips every 8 frames: frame number = direction * 2 + step */
        f = (frame_ct >> 3) & 1;
        for (d = 0; d < 4; ++d)
            sprite(20 + d * 20, 40, doug_x[d * 2 + f], doug_y[d * 2 + f]);

        /* the same thing for a Vumpire (it only has left and right) */
        for (d = 0; d < 2; ++d)
            sprite(30 + d * 20, 70, vamp_x[d * 2 + f], vamp_y[d * 2 + f]);

        /* and a gold bar, which does not move */
        sprite(90, 70, GOLD_GX, GOLD_GY);

        queue_clear_border(0);
        await_draw_queue();
        await_vsync(1);
        flip_pages();
    }
}
