# Lesson 4: digging

**You will build:** the field turns into dirt that Doug digs as he walks, paid by depth, with a score on screen.

![Doug has dug a tunnel](shots/dug.png)

## The map

```c
void new_map(void)
{
    unsigned char r, c;
    for (r = 0; r < ROWS + 1; ++r)
        for (c = 0; c < 16; ++c)
            M(c, r) = (r == 0 && c < COLS) ? 0 : 1;     /* dirt everywhere except row 0, the surface */
    M(6, 1) = 0; M(6, 2) = 0;                            /* and Doug's shaft */
}
```

The map is one byte per cell: 1 is dirt, 0 is tunnel. It has 16 bytes to a row (not 14), so that a cell is
`map[(row << 4) | column]`: again shifts instead of a multiply. The two spare columns and the extra row are "outside" (always
dirt in this lesson, so nothing wanders into them).

## Digging

```c
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
```

Two new ideas in there. Dirt **slows** Doug to every other frame (look at the cell just ahead of him, `LEADX`/`LEADY`), so digging
feels heavy and running along an existing tunnel feels fast. And on every step the cell under his middle is cleared, and he is
paid for it: 10 points near the top, rising to 40 at the bottom.

## Drawing a field that does not fit in the queue

The field is 182 cells, and the draw queue holds 250 jobs. Drawing each cell as a sprite would eat the queue before anything else.
So the field is drawn as **a few big boxes**: four boxes for the four layers of dirt, then one box for each run of
tunnel cells in a row, drawn on top.

```c
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
```

## Text

```c
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
```

The SDK's text module draws characters straight to the screen, not through the queue, so it must run after
`await_draw_queue()` and before `flip_pages()`. It also needs its font loaded into sprite memory *before* your own sheet:

> **Gotcha.** If the first thing you load is your sheet, the text does not show. A 128 x 128 sheet takes a quarter of a sprite
> page, and its slot number says which quarter. The SDK's text routine copies the font's slot number into a hardware register without
> masking off the quarter bits, so it only works when the font is in the first quarter, which is where the first sheet loaded goes.
> Load the font (`text_load_font`) and then your own sheet.

## Try it

* Add a second layer colour and make the deep dirt harder (slower) to dig.
* Make the score count only the first time a cell is dug.

Next: [lesson 5, enemies](../05-enemies/README.md).
