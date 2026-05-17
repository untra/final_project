# WASM build diagnostic handoff (round 2)

You're picking up a stalled Emscripten/WebGL port of an OpenGL/GLUT
bowling-alley scene. Read `WASM_DIAGNOSTICS.md` first — that's the
round-1 handoff and the foundation. **This file** is the round-2
addendum: what the previous Claude tried, what was conclusively
proven, and where the trail goes next. The native macOS build still
renders correctly; only the wasm build is broken.

**Read this entire document before touching code.** Two previous
sessions burned cycles chasing the wrong hypothesis. Don't make it
three.

---

## TL;DR — what is conclusively known now

1. **The rendering pipeline reaches the canvas.** A `glClearColor(1,0,1,1)`
   + `glClear` + `glutSwapBuffers` + immediate `return` from `display()`
   produces solid magenta on the canvas. Verified by user screenshot
   (2026-05-14 ~18:48). Therefore: framebuffer, swap chain, canvas
   pixels — all good.
2. **All 10 textures load and decode successfully.** Console logs
   `LoadTexFromMemory: WxH ch=3 -> id N` for ids 2 through 11,
   non-zero. Texture sizes are 256², 512², 1024² — all power-of-two.
   Texture *binding* and *sampling* during draw also work (one
   wood-grain quad renders correctly through the full pipeline).
