# Lesson 6: throwing

**You will build:** Doug throws baseballs with button A. Three hits put a Vumpire out and score by depth. A Vumpire that is stunned is
still deadly to touch, so a hit buys a moment, not safety.

| in flight | a strike |
|---|---|
| ![ball](shots/out.png) | ![struck](shots/struck.png) |

## The ball

```c
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
```

The ball moves 4 pixels per frame, as **two steps of 2**. A Vumpire is 8 pixels wide and the hit window is 6, so a ball that
moved 8 at once could jump straight over one. Whenever something fast has to collide with something small, move it in steps
smaller than the target. The same loop checks dirt (the ball stops at it) and range (it falls after 40 pixels).

## A strike

```c
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
```

Each enemy has a small state machine: walking, struck (`ES_INFL`, stunned), or put out (`ES_POP`, showing its popup). The
`e_infl` count is how many strikes it has taken. The scoring by depth uses the same layers as the digging.

## Score popups

```c
void add_popup(unsigned char x, unsigned char y, unsigned int v)
{
    unsigned char i, best = 0, oldest = 255;
    for (i = 0; i < MAXP; ++i)                           /* use a free slot, or else replace the oldest popup */
        if (pop_t[i] == 0) { best = i; break; }
        else if (pop_t[i] < oldest) { oldest = pop_t[i]; best = i; }
    pop_x[best] = x; pop_y[best] = y; pop_v[best] = v; pop_t[best] = 28;
}
```

Popups are another small pool: two slots, reuse a free one or take over the oldest. A fixed pool is the usual way to do "many
short-lived things" without ever allocating.

## Try it

* Let Doug have two balls in the air.
* Make a stunned Vumpire recover faster each time.

Next: [lesson 7, sound](../07-sound/README.md).
