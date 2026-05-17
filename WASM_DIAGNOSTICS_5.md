# WASM build diagnostic handoff (round 5)

Read [`WASM_DIAGNOSTICS.md`](WASM_DIAGNOSTICS.md),
[`WASM_DIAGNOSTICS_2.md`](WASM_DIAGNOSTICS_2.md),
[`WASM_DIAGNOSTICS_3.md`](WASM_DIAGNOSTICS_3.md), and
[`WASM_DIAGNOSTICS_4.md`](WASM_DIAGNOSTICS_4.md) first for context. This is
the round-5 addendum. Tooling docs in [`AGENT_RUNBOOK.md`](AGENT_RUNBOOK.md).

Round 4 ended pointing at Phase B (VBO renderer for static geometry) as the
next move. **Phase B was already done in commit `388eb56` before round 4
declared it pending** — round 4 hadn't noticed. Round 5's job ended up
being to finish the cleanup: the Phase-B fragment shader was left in a
deliberate UV-as-color diagnostic state (`+ t * 0.0`) plus two other live
diagnostic remnants, all of which made the WASM build look like nothing
was textured. Those are now reverted. **All static textures sample
correctly on WASM as of vB18.** Remaining issues are documented below for
round 6.

---

## TL;DR — what is conclusively known now

1. **Textures sample correctly on every static surface on WASM.** The
   round-4 conclusion ("the immediate-mode shim can't deliver textured
   QUADS sampling") was correct, and Phase B is what unblocks it. The
   walls (id 8), murals (ids 4–7), floor (id 2), ceiling (id 10), and
   ducts (id 11) all draw via `wasm_static_geom.c`'s VBO + custom shader,
   not the shim. The shader is now:
   ```glsl
   gl_FragColor = texture2D(u_texture, v_uv) * v_color;
   ```
   which mirrors the native `glTexEnvi(GL_TEXTURE_ENV_MODE, GL_MODULATE)`
   at `final.c:360`. Verified visually in `build/web/probe/latest.angle-{0,2,3,5}.png`:
   wood grain on the floor strips, red brick on the walls, perforated
   tiles on the ceiling, metallic diamond pattern on the duct cubes.
2. **The probe harness is unchanged and still the right tool.** No new
   tooling was added in round 5. `make wasm-probe` writes 6 angle PNGs
   plus state JSON and full console log to `build/web/probe/`. The
   `WASM BUILD vB18:` sentinel in `final.c:220` confirms the live build.
3. **None of the 6 current probe angles frames the back wall** where the
   murals live (world z≈108–110, x≈{-2,-38,-74,-110}). Mural texture
   sampling is verified by *parity* (same shader, same binding code,
   same vertex format as wall/floor batches which sample correctly) but
   not by visual pixel inspection. **Round 6 should add an `angle-6`
   that points at the back wall** — see "Recommended next steps" below.
4. **Bowling pins render at the wrong scale on WASM.** Native renders pins
   at `0.25 × xScale,yScale,zScale` per `bowling_pin()` at `bowling.c:74`.
   On WASM the pins appear visibly oversized — taking up significantly
   more lane width than the native build. Best visible in
   `build/web/probe/latest.angle-5.png` (the yellow pin blobs embedded in
   the wood lane). The `pins()` call at `final.c:380` is identical
   between native and WASM; the regression is in how the shim handles
   the pin's matrix stack / `glScaled`. **This is the round-6 priority.**
5. **Ball returns / dividers / caps / upcurves are missing on WASM.**
   `double_lane()` is gated out at `final.c:382-393` and the VBO
   replacement (`wasm_static_geom.c`) only re-implements the lane floor
   strips, not the ball-return infrastructure. The white blob the user
   may interpret as a misplaced "ball return" in some WASM frames is
   actually the diagnostic light-position marker drawn at `final.c:286`
   (`ball(Position[0]...,1,white)`) — not a true ball return.

---

## Round-5 changes

All in two files. Native build untouched.

| File | Change |
|---|---|
| `wasm_static_geom.c:63-70` | Fragment shader replaced UV-as-color diagnostic (`gl_FragColor = vec4(v_uv.x, v_uv.y, 0.5, 1.0) + t * 0.0`) with real `texture2D(u_texture, v_uv) * v_color`. |
| `wasm_static_geom.c:151` | `build_walls` vertex color reverted from diagnostic red `{0.8,0.4,0.4,1.0}` to `{1,1,1,1}` — otherwise the new shader would multiply every wall texel by red. |
| `wasm_static_geom.c:193` | `build_murals` transform z reverted from vB14 diag `-100.0` (camera-spawn) back to real `-10.0` (matches `final.c:371-374` call sites for native `mural()`). Stale `vB14 diag:` comment removed. |
| `wasm_static_geom.c:463-464` | Stale `vB13 diag: ... use UV-as-color shader` comment removed from `wasm_static_geom_draw`. |
| `final.c:220` | Sentinel: `WASM BUILD vB18: fragment shader samples texture; wall+mural diagnostics reverted`. |

---

## Open problem 1 (round-6 priority) — pin scaling on WASM

**Symptom:** in WASM probe angles 0/1/5, pins render at what looks like
~4× their native size — taking visible width of the lane rather than
sitting as small markers. Compare native (run `make native-shot` after
fixing macOS permissions, below) against
`build/web/probe/latest.angle-5.png`.

**Why this is suspicious:**

- `pins()` at `bowling.c:770-791` is called identically on native and
  WASM (`final.c:380`, no `#ifdef`).
- `pins()` calls `bowling_pin(...,0.25*xScale,0.25*yScale,0.25*zScale,...)`,
  so each pin should be 25% of its unscaled size.
- `bowling_pin()` at `bowling.c:65-222` builds the pin using
  `glPushMatrix → glTranslated → glRotated × 2 → glScaled` then a series
  of `glBegin(GL_TRIANGLE_FAN)` / `glBegin(GL_QUAD_STRIP)` blocks ending
  with `glPopMatrix`.
- Per round-4 findings the `GL_QUAD_STRIP → GL_TRIANGLE_STRIP` direct
  substitution path in `wasm_compat.h` works correctly (wood lanes
  sample fine), so the pin's strip emission is going through a known-
  working code path.

**Hypotheses to investigate (round 6):**

1. **Matrix-stack leak from the VBO draw.** `wasm_static_geom_draw()`
   (called at `final.c:363` before the pin loop) saves and restores
   `GL_CURRENT_PROGRAM`, `GL_ARRAY_BUFFER_BINDING`, `GL_ACTIVE_TEXTURE`,
   `GL_TEXTURE_BINDING_2D`, but it does **not** save/restore matrix
   state. It does set `GL_PROJECTION_MATRIX` and `GL_MODELVIEW_MATRIX`
   into its shader as uniforms (lines 431-432, 440-441), but it does
   not modify the matrix stack itself. Still worth verifying with the
   "is the modelview matrix actually changing per frame" probe pattern
   from `AGENT_RUNBOOK.md:141-164`, applied immediately before the
   `pins()` loop, to see if the matrix at pin-draw time differs between
   native and WASM. Specifically check: is the modelview scaled? Was
   there a stray `glScaled` somewhere that the wasm shim swallowed?
2. **`glScaled` not honored inside the shim.** `wasm_compat.h` may not
   forward `glScaled`/`glScalef` correctly. Grep for it:
   `grep -n "glScale" wasm_compat.h` — if there's no shim entry and
   Emscripten's `LEGACY_GL_EMULATION` handles it natively, that should
   be fine, but it's worth confirming the matrix actually changes via
   `glGetFloatv(GL_MODELVIEW_MATRIX, m)` immediately after `glScaled`
   in a one-shot debug build of `bowling_pin()`.
3. **Frame-of-reference mismatch with the VBO shader.** The VBO's
   shader uses the snapshotted projection+modelview at draw time. If
   the pin's coordinate system was implicitly relying on a matrix
   state that the VBO draw established and then "left behind", the
   pins could be drawing in VBO-shader space rather than fixed-
   function space. Less likely but cheap to test by swapping the order
   of `wasm_static_geom_draw()` and the `pins()` loop in `display()`.
4. **The pin's accumulated `alphaheight`.** `bowling_pin()` builds the
   pin in 10+ stacked strips, each incrementing a local `alphaheight`.
   If `glScaled(0.25, ...)` were silently a no-op, the unscaled pin
   would be ~6 units tall and ~2 wide. Lane is 10 wide. Pin at 6×2 in
   world coords would look exactly like the oversized pins in the
   WASM screenshots. **This is the leading hypothesis.**

**First experiment for round 6:** in `display()` immediately before the
pin loop, on WASM only, print the current modelview:
```c
#ifdef __EMSCRIPTEN__
{
    static int pin_mv_logged = 0;
    if (!pin_mv_logged) {
        GLfloat m[16];
        glGetFloatv(GL_MODELVIEW_MATRIX, m);
        printf("PIN-PRE MV: %.3f %.3f %.3f %.3f / %.3f %.3f %.3f %.3f / %.3f %.3f %.3f %.3f / %.3f %.3f %.3f %.3f\n",
               m[0],m[4],m[8],m[12], m[1],m[5],m[9],m[13],
               m[2],m[6],m[10],m[14], m[3],m[7],m[11],m[15]);
        pin_mv_logged = 1;
    }
}
#endif
```
Add a matching print *inside* `bowling_pin()` right after the `glScaled`
call (line 74) — also gated to wasm and one-shot — to confirm the scale
actually applied. If `m[0]/m[5]/m[10]` are not the expected 0.25 (times
whatever the camera matrix contributes), the scaled-out matrix isn't
sticking, and the shim's matrix-stack handling is the bug.

---

## Open problem 2 — probe doesn't frame the murals

**Symptom:** none of the 6 angles in `tools/wasm-probe.mjs:203-227`
points at the back wall (world z≈108–110, x={-2,-38,-74,-110}). The
mural batches (1–4) are confirmed bound to texture IDs 4–7 with count=6
per batch in `latest.log`, but no PNG visually proves the mural texels
appear on the right surface.

**Fix (small):** add `angle-6` to `tools/wasm-probe.mjs`. From spawn
(first-person, looking down +z), murals are already in the camera
direction but very far away. An angle that walks forward (the `]` key
moves forward per README/main controls — check the actual key) and then
tilts down slightly would frame the murals. Or use orbit mode from a
position that frames the back wall:

```js
// example angle-6 spec for tools/wasm-probe.mjs
{ name: 'angle-6', keys: ['KeyP', 'KeyP', 'ArrowDown:3'], duration: 250 },
// then capture
```

(Confirm key bindings via `final.c::key()` — the existing angle specs
press `ArrowUp`/`ArrowRight`/`BracketLeft`, so the camera-step keys are
clearly mapped.)

**Pass criterion for the new angle:** four distinct mural images
(`tex_1` through `tex_4` from `assets/textures.c`) visible side-by-side
across the back wall. Currently the mural .bmp/.png sources should be
inspected via `assets/bmp/1.bmp`..`4.bmp` to know what to look for.

---

## Open problem 3 — `make native-shot` blocked by macOS permissions

**Symptom (round 5):**
```
67:72: execution error: Not authorized to send Apple events to System Events. (-1743)
```

`tools/native-shot.sh` uses AppleScript `System Events` to find the
running `./final` window, then `screencapture -l WID`. This requires
two macOS permissions on the **terminal/process that invokes make** —
not on `./final` itself.

**One-time setup (must be done by the human at the keyboard — Claude
cannot grant these):**

1. Open **System Settings → Privacy & Security**.
2. **Accessibility** → click the `+` → add **the application that
   runs `make`** (Terminal.app, iTerm.app, VS Code, Claude Code, etc.
   — whichever is your shell host). Toggle the entry **on**.
3. **Screen Recording** (also under Privacy & Security) → add the same
   application. Toggle on.
4. **Automation** (Privacy & Security → Automation) → expand your
   terminal app, find **System Events**, toggle on. *This is the one
   that fixes the `-1743` error specifically — Accessibility/Screen
   Recording alone are not enough on recent macOS.*
5. **Fully quit and relaunch** the terminal application. macOS does
   not pick up new permissions for already-running processes.
6. Re-run `make native-shot`. The first run may pop additional
   permission dialogs — click **Allow** on every one and retry until
   it succeeds.

If `osascript` still fails after the four toggles and a relaunch, the
fallback is to bypass the AppleScript window-find entirely: comment out
the `AXWindowNumber` lookup in `tools/native-shot.sh` and capture the
full screen with `screencapture -x build/native/probe/latest.png`.
That requires only the Screen Recording permission, not Accessibility
or Automation, and gives a full-screen PNG to crop afterward.

For Claude sessions: **if `make native-shot` fails, do not pursue it
further** — surface the error to the user and continue with `make
wasm-probe`-only verification. The native screenshot is for visual
parity comparison, not a correctness gate.

---

## Open problem 4 — ball returns / dividers / caps missing on WASM

`double_lane()` (`bowling.c:793`) draws `alley()`, `upcurve()`,
`divider()`, `cap()`, `ball_return_body()`, `cap()`. On WASM it is
gated out at `final.c:382-393`. The VBO replacement (`wasm_static_geom.c::build_floor_panels`)
only re-implements the lane wood strips and a few floor panels — it does
not include the dividers, ball returns, upcurves, or caps.

**Why this was punted in round 5:** the user explicitly scoped round 5
to "fix textures + verify with the probe; pin geometry handled in
follow-up". Ball-return reconstruction is meaningful new geometry work
and didn't make sense to interleave with the texture-sampling fix.

**For round 6 or later:** add `build_ball_returns`, `build_dividers`,
`build_caps`, `build_upcurves` to `wasm_static_geom.c` that emit the
same primitives as `bowling.c::ball_return_body` / `divider` / `cap` /
`upcurve` but pre-transformed into the existing `WasmStaticBatch` for
their respective texture (or `0` for the no-texture
`GL_TEXTURE_BINDING_2D=0` flat-colored ones, which the shader needs to
handle — currently `texture2D` will sample undefined data when no
texture is bound). Probably easiest to extend the shader with a
`uniform bool u_use_texture;` and branch in `main`.

---

## Invariants (preserve into round 6)

- Native macOS build (`make clean && make && ./final`) renders
  identically — verified after round 5.
- The three `bowling.c` UB fixes from round 4 (`cieling()`, `alley()`,
  `divider()` orphan Begin) are correct on every platform. Do not
  revert.
- The `gluLookAt` macro override (`wasm_compat.h:143-172`) is
  load-bearing for camera, kept.
- The `glBindTexture` shim in `wasm_compat.h` is still load-bearing for
  the wood lane's per-iteration rebind tracking. Keep.
- The `wasm_static_geom_draw` saves/restores `GL_CURRENT_PROGRAM`,
  `GL_ARRAY_BUFFER_BINDING`, `GL_ACTIVE_TEXTURE`, `GL_TEXTURE_BINDING_2D`.
  Do not remove these saves — pin/ball immediate-mode draws happen
  immediately after and they depend on the legacy fixed-function path
  being intact.
- The probe sentinel pattern at `final.c:217-223` is the only defense
  against stale-wasm artifacts surviving across builds. Bump on every
  round-6 rebuild that touches generated output.
- Do not pursue `GL_COLOR_MATERIAL` off on wasm (round-3 finding —
  regresses to black scene).

---

## File map (round-5 state)

| File | Round-5 changes |
|---|---|
| `wasm_static_geom.c:63-70` | Fragment shader samples `u_texture` × `v_color` (was UV-as-color diag) |
| `wasm_static_geom.c:151` | `build_walls` vertex color: `{0.8,0.4,0.4,1}` → `{1,1,1,1}` |
| `wasm_static_geom.c:193` | `build_murals` z: `-100.0` → `-10.0`; stale comment removed |
| `wasm_static_geom.c:463-464` | Stale `vB13 diag` comment removed |
| `final.c:220` | Sentinel bumped to `vB18` |
| `WASM_DIAGNOSTICS_5.md` | This file |

Unchanged but referenced: `bowling.c::bowling_pin` (line 65), `bowling.c::pins`
(line 770), `bowling.c::double_lane` (line 793), `wasm_compat.h`
(round-4 state intact), `tools/wasm-probe.mjs:203-227` (angle specs),
`tools/native-shot.sh` (permission-gated, never modified by round 5).

---

## Recommended round-6 entry sequence

1. Read this file + skim `WASM_DIAGNOSTICS_4.md` for the shim state.
2. `source ~/emsdk/emsdk_env.sh && make wasm-probe` — confirm vB18
   build still runs clean; sanity-check the probe artifacts.
3. **Tackle the pin scaling first** (Open Problem 1, "First experiment"
   above). It's a self-contained probe-driven debug loop — should be a
   1–2 hour task at most. Bump sentinel to `vB19` on entry.
4. Add the back-wall angle (Open Problem 2) — 15-minute task. This is
   the falsifiable test for mural texture sampling.
5. Decide with the user whether to take on Open Problem 4 (ball
   returns) — that's a 1–2 hour focused VBO-extension task.

If pin scaling turns out to be a deeper shim issue, the round-1 off-ramp
is still open: replace pin immediate-mode draws with a small VBO
animated-mesh path. The pin's vertex layout never changes per frame —
only its transform does — so it would batch well.