3. **The `wasm_compat.h` shim layer is mostly correct** as a strategy.
   No new `Aborted(...)` errors appear in console. Only the three
   baseline emulator warnings ("GL emulation", "unsafe opts", "GL
   immediate mode emulation"). After adding `-s GL_UNSAFE_OPTS=0` the
   middle warning is suppressed.
4. **`GL_LIGHTING` being on is part of the bug.** With lighting
   *disabled* on wasm (`#ifdef __EMSCRIPTEN__ glDisable(GL_LIGHTING);`
   inserted right before scene helpers in `display()` — see
   `final.c:213-218` in current state), substantially more geometry
   becomes visible. This isolated the COLOR_MATERIAL-emulation
   limitation as a real cause of "black scene."
5. **THE NEW SMOKING GUN (this round's key finding):** the camera
   override technique conclusively showed that **the modelview transform
   does not change the rendered output**. Setting `xOffset/yOffset/
   zOffset/th/ph/dim/mode` to wildly different values before the
   `gluLookAt` call in `display()`, with `printf` proving the values
   reach `gluLookAt`, produced **the same canvas pixels** as the
   default spawn. Camera positions tried, all producing identical
   screenshots:
   - `(0, 2, -60)` (spawn, first-person looking +z)
   - `(94, 68, -163)` (orbit at th=30°, ph=20°, dim=100 looking at origin)
   - `(-38, 60, -120)` (first-person elevated, ph=-25°, dim=80)
   - `(-44, 4, -30)` (first-person player at foul line, ph=0°, dim=50)
6. **Same view, very different `gluLookAt` arguments.** The first-frame
   printf for case 4 confirmed:
   `gluLookAt eye=(-44.0,4.0,-30.0) center=(-44.0,4.0,70.0) th=0 ph=0 dim=50.0 mode=1`
   and the rendered scene matched the default-spawn screenshot pixel for
   pixel.

The strongest available hypothesis: **Emscripten's `gluLookAt` (and/or
the matrix-mode state around it) is silently failing or being applied
in the wrong matrix context, so geometry is always drawn from an
identity modelview regardless of camera input.**

---

## How to build, serve, verify

Same as round 1. Repeating here so you don't have to flip files.

```sh
source ~/emsdk/emsdk_env.sh         # emsdk is NOT on PATH; source per shell
make wasm                            # produces build/web/index.{html,js,wasm}
lsof -i :8000                        # reuse existing python server if running
/usr/bin/python3 -m http.server -d build/web 8000   # NOT plain python3 (asdf)
```

Native sanity check (any change you make should preserve the native build):

```sh
make clean && make && ./final
```

After every wasm rebuild:

1. DevTools open, **Network tab → "Disable cache" ticked**.
2. Right-click reload → "Empty Cache and Hard Reload" (Chrome) OR open
   in a private/incognito window. The browser caches wasm aggressively
   and **at least one round of confusion was caused by stale wasm
   appearing identical to working wasm**. Confirm in console that any
   `printf`-based version sentinel you add actually prints the expected
   value.
3. Confirm the served file shasum matches disk:
   ```sh
   shasum build/web/index.wasm
   curl -sI http://localhost:8000/index.wasm | grep -i content-length
   ```

---

## Current code state (cleaned up — start here)

The previous session's diagnostic shims have been REMOVED from `final.c`
to give you a clean baseline. What remains under `#ifdef __EMSCRIPTEN__`:

| File | Location | What | Status |
|---|---|---|---|
| `final.c` | after `if (light) { ... }` block, before scene helpers (~line 339) | `glDisable(GL_LIGHTING)` | **Keep** — production wasm path. Reasoning in code comment. |
| `final.c` | inside `if (light)` block (~lines 268-280) | Brightened `Ambient[]`/`Diffuse[]` to (0.55, 0.55, 0.55) | Keep — currently inert because lighting is disabled immediately after, but documents the dim-lighting problem and is ready if lighting is re-enabled. |
| `loadteximg.c` | end of `LoadTexFromMemory` | `printf` each loaded texture's WxH/ch/id | Keep — useful smoke test, harmless. |
| `wasm_compat.h` | `wasm_set_color3f`/`wasm_set_color4f` | `&& g_wasm_mode == 0` gate on `glMaterialfv` (only call outside Begin/End) | Keep — round 1's findings still hold. Removing this gate breaks texture binding (confirmed empirically twice). |
| `makefile` | `EMCC_FLAGS` | `-s GL_UNSAFE_OPTS=0` added | Keep. |

What's NOT in the code anymore (was diagnostic-only): magenta
`glClearColor`, the camera-position override, the version-sentinel
`printf`s, the `gluLookAt`-argument trace. If you want them back, the
template is below in "Reinstrumenting diagnostics."

---

## Investigate in this order (highest signal first)

### 1. Verify the modelview matrix actually changes after `gluLookAt`

This is the load-bearing test. If `gluLookAt` is silently doing
nothing, then EVERY render is from an identity view, regardless of
keyboard input or position override, and that explains every weird
symptom this round.

Right after the `gluLookAt(xOffset, yOffset, zOffset, Cx, Cy, Cz,
0, Cos(ph), 0);` call in `final.c:255` (current state — first-person
branch), insert:

```c
#ifdef __EMSCRIPTEN__
{
    static int mv_logged = 0;
    if (mv_logged < 2) {
        GLfloat m[16];
        glGetFloatv(GL_MODELVIEW_MATRIX, m);
        printf("frame %d post-gluLookAt MV row0=(%.2f %.2f %.2f %.2f) row1=(%.2f %.2f %.2f %.2f) row2=(%.2f %.2f %.2f %.2f) row3=(%.2f %.2f %.2f %.2f)\n",
            mv_logged,
            m[0], m[4], m[8],  m[12],
            m[1], m[5], m[9],  m[13],
            m[2], m[6], m[10], m[14],
            m[3], m[7], m[11], m[15]);
        mv_logged++;
    }
}
#endif
```

(Note: OpenGL stores matrices column-major; printing as I have above
prints by ROW which is what a human expects.)

If the matrix is **identity** for any of:
- spawn `(0, 2, -60)` first-person looking forward
- a moved-camera state (e.g., override `xOffset=-44, yOffset=4,
  zOffset=-30`)

…then `gluLookAt` is broken under Emscripten's `LEGACY_GL_EMULATION`.
That's almost certainly what's happening.

**If gluLookAt IS broken:** the workaround is to compute the view
matrix in C and load it with `glLoadMatrixf` instead. The math is
straightforward (see "If gluLookAt is broken" below).

**If gluLookAt IS working:** the matrix is correct but somewhere
downstream the geometry is still being placed at unexpected world
positions. Then chase `glPushMatrix`/`glPopMatrix` and the per-helper
transforms in `bowling.c`.

### 2. Check `glMatrixMode` consistency

The `Project()` helper (in `project.c`) ends with:

```c
glMatrixMode(GL_MODELVIEW);
glLoadIdentity();
```

If Emscripten's emulator is in `GL_PROJECTION` mode (or some other
mode) when `display()` calls `glLoadIdentity` + `gluLookAt`, the
view transform goes to the wrong matrix. Add a `glGetIntegerv(GL_MATRIX_MODE, &m)`
trace right before the `gluLookAt` call to confirm.

