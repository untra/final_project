# Bowling Alley Final Project
## Made by Samuel Volin for Fall 2015 CSCI-4229

## Build targets

The 2015 native build is preserved. A second target compiles the same C
sources to WebAssembly via Emscripten so the scene runs in a browser.

### macOS / Linux / Windows native (default)

Requires a C compiler and GLUT. macOS ships GLUT in the deprecated `GLUT.framework`
(the makefile silences the deprecation warning). Linux distros usually package
it as `freeglut3-dev`. MinGW on Windows uses `glut32`.

```
make            # builds ./final
./final         # launches the GLUT window
make clean
```

Texture data is regenerated automatically on a fresh checkout — `make` invokes
`scripts/embed-textures.sh` to convert `assets/bmp/*.bmp` into
`assets/textures.{c,h}` (using `sips` on macOS, `magick`/`convert` elsewhere),
which is then linked into the binary. To force a regen:

```
make clean-wasm     # also wipes assets/textures.{c,h} and assets/png/
make textures       # regenerate without building
```

### WebAssembly (browser)

Requires the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
on your `PATH` so `emcc` resolves, plus `sips` or ImageMagick for the texture
conversion step.

```
# one-time: install + activate emsdk
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
~/emsdk/emsdk install latest
~/emsdk/emsdk activate latest
source ~/emsdk/emsdk_env.sh        # add this to your shell rc to persist

# build
make wasm
```

Output lands in `build/web/`:

```
build/web/index.html      # the page (hand-written shell, controls cheat-sheet)
build/web/index.js        # Emscripten loader
build/web/index.wasm      # compiled binary (~5 MB, textures embedded)
build/web/style.css       # styling
```

Serve it locally over HTTP — browsers refuse to load `.wasm` from `file://`:

```
python3 -m http.server -d build/web 8000
# open http://localhost:8000/
```

Click the canvas before pressing keys (keyboard events only fire on the focused
element). If textures appear upside-down on first run, flip the `1` to a `0` in
the `stbi_set_flip_vertically_on_load(1)` call in `loadteximg.c` — the BMP and
PNG row-order convention differ and it can only be confirmed by eye.

### Lint and format

```
make lint           # cppcheck (warning/performance/portability)
make format         # clang-format -i over project sources
make format-check   # dry-run for CI
```

Vendored `stb_image.h` and generated `assets/textures.{c,h}` are excluded.

## Controls

* left / right arrow keys to rotate view around the x axis

* up / down arrow keys to rotate the view and the y axis

* pgDn / pgUp keys to zoom out and in, respectively

* left-click dragging horizontally will rotate the view around the x axis

* left-click dragging vertically will rotate the view around the y axis

* scrolling in and out will zoom in and out, respectively.

* p will change the projection mode

* 0 resets the view

* 'l' toggles lighting

* Press R to roll the ball!

* 1 through 8 will launch the bowling ball

* -/+ decrease/increase the field of view

* [/] decreases/increases the light y-component

* f moves forward (in first person mode only)

* v moves backward (in first person mode only)
