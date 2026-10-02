# Lesson 8: banks

**You will build:** nothing new on screen. The same game as lesson 7, laid out differently in the cartridge so it can grow beyond the
processor's 64K.

| sealed | hunting |
|---|---|
| ![sealed](shots/sealed.png) | ![hunting](shots/hunting.png) |

## Why

The 6502 can only see 64K at once and a GameTank cartridge can be 2M. The cartridge is cut into 16K **banks**. One bank at a time is
switched into the window at `$8000-$BFFF`; the top 16K (`$C000-$FFFF`, the **fixed bank**) is always there, and is where the SDK's
code, the interrupt handlers and the first thing that runs live.

In `project.json`, `"progbanks": 2` asks the build for two program banks (`PROG0` and `PROG1`) besides the fixed one and the
sound/graphics ones, and `make import` writes `gen/bank_nums.h` with their numbers.

{{file 08-banks project.json}}

## Putting code in a bank

`#pragma code-name` tells the compiler where the code that follows goes:

{{lines 08-banks ^#pragma code-name \(push, "PROG1"\)|^#pragma code-name \(pop\)}}

(Everything between the two pragmas, including the helpers `open_cell` and `bfs_dir`, lands in PROG1; the game itself is in PROG0.)

## Calling across banks

A function in PROG1 can only be called while PROG1 is switched in, and the caller is in PROG0, in the same window. So a call has to
switch banks, call, and switch back, and it must be done by code that is *not* in the window: code in the fixed bank. That is
`bank_call` in `bank_call.s` (read it: it is twenty lines of assembler that saves A and X, switches, calls, restores, switches back),
and cc65 can be told to route calls to chosen functions through it:

{{lines 08-banks ^void bank_call\(void\);|^#pragma wrapped-call \(pop\)}}

After that, `update_enemies()` is called like any other function. The compiler emits a call to `bank_call` with the target bank and
address; you do not see it.

## The one thing in the fixed bank

{{fn 08-banks main}}

`main` switches PROG0 in and runs the game. Anything that must be reachable from every bank (like helpers that read other banks'
data, as lesson 9 will do) goes in the fixed bank too, which is simply code that has no `code-name` pragma.

## Gotchas

* **Function pointers across banks.** A pointer to a function in another bank is only good while that bank is switched in. Call
  through the wrapped declaration (above), not through a stored address.
* The fixed bank is small. It is shared with the SDK, so put your code in PROG banks and keep only the glue there.
* Anything that is not in the window's bank when you read it will read as some other bank's bytes. If code in one bank reads
  something in another, the second must be switched in (as `save_peek` does in lesson 9).

## Try it

* Add `"progbanks": 3` and move the drawing code into its own bank.
* What happens if you remove the pair of `wrapped-call` pragmas?

Next: [lesson 9, game flow](../09-game-flow/README.md).