### 3. Verify `glViewport` is set correctly

`reshape()` in `final.c:631` calls `glViewport(0, 0, width, height)`.
But GLUT's first reshape in the wasm environment might pass the wrong
dimensions. The HTML canvas is 900×600 (see `web/shell.html`). If
`glViewport` is set to 600×600 or some other wrong size, content gets
stretched or cut off.

Trace: in `reshape()`, log `width` and `height`. Confirm they match
the actual canvas.

```c
#ifdef __EMSCRIPTEN__
   printf("reshape: width=%d height=%d -> asp=%.3f\n", width, height, asp);
#endif
```

### 4. Confirm `asp` is right for `gluPerspective`

`Project()` calls `gluPerspective(fov, asp, dim/16, 16*dim)`. `asp`
defaults to 1.0 and is set to `width/height` in `reshape()`. If
reshape never fires (or fires with width=height=0), aspect is 1 and
the FOV horizontal will be wrong (which significantly changes what's
in frustum).

### 5. Sanity-check texture binding state across helpers

The earlier "one wood-grain quad visible, nothing else" behavior was
not investigated all the way. Possible mechanism: `wall()` binds
`wall_texture` and draws ONE quad; helpers that draw MULTIPLE quads
in one `glBegin(GL_QUADS)` block hit the QUADS→TRIANGLES re-emission
loop in `wasm_compat.h` and **state may not survive** the per-vertex
re-emission of `glColor4f`/`glNormal3f`/`glTexCoord2f`. Worth checking
once #1-#4 are settled — but **don't pursue this until you've ruled
out the gluLookAt theory**.

---

## What was tried that didn't work (don't repeat)

| Attempt | What happened | Conclusion |
|---|---|---|
| Removed `&& g_wasm_mode == 0` gate in `wasm_compat.h` to let `glColor3f` inside Begin/End update material via `glMaterialfv` | Scene went from "one wood-grain fragment visible" to "completely black, only animated light sphere visible" | Round 1's "this gate is the bug" hypothesis was wrong. The previous session's note that in-Begin/End `glMaterialfv` breaks texture binding was empirically right. Gate has been restored. |
| Brightened wasm Ambient/Diffuse from (0.1)/(0.3) to (0.55)/(0.55) | No visible change — surfaces still invisible with lighting on | Eventually superseded by disabling lighting entirely on wasm. The brightened values are now inert (kept as documentation). |
| Added `-s GL_UNSAFE_OPTS=0` to `EMCC_FLAGS` | Silenced one of the three Emscripten warnings. No visible scene change. | Keep the flag (cheap, recommended by emulator itself), but it's not the missing piece. |
| Disabled `GL_LIGHTING` on wasm just before scene helpers | Substantially more geometry rendered (was the breakthrough of this round) — but most of canvas still filled with one mauve wall fragment. | **Keep this.** This is the current production wasm path. |
| Forced camera to four different overview positions via per-frame assignment to global state in `display()` | All four positions produced **identical canvas output**. Console-`printf` confirmed `gluLookAt` was called with the override values each frame. | Smoking gun — see TL;DR. Drove the round-2 hypothesis. |

## What was tried that exposed cache problems (also don't repeat)

- The browser caches `index.wasm` aggressively. Multiple iterations
  this round were spent on "the override doesn't work" before realizing
  the canvas was showing a stale wasm. **Always confirm with a printf
  sentinel that your latest build is actually running before drawing
  conclusions from screenshots.**
- "Empty Cache and Hard Reload" sometimes failed to bust the wasm
  cache even with DevTools open and "Disable cache" ticked. Most
  reliable bust: **incognito window**.

---

## If `gluLookAt` is broken (likely root cause)

Replace the `gluLookAt` call with explicit matrix construction. The
view matrix for an eye-center-up triple is:

