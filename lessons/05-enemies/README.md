# Lesson 5: enemies

**You will build:** three Vumpires, each sealed in a cave. They stay put until Doug's tunnel reaches them, then they find the way to
him and walk it. If one touches Doug he loses a life.

| sealed | hunting |
|---|---|
| ![sealed](shots/sealed.png) | ![hunting](shots/hunting.png) |

## Many things with the same fields

Three enemies, each with a position, a direction, and so on. The natural C is an array of structs. On a 6502 it tends to be the wrong
tool: reaching the field of element `i` of an array of structs means multiplying `i` by the struct size, and the 6502 has no
multiply instruction. So the fields are kept in **parallel arrays**, one per field (`e_x[i]`, `e_y[i]`, `e_dir[i]`):
element `i` is then just `base + i`.

## Finding the way: breadth first search

```c
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
```

A Vumpire cannot dig, so the only way to Doug is through open tunnel. The search starts at the Vumpire and looks at its
neighbours, then their neighbours, ring by ring. The first time it reaches Doug's cell, the number of rings is the shortest
distance. For every cell reached it writes down *which step got it there*, so when it arrives it can walk that trail backwards to
the very first step, which is all the Vumpire needs to know. No path (the cave is sealed): it returns 0.

The whole map is 14 x 13, so one search is a few hundred instructions at most, and an enemy only searches when it is exactly on a
cell (every 8 pixels), so there is no need for anything cleverer.

```c
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
```

If there is no way, the Vumpire wanders: any open neighbour except the one it came from. The result is an enemy that rattles
around in its cave until the day Doug breaks in, and that is the behaviour that makes the game tense.

## Speed

```c
void update_enemies(void)
{
    unsigned char i, d;
    for (i = 0; i < MAXE; ++i) {
        e_acc[i] += ESPEED;
        if (e_acc[i] < 16) continue;                     /* not this tick */
        e_acc[i] -= 16;
        if (!((e_x[i] | e_y[i]) & 7)) choose_dir(i);     /* only on a whole cell can an enemy choose where to go */
        d = e_dir[i];
        e_x[i] += DX[d]; e_y[i] += DY[d];
        if (d < 2) e_face[i] = d;
    }
}
```

Each Vumpire adds `ESPEED` (9) to a counter every frame and steps when the counter reaches 16: that is 9/16 of a pixel a frame, a
bit over half of Doug's speed, with no division and no fractions. This pattern, "an accumulator and a threshold", is how you get
speeds that are not whole pixels on hardware with no decimals. You will use it again for the beat in lesson 10.

## Gotchas

* **Signed vs unsigned.** `open_cell(c, r)` takes `signed char`s. Column `-1` would be `255` as a byte and look like a valid cell
  far off the edge. The casts in the code are there for that.
* **No recursion.** The search keeps its own queue in a plain array. Recursion is expensive for cc65 code (every call and local
  variable costs time and memory), and a plain loop is easier to follow.

## Try it

* Give each Vumpire a different speed.
* Have the Vumpires prefer to wander toward Doug even when there is no path.

Next: [lesson 6, throwing](../06-throwing/README.md).
