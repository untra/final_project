# WASM build diagnostic handoff

You're picking up a stalled port of an OpenGL/GLUT fixed-function bowling-alley
scene to Emscripten/WebAssembly. The wasm build no longer aborts at runtime,
but the scene renders almost entirely black — only a small fragment of one
textured surface is visible. The user cannot see the full alley.

**Read this entire document before touching code.** A previous Claude session
piled shim after shim into `wasm_compat.h` chasing per-call-site
`Aborted(...)` errors; that approach has hit a wall and likely needs to be
re-thought rather than continued. The systematic-debugging skill's "3+ fixes
failed → question the architecture" rule applies.

---

## Read these files first

In this order:

1. `CLAUDE.md` — project overview, build, code layout, what the textures and
   state model look like.
2. `makefile` — both `make` (native) and `make wasm` targets. The `EMCC_FLAGS`
   block is where any flag changes go.
3. `wasm_compat.h` — every shim added so far lives here. Skim it end-to-end
   before changing anything.
4. `fatal.c` — yes, `fatal.c`. It now also holds the single definition of the
   `g_wasm_cm_*` state variables that `wasm_compat.h` declares `extern`.
5. `final.c` `main()` and `display()` (around lines 230–340, 740–770) — the
   GLUT entry point and the body of every frame. The lighting setup and the
   camera setup live here.

Do not read `bowling.c` end-to-end (it's 1000+ lines of repetitive scene
geometry). Grep it for whatever specific call you're investigating.

---

## How to build and serve locally

Emscripten is installed at `~/emsdk` (emcc 5.0.7). It is NOT on the default
`PATH` — `which emcc` in a fresh shell returns nothing. You must source it
each shell:

```sh
source ~/emsdk/emsdk_env.sh
make wasm                                    # produces build/web/index.{html,js,wasm}
/usr/bin/python3 -m http.server -d build/web 8000
# ⚠️  NOT plain `python3` — that's an asdf shim that fails without .tool-versions
# Open http://localhost:8000/ in a browser
```

A python http.server may already be running from the previous session on
port 8000 — check with `lsof -i :8000` before starting a new one.

Native sanity check (any shim regression you cause should preserve the
native macOS build):

```sh
make clean && make && ./final
```

`make clean-wasm && make wasm` does a full clean rebuild including
regenerating `assets/textures.c` from the BMPs (about 30s).

**Always test in the browser with DevTools open and "Disable cache" ticked in
the Network tab.** Both `index.js` and `index.wasm` cache aggressively —
multiple iterations in the previous session were wasted on the user seeing
a stale build.

---

## Current state (as of handoff)

### What changed from the original repo

| File | Change | Why |
|---|---|---|
| `loadteximg.c:37`, `loadtexbmp.c:90` | Integer literal `3` → `GL_RGB` for `glTexImage2D` internalformat | WebGL rejects numeric internalformat; only symbolic enums |
| `wasm_compat.h` | Major expansion — see breakdown below | Each entry was a runtime `Aborted(...)` fix |
| `fatal.c` | Added `#ifdef __EMSCRIPTEN__` block with definitions of `g_wasm_cm_face`, `g_wasm_cm_mode`, `g_wasm_cm_on` | Cross-TU sharing of glColorMaterial emulation state |
| `web/shell.html` | Added `<link rel="icon" href="data:,">` | Suppress favicon 404; cosmetic |
| `makefile` | Added `-s USE_WEBGL2=1` to `EMCC_FLAGS` | Unlocked some textured rendering; was the move that got the scene from fully-black to partially-rendering |

### Shims in `wasm_compat.h` (in approximate order added)

- `glVertex3d`/`glNormal3d`/`glRotated`/`glScaled`/`glTranslated` → float
  variants (pre-existing).
