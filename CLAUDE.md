# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Single-binary OpenGL/GLUT scene: an 8-lane bowling alley with animated balls and pins. Originally a Fall 2015 CSCI-4229 final project. C, fixed-function OpenGL (immediate mode `glBegin`/`glEnd`, `gluLookAt`).

## Build & run

```
make            # produces ./final
make clean
./final         # must be launched from this directory — texture paths are relative
```

The makefile branches on platform: `-framework GLUT -framework OpenGL` on Darwin, `-lglut -lGLU -lGL -lm` on Linux, `-lglut32cu -lglu32 -lopengl32` on Windows. Darwin builds add `-Wno-deprecated-declarations` because Apple's GLUT/OpenGL frameworks are formally deprecated — leave that flag in.

There are no tests, no linter, no CI. Verification is visual: build and run `./final`, then exercise the keybindings (see README.md).

## Code layout

Two static archives link into `final`:

- **`CSCIx229.a`** — course-provided utility library: `fatal.c` (error helper), `loadtexbmp.c` (24-bit uncompressed BMP loader → GL texture name), `print.c`, `project.c` (sets up perspective/ortho projection), `errcheck.c`, `object.c` (OBJ loader). Public surface is `CSCIx229.h`.
- **`bowling.a`** — scene-primitive helpers (`bowling.c`/`bowling.h`): `cieling`, `duct_cube`, `bowling_pin`, `alley`, `mural`, `divider`, `upcurve`, `ball_return_body`, `cap`, `floor_panel`, `wall`, `double_lane`, `pins`, `bowling_ball`, `cube`, `pyramid`. Each takes position + scale + rotation + a texture id and emits geometry.

`final.c` is the application: GLUT entry point, all callbacks (`display`, `key`, `special`, `mouse`, `motionmouse`, `reshape`, `idlefunc`), camera/lighting state, and the scene composition (the body of `display()` is a flat list of calls into `bowling.c` helpers).

`stb_image.h` is vendored. The `STB_IMAGE_IMPLEMENTATION` define lives in `final.c` only — do not define it in any other translation unit.

## State model

The application is driven by a pile of file-scope globals in `final.c`. The important ones:

- Camera: `th`/`ph` (azimuth/elevation), `dim` (zoom), `xOffset`/`yOffset`/`zOffset` (first-person position), `mode` (0 = orbit perspective, 1 = first-person; default is first-person, toggled with `p`). `checkOffsets()` clamps the first-person camera inside the alley.
- Per-lane parallel arrays of length 8: `bowling_ball_z`, `ball_ph`, `pin_x`, `explosion`, `reset`, `colors` — index `i` is lane `i`. Adding/removing a lane means updating every one of these.
- Animation timing: `idlefunc` advances on GLUT elapsed time; `roll_dat_ball` and `explode_and_reset` (one call per lane per tick) drive the rolling-and-resetting state machine using `rollmax` / `explosion_limit` / `explode_at` as thresholds.

## Texture pipeline

`LoadTexBMP` (in `loadtexbmp.c`) only accepts **uncompressed 24-bit BMP** files; it `Fatal`s on anything else. The `.xcf` files in the repo are GIMP source for the corresponding `.bmp` exports — if you regenerate a texture, export back to 24-bit BMP. All textures are loaded once in `main()` and stored in `unsigned int` handles at the top of `final.c`.

## Controls

See README.md for the full keybinding list. Two things to remember when reading code: `p` toggles projection mode, and keys `1`–`8` call `gobowl(lane)` to launch a ball down that lane.