```
forward = normalize(center - eye)
side    = normalize(cross(forward, up))
up'     = cross(side, forward)

view = [  side.x,     side.y,     side.z,    -dot(side, eye)    ]
       [   up'.x,      up'.y,      up'.z,    -dot(up',  eye)    ]
       [-forward.x, -forward.y, -forward.z,   dot(forward, eye) ]
       [    0,         0,          0,           1                ]
```

In column-major (what `glLoadMatrixf` expects):

```c
static inline void load_view(double ex, double ey, double ez,
                             double cx, double cy, double cz,
                             double ux, double uy, double uz)
{
    double fx = cx - ex, fy = cy - ey, fz = cz - ez;
    double fn = 1.0 / sqrt(fx*fx + fy*fy + fz*fz);
    fx *= fn; fy *= fn; fz *= fn;
    double sx = fy*uz - fz*uy, sy = fz*ux - fx*uz, sz = fx*uy - fy*ux;
    double sn = 1.0 / sqrt(sx*sx + sy*sy + sz*sz);
    sx *= sn; sy *= sn; sz *= sn;
    double u2x = sy*fz - sz*fy, u2y = sz*fx - sx*fz, u2z = sx*fy - sy*fx;
    GLfloat m[16] = {
        (GLfloat)sx,  (GLfloat)u2x, (GLfloat)-fx, 0.0f,
        (GLfloat)sy,  (GLfloat)u2y, (GLfloat)-fy, 0.0f,
        (GLfloat)sz,  (GLfloat)u2z, (GLfloat)-fz, 0.0f,
        (GLfloat)(-(sx*ex + sy*ey + sz*ez)),
        (GLfloat)(-(u2x*ex + u2y*ey + u2z*ez)),
        (GLfloat)(fx*ex + fy*ey + fz*ez),
        1.0f
    };
    glLoadMatrixf(m);
}
```

Use this in place of `gluLookAt` under `#ifdef __EMSCRIPTEN__`. If the
scene starts responding to camera changes after that, you have your
answer.

A simpler intermediate test: replace `gluLookAt(...)` with
`glTranslated(-xOffset, -yOffset, -zOffset)` (yes, just translation,
no rotation). If that makes the scene shift when you change
`xOffset`, you've isolated the failure to `gluLookAt` specifically.

---

## Reinstrumenting diagnostics (templates)

If you need to re-add the diagnostic infrastructure I removed, here
are the exact patterns:

**Build-version sentinel** (so you can prove the new wasm is loaded):

```c
#ifdef __EMSCRIPTEN__
{
    static int v_logged = 0;
    if (!v_logged) {
        printf("WASM BUILD vN: <one-line description>\n");
        v_logged = 1;
    }
}
#endif
```

Bump `vN` every rebuild. Look for the line in the browser console.

**Bright clear color** (so you can see what's covered by geometry vs
empty):

```c
#ifdef __EMSCRIPTEN__
glClearColor(1.0f, 0.0f, 1.0f, 1.0f);  // magenta
#endif
```

Place immediately before `glClear`.

**Camera override** (to force a known view):

```c
#ifdef __EMSCRIPTEN__
mode = 1;
xOffset = -44; yOffset = 4; zOffset = -30;
th = 0; ph = 0; dim = 50;
Project(fov, asp, dim);
#endif
```

Place at the top of `display()`. This overrides keyboard input each
frame — fine for debugging, but remember to remove it before
restoring user control.

**The "draw nothing" pipeline test** (binary: does the pipeline
reach the canvas?):

```c
#ifdef __EMSCRIPTEN__
glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
glFlush();
glutSwapBuffers();
return;
#endif
```

At the top of `display()`. If canvas isn't magenta, the pipeline is
broken; if it is, the pipeline works and the bug is in what's drawn.

---

## Architectural off-ramp (still on the table)

Round 1's WASM_DIAGNOSTICS.md flagged a possible VBO-based rewrite as
the "Phase B" fallback. **This is still the correct fallback if the
gluLookAt/matrix path turns out to be irrecoverable.** The scene's
geometry is ~90% static (see round-1 analysis); a retained-mode
rewrite with hand-written shaders would bypass `LEGACY_GL_EMULATION`
entirely and is the most reliable path to a working wasm build.