- `glRasterPos3d`, `glWindowPos2i` → no-ops (debug-text positioning is
  already no-op'd in `print.c`).
- `glLightModelfv` → filter out `GL_LIGHT_MODEL_LOCAL_VIEWER` (Emscripten
  aborts on `TODO: 2897`).
- `glLightModeli` → bridge to filtered `glLightModelfv`.
- `glColorMaterial` + `glEnable(GL_COLOR_MATERIAL)`/`glDisable(...)` →
  emulated. Tracks `g_wasm_cm_on`, `g_wasm_cm_face`, `g_wasm_cm_mode`. Used
  to drive the `glColor3f → glMaterialfv` bridge below.
- `glMaterialfv` → filter `GL_EMISSION` (aborts on `TODO: 5632`) and split
  `GL_AMBIENT_AND_DIFFUSE` (aborts on `TODO: 5634`) into individual `GL_AMBIENT`
  + `GL_DIFFUSE` calls.
- `glMaterialf` → bridge to filtered `glMaterialfv`.
- `glBegin` / `glEnd` → substitute unsupported primitive modes
  (`GL_QUADS → GL_TRIANGLES`, `GL_QUAD_STRIP → GL_TRIANGLE_STRIP`,
  `GL_POLYGON → GL_TRIANGLE_FAN`). For `GL_QUADS`, also buffers per-vertex
  state and re-emits 6 vertices as 2 triangles every 4 vertices.
- `glNormal3f` / `glTexCoord2f` / `glColor3f` / `glColor4f` / `glVertex3f` /
  `glVertex3fv` / `glVertex3dv` → wrappers that maintain `g_wasm_cur`
  per-vertex state for the QUADS buffer.
- `glColor3f` / `glColor4f` additionally call `glMaterialfv(face, mode, c)`
  for COLOR_MATERIAL emulation, **but only when outside a `glBegin/glEnd`
  block** (`g_wasm_mode == 0`). The inside-Begin/End case was tried and
  observed to suppress all texturing.

### What is observed in the browser

- **No `Aborted(...)` lines in the console.** Only the three baseline
  Emscripten warnings: "GL emulation", "GL emulation unsafe opts", "GL
  immediate mode emulation. This is very limited in what it supports". The
  unsafe-opts warning explicitly suggests `-sGL_UNSAFE_OPTS=0`.
- **One textured fragment is visible** in screenshots — a brown wood-grain
  rectangle in the upper right of the canvas, presumably the wall or a duct.
- The rest of the canvas is dark gray / black. The bowling alley's lanes,
  pins, floor, ceiling, mural — none of it is clearly visible. The user
  reports they "cannot see the full alley" and is uncertain textures are
  loading at all.
- Whether the keyboard controls work in the current build is **not
  confirmed**. The previous session erroneously concluded controls were
  broken when in fact the program was being halted by an abort; that abort
  is now fixed, so controls should work, but this has not been verified
  with the user. **Confirm or rule out before debugging rendering further** —
  if the user can move the camera, the "mostly black" view might just be a
  bad spawn position, not a rendering bug.

---

## Hypothesis ranking — investigate in this order

### 1. The camera is spawned inside or facing into geometry (cheapest to rule out)

The default mode is first-person (`mode = 1` in `final.c`). If `xOffset` /
`yOffset` / `zOffset` start inside a wall, you'd see a black scene. The fix
is trivial: get the user to press `0` (reset view), `P` (toggle to orbit
projection), or `W`/`A`/`S`/`D` to move.

**Test:** ask the user to click the canvas (focus), press `0`, then `P`,
then describe what they see. If the alley becomes visible from that angle,
this entire "rendering is broken" thread was a camera-position red herring.

If keys do nothing after clicking the canvas, you have a *separate* GLUT
input issue to debug — but don't start there. Confirm or rule out the
camera position first.

### 2. `g_wasm_cm_on == 1 && g_wasm_mode != 0` skips material updates (most likely real bug)

The `glColor3f` shim only updates material when called **outside** a
`glBegin/glEnd` block (see `wasm_compat.h` `wasm_set_color3f`). But
`bowling.c` sets per-quad color *inside* `glBegin` constantly:

```c
glBegin(GL_QUADS);
glColor3f(0.8, 0.4, 0.4);     // ← gated out by my fix; material stays default
glNormal3f(1, 0, 0);
glTexCoord2f(...); glVertex3f(...);
...
glEnd();
```

So most textured surfaces are getting drawn with whatever material was last
set *outside* any `glBegin/glEnd` — which is often the GL default (white
ambient 0.2, diffuse 0.8). Combined with the very dim lighting in
`final.c:241-243` (Ambient (0.1, 0.1, 0.1), Diffuse (0.3, 0.3, 0.3)),
surfaces render at roughly 0.06 luminance × texture, which is near-black.

The previous session enabled this skip-inside-Begin/End specifically because
calling `glMaterialfv` between `glBegin/glEnd` appeared to break texturing.
That correlation should be re-tested rigorously:

**Test plan:**

a. Remove the `&& g_wasm_mode == 0` gate temporarily. Rebuild. Refresh with
   cache disabled. Observe:
   - If textures now appear AND no abort fires: the previous correlation was
     wrong (or fixed by `USE_WEBGL2`). Keep this version. Done.
   - If textures still don't appear: revert.
   - If an abort fires: the in-Begin/End glMaterialfv is still incompatible.
     Pursue alternative below.

b. Alternative if (a) shows in-Begin/End `glMaterialfv` is incompatible:
   apply the material *at glEnd time* using the last-set color. This is
   ugly (loses per-vertex variation) but matches what this codebase
   actually does (one color per quad block in 99% of cases).

   Approximate sketch:

   ```c
   // in wasm_set_color3f, when inside Begin/End:
   if (g_wasm_cm_on && g_wasm_mode != 0) {
       g_pending_cm = 1;
       g_pending_cm_color = {r, g, b, 1};
   }
   // in wasm_glEnd:
   real glEnd();
   if (g_pending_cm) {
       glMaterialfv(g_wasm_cm_face, g_wasm_cm_mode, g_pending_cm_color);
       g_pending_cm = 0;
   }
   // BUT this applies material AFTER the geometry is rendered, which is wrong.
   // Workaround: apply material BEFORE glBegin from the next-most-recent color,
   // by snapshotting g_pending_cm before glBegin and applying it as glMaterialfv
   // immediately prior to entering the next glBegin.
   ```

   This is fiddly. Don't go here unless (a) clearly fails.

### 3. Texture handles silently failed to load (somewhat likely)

If `LoadTexFromMemory` returned `0` (failure) for most textures, the only
surface that renders correctly is the one whose handle did succeed. The
loader does `Fatal()` on stb decode failure but does NOT `Fatal()` on a
`glGetError()` after `glTexImage2D` returning a value that means "no error
but the texture is incomplete." This isn't a thing in WebGL but it's worth
checking the actual returned `texture` handle.

**Test:** add a `printf` at the bottom of `loadteximg.c::LoadTexFromMemory`:

```c
printf("LoadTexFromMemory: %dx%d ch=%d -> id %u\n", w, h, channels, texture);
```

Recompile, reload, look at the console. You expect 10 lines, all non-zero
handles. If you see `id 0` for any, that texture failed silently.

(`printf` in Emscripten goes to `console.log`, but only if the Module is
configured to forward it. The current `shell.html` has `print: function (t) {
console.log(t); }` so it should work.)

### 4. Try `-sGL_UNSAFE_OPTS=0` (Emscripten itself suggested this)

The console warning says: "WARNING: using emscripten GL emulation unsafe
opts. If weirdness happens, try -sGL_UNSAFE_OPTS=0." We have weirdness.

**Test:** edit `makefile` EMCC_FLAGS, add `-s GL_UNSAFE_OPTS=0`, rebuild,
reload. Will probably either (a) make texturing more reliable, (b) expose
new aborts that were being masked, or (c) do nothing. Cheap to try.

### 5. The Emscripten LEGACY_GL_EMULATION + immediate mode + WebGL2 combination is fundamentally limited

If hypotheses 1–4 all fail to produce a watchable scene, the approach of
"link the original C immediate-mode code through Emscripten's fixed-function
emulation" is the wrong architecture for this codebase. At that point,
options are:

- **Convert the inner geometry loops to retained-mode** (build a VBO once,
  draw it each frame). The scene is mostly static — lanes, walls, pins,
  ceiling don't change. Only the rolling balls and the light marker
  animate. A retained-mode pipeline would bypass the immediate-mode
  emulation entirely. Significant rewrite but clean.
- **Use the regl/sokol/raylib path**, which the user would probably reject
  for a CSCI-4229 final project port — but worth flagging as the "actually
  correct" answer if the user is open to it.
- **Accept that the wasm build is a degraded preview** and ship it as
  such — document the limitations in the README and move on.

**Do not** continue adding shims past this point without explicit user
approval. The user explicitly said earlier "prepare diagnostic instructions
for the next claude to follow"  — they are aware progress has stalled.

---

## Diagnostic command cheat sheet

```sh
# build + serve
source ~/emsdk/emsdk_env.sh
make clean-wasm && make wasm
/usr/bin/python3 -m http.server -d build/web 8000

# fast iteration (no texture regen)
source ~/emsdk/emsdk_env.sh
make wasm

# native sanity
make clean && make && ./final

# inspect a shim's effect on emitted wasm
emcc -O3 -I. -s LEGACY_GL_EMULATION=1 -s USE_WEBGL2=1 -s ALLOW_MEMORY_GROWTH=1 \
     -E final.c | grep -A2 'wasm_set_color3f' | head -40

# search the scene for a specific GL pattern
grep -n -E 'glBegin\(GL_QUADS\)' bowling.c | head

# check texture dimensions (all should be POT)
for f in assets/png/*.png; do
  /usr/bin/python3 -c "import struct; d=open('$f','rb').read(24); w,h=struct.unpack('>II',d[16:24]); print(f'$f: {w}x{h}')"
done

# check what's actually in the deployed wasm
shasum build/web/index.wasm
curl -sI http://localhost:8000/index.wasm
```

---

## Things the previous session got wrong — don't repeat

- Concluded "controls don't work" from a build that was actually aborting.
  Always verify there's no abort before debugging downstream symptoms.
- Added shims for 6+ separate aborts without questioning whether
  `LEGACY_GL_EMULATION` was the right approach in the first place. Each new
  shim is a Band-Aid; the architectural problem is upstream.
- Did not get visual confirmation (screenshot) until very late. Ask the user
  for a screenshot AND a console paste AND a description of what they tried
  in keyboard input, every iteration.
- Started new HTTP servers without checking if one was already running on
  the port. Use `lsof -i :8000`.

## Things the previous session got right — keep doing

- Sourced emsdk per-shell rather than mutating the user's profile.
- Gated all changes behind `#ifdef __EMSCRIPTEN__` so the native build
  remains untouched and continues to render correctly. **Preserve this
  invariant.**
- Verified the native build after every shim change. Do the same.
- Cross-referenced TODO numbers in `Aborted(glXxx: TODO: NNNN)` to specific
  GL enum constants — that's how each shim was scoped. Same pattern applies
  if any new abort surfaces.
