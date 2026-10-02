/*
 * Lesson 1: the screen.
 *
 * The smallest game loop there is: a box bounces around a grey screen. Everything later in the tutorial is built on this loop.
 */
#include "gt/gametank.h"
#include "gt/gfx/draw_queue.h"

char box_x = 30, box_y = 20;     /* where the box is (its top left corner) */
char dx = 1, dy = 1;             /* which way it is going: one pixel a frame */

void main(void)
{
    while (1) {
        /* Describe the picture. Nothing is drawn yet: these calls only add jobs to the "draw queue". */
        queue_clear_screen(3);                  /* fill the screen with palette colour 3, a mid grey */
        queue_draw_box(box_x, box_y, 8, 8, 92); /* an 8x8 box in colour 92 */
        queue_clear_border(0);                  /* the top 7 and bottom 8 rows of the 128x128 screen are not shown: paint them black */

        /* move the box, and bounce it off the edges of the visible area */
        box_x += dx;
        box_y += dy;
        if (box_x == 1) dx = 1; else if (box_x == 119) dx = -1;
        if (box_y == 8) dy = 1; else if (box_y == 112) dy = -1;

        await_draw_queue();      /* let the blitter finish the jobs */
        await_vsync(1);          /* wait for the TV to finish its frame, so nothing tears */
        flip_pages();            /* show what was just drawn, and draw the next frame on the page that was showing */
    }
}