But don't start there. **Verify the matrix-state hypothesis first.**
The cost is one `glGetFloatv(GL_MODELVIEW_MATRIX, m); printf(...)`
block and one rebuild. If it's an identity matrix, the diagnostic is
final and the fix is small (`glLoadMatrixf` workaround above). If the
matrix is non-identity yet the scene still doesn't change with camera
moves, that's a different — and much harder — bug, and the VBO
rewrite probably is the right call.

---

## What the native build looks like (target)

The user confirmed the native macOS build still renders correctly:
`make clean && make && ./final` produces a working textured, lit
bowling-alley scene with all the keybindings working. That's the
visual target — same alley, same textures, same controls. The
acceptable degradations on wasm (in order of how acceptable):

1. Flat shading instead of Phong (acceptable — this is the current
   path with `GL_LIGHTING` disabled).
2. Missing specular highlights on the ball/pins (acceptable).
3. Missing emission on the light marker sphere (already the case;
   the shim filters `GL_EMISSION`).
4. Reduced colour palette (less acceptable — some surfaces will
   appear default-white instead of their `glColor` tint if material
   updates inside Begin/End don't work).
5. No camera motion / fixed view (NOT acceptable — must be fixed).

The native is at `./final` after `make`. Compare side-by-side with
the wasm canvas at `http://localhost:8000/` as you work.

---

## File map (where things live)

- `/Users/samuelvolin/Documents/untra/final_project/final.c` — GLUT
  entry point, `display()`, camera state, `main()`. The camera
  override and verification printfs go here.
- `/Users/samuelvolin/Documents/untra/final_project/wasm_compat.h` —
  all the shims. The `&& g_wasm_mode == 0` gate at `wasm_set_color3f`/
  `wasm_set_color4f` is the round-1 finding to **not** disturb.
- `/Users/samuelvolin/Documents/untra/final_project/project.c` —
  contains `Project()` which sets up perspective + matrix mode.
  Check the `glMatrixMode` calls if you suspect mode issues.
- `/Users/samuelvolin/Documents/untra/final_project/fatal.c:15-21` —
  defines `g_wasm_cm_face/mode/on` (extern in `wasm_compat.h`).
- `/Users/samuelvolin/Documents/untra/final_project/loadteximg.c` —
  the wasm-shared texture loader. Currently has a `printf` smoke
  test at end.
- `/Users/samuelvolin/Documents/untra/final_project/bowling.c` —
  scene helpers. **Do not read end-to-end** (1000+ lines). Grep for
  specific calls you're investigating.
- `/Users/samuelvolin/Documents/untra/final_project/makefile` —
  `EMCC_FLAGS` block has `-s LEGACY_GL_EMULATION=1 -s USE_WEBGL2=1
  -s GL_UNSAFE_OPTS=0 -s ALLOW_MEMORY_GROWTH=1`.
- `/Users/samuelvolin/Documents/untra/final_project/web/shell.html` —
  HTML shell, canvas 900×600, JS Module config.
- `/Users/samuelvolin/Documents/untra/final_project/WASM_DIAGNOSTICS.md`
  — round-1 handoff. Still relevant for context, but its hypothesis
  ranking is now superseded by this document.

---

## Invariants (preserve these)

- Native macOS build (`make clean && make && ./final`) must render
  identically after every change. Gate every wasm-specific change
  behind `#ifdef __EMSCRIPTEN__` or wasm-only source files.
- Never `cd` to add `cd <dir> &&` prefixes to git commands.
- Never `source ~/emsdk/emsdk_env.sh` in the user's shell profile —
  source it per-shell when invoking `emcc`/`make wasm`.
- Use `/usr/bin/python3` for the http server — plain `python3` is an
  asdf shim and fails without `.tool-versions`.
- After every shim or shader change: verify in the browser with cache
  disabled OR in an incognito window before reporting success.
  Stale-wasm has burned hours across three sessions.
- Don't continue piling shims into `wasm_compat.h` without proof that
  each one moves a measurable indicator. The first thing to do on any
  iteration is verify the previous change actually took effect via a
  `printf` sentinel.
