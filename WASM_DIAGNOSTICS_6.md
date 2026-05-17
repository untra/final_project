# WASM build diagnostic handoff (round 6)

Read [`WASM_DIAGNOSTICS.md`](WASM_DIAGNOSTICS.md),
[`WASM_DIAGNOSTICS_2.md`](WASM_DIAGNOSTICS_2.md),
[`WASM_DIAGNOSTICS_3.md`](WASM_DIAGNOSTICS_3.md),
[`WASM_DIAGNOSTICS_4.md`](WASM_DIAGNOSTICS_4.md), and
[`WASM_DIAGNOSTICS_5.md`](WASM_DIAGNOSTICS_5.md) first for context. This is
the round-6 addendum. Tooling docs in [`AGENT_RUNBOOK.md`](AGENT_RUNBOOK.md).

Round 5 (`vB18`) shipped the texture-sampling fix for the VBO renderer and
flagged two follow-up items: pin sizing and mural visual verification.
Round 6 (`vB28`) addresses both — with a surprise: **the round-5 hypothesis
that pins render oversized on WASM was wrong**, and **murals at the
original z=108-110 do not render visibly on WASM**, the opposite of what
round 5's "verified by parity" assumption assumed.

---

## TL;DR — what round 6 established

1. **Pins are correctly scaled on WASM.** `glScaled(0.25, ...)` at
   `bowling.c:74` *does* apply. Matrix dump probe (PIN-DIAG below) shows
   modelview diagonal `(-0.25, 0.25, -0.25)` — exactly the expected
   `0.25 × view_matrix_diagonal`. Pin top vertex (local `(0, 6, 0)`)
   projects to NDC `(-0.162, -0.005)` in the spawn view — confirming the
   pin lands as a ~5 px tall marker at the far end of the lane at z=110.
   The round-5 author's "yellow pin blobs embedded in the wood lane"
   observation in `WASM_DIAGNOSTICS_5.md:50` was a misidentification:
   pins have no yellow color in their geometry (`bowling.c:81-220` uses
   `glColor3f(1,1,1)` white and `glColor3f(0.765,0,0.2)` red). The
   yellow circles in the probe screenshots are `bowling_ball()` at
   `bowling.c:1000` (`glColor3f(1,1,0)`).
2. **Murals at z=108-110 do not render visibly on WASM.** Geometry IS
   queued (every probe run shows `batch 1-4: tex=4..7 count=6`) and the
   VBO data dump (round-6 transient diag) confirmed world positions
   match the native `mural()` call sites exactly. But when the mural's
   fragment shader was forced to output magenta, only a single tiny
   speck of magenta appeared in the spawn view — corresponding to one
   corner of mural 2 (world x≈-45..-38). The other 3 murals plus most
   of mural 2 produced no pixels.  When mural 0 was relocated to
   z=-43 (in front of the spawn camera) as a control, it rendered
   fully and correctly with its texture. So the rendering pipeline
   works; something about the z=108-110 position breaks it. **This is
   the round-7 priority.**
3. **Two new probe angles added.** `tools/wasm-probe.mjs:228-256` adds
   `angle-6` (mural row from spawn-back-and-left position, currently
   unsuccessful — geometry should be in frame but isn't drawn) and
   `angle-7` (pin row close-up from inside the pin chamber, showing
   pins rendering correctly at small/dense scale).

---

## Round-6 changes (final state, all diagnostics reverted)

| File | Change |
|---|---|
| `tools/wasm-probe.mjs:228-256` | Added `angle-6` (back-wall framing) and `angle-7` (pin close-up). Each uses the existing `page.keyboard.press` harness — `KeyP/KeyW/KeyS/KeyA/KeyD/BracketLeft` all verified to round-trip correctly through Emscripten's GLUT keyboard handler. |
| `final.c:220` | Sentinel bumped to `vB28: round-6 cleanup — pins verified correct, angles 6/7 added`. |
| `WASM_DIAGNOSTICS_6.md` | This file. |

**No code changes** to `bowling.c`, `final.c::display()`, `wasm_compat.h`,
or `wasm_static_geom.c` shader/batch code. The round-5 invariants from
`WASM_DIAGNOSTICS_5.md:263-284` are all preserved.

---

## How the pin diagnosis ran

Transient instrumentation added in `bowling_pin()` after `glScaled` at
`bowling.c:74` (removed after diagnosis):

```c
#ifdef __EMSCRIPTEN__
{
  static int pin_diag_logged = 0;
  if (!pin_diag_logged) {
    GLfloat mv[16], pj[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);
    glGetFloatv(GL_PROJECTION_MATRIX, pj);
    float lx = 0.0f, ly = 6.0f, lz = 0.0f, lw = 1.0f;
    float ex = mv[0]*lx + mv[4]*ly + mv[8]*lz  + mv[12]*lw;
    float ey = mv[1]*lx + mv[5]*ly + mv[9]*lz  + mv[13]*lw;
    float ez = mv[2]*lx + mv[6]*ly + mv[10]*lz + mv[14]*lw;
    float ew = mv[3]*lx + mv[7]*ly + mv[11]*lz + mv[15]*lw;
    float cx = pj[0]*ex + pj[4]*ey + pj[8]*ez  + pj[12]*ew;
    float cy = pj[1]*ex + pj[5]*ey + pj[9]*ez  + pj[13]*ew;
    float cw = pj[3]*ex + pj[7]*ey + pj[11]*ez + pj[15]*ew;
    printf("PIN-DIAG mvdiag=%.3f %.3f %.3f  top-ndc=(%.3f,%.3f)  cw=%.3f\n",
           mv[0], mv[5], mv[10], cx/cw, cy/cw, cw);
    pin_diag_logged = 1;
  }
}
#endif
```

Output:
```
PIN-DIAG mvdiag=-0.250 0.250 -0.250  top-ndc=(-0.162,-0.005)  cw=171.000
```

Interpretation:
- `mvdiag=(-0.25, 0.25, -0.25)` = `glScaled(0.25)` × view matrix diagonal
  `(-1, 1, -1)` from `wasm_glu_lookat` (with eye looking +Z, up +Y). The
  scale **is** sticking.
- `top-ndc=(-0.162, -0.005)` and `cw=171.0` say the pin's top vertex
  lands very close to the screen center, at a clip-space depth of 171
  units. The pin bottom (local origin) is at the same NDC y minus a
  tiny offset (since the pin is 6 units tall in local coords × 0.25
  scale = 1.5 world units, viewed from 171 units away → ~5 px). Pin is
  correctly sized but tiny in default angles.

---

## Open problem 1 (round-7 priority) — murals at z=108-110 don't render

**Symptom (verified with shader-forced magenta diag):** the mural VBO
batches at world z=108-110 produce essentially no pixels on screen.
A single ~5 px magenta speck appears in spawn view at the location of
mural 2's right edge (world x≈-45) when the shader is forced to output
magenta for mural vertices. Murals 0, 1, 3 plus the rest of mural 2
produce nothing.

**Verified NOT the cause:**
- Geometry world positions: dumped all 6 vertices per batch; coordinates
  match `final.c:371-374` native `mural()` calls exactly:
  ```
  batch 0 (mural 0): (34,6,110)→(-2,6,110)→(-2,36,108)→(34,36,108)
  batch 1 (mural 1): (-2,6,110)→(-38,6,110)→(-38,36,108)→(-2,36,108)
  batch 2 (mural 2): (-38,6,110)→(-74,6,110)→(-74,36,108)→(-38,36,108)
  batch 3 (mural 3): (-74,6,110)→(-110,6,110)→(-110,36,108)→(-74,36,108)
  ```
- UVs: `(0,0)→(1,0)→(1,1)→(0,1)`, identical to native and to all the
  other VBO batches that sample correctly.
- Texture binding: `g_batches[i].texture = mural_texture[i]` set during
  init, matches the IDs loaded by `LoadTexFromMemory` (4-7 per log).
- Texture content: `assets/png/1.png` etc. decode to colored 512×512
  RGB images (verified visually). When mural 0 was relocated to z=-43
  the texture sampled correctly and the graffiti image was visible in
  the spawn view.
- Walls occluding murals: only the rightmost corner of mural 0 is
  geometrically behind the right wall at x=33.5. Disabling all wall
  batches did not increase mural visibility — same single speck.
- Shader / attribute bindings: `glBindAttribLocation` correctly maps
  `a_pos`, `a_uv`, `a_color` to slots 0, 1, 2. Wall, floor, ceiling,
  duct batches use the same shader and sample correctly.
- `glGetError` after each batch draw: clean (no GL errors).
- Backface culling: never enabled in this codebase (`grep -r
  GL_CULL_FACE` returns nothing).
- Depth-test corruption: the floor (y=0) and ceiling (y=30) do not
  intersect the mural quad plane (y=6..36, z=108..110) for most
  pixels; only the top portion (y>30) is geometrically behind the
  ceiling. But the *bottom* half of all murals should be visible.
- Far plane: `gluPerspective(fov=60, asp=1.502, dim/16=3.125, 16*dim=800)`
  at `project.c:14` puts the far plane at 800, well beyond mural at
  distance ~170 from spawn.

**Working hypotheses for round 7:**
1. **WebGL precision / depth-buffer artifact specific to this z-range.**
   The mural quad is non-planar in z (bottom z=110, top z=108) — a
   2-unit tilt over 30 y-units. Combined with `mediump` precision in the
   shader, maybe the depth values land in a degenerate range and the
   rasterizer drops the fragments. The single-pixel magenta speck at
   mural 2's edge is suspicious — that's exactly where the tilt would
   make depth-test most marginal.
2. **NDC-z very close to 1.0.** Mural at distance 170 has NDC z=0.971;
   any further or any z-fight could push it past the far clip. Not the
   primary suspect (relocated-to-z=-43 mural renders fully fine and a
   wall slice at z=120 *does* render).
3. **Some interaction with Emscripten's LEGACY_GL_EMULATION that we
   haven't pinned down yet.** Round-5 verified texture sampling works
   for the VBO path *for batches at low z*. We never visually checked
   the z=108-110 batches. The "verified by parity" claim in
   `WASM_DIAGNOSTICS_5.md:42-46` was overconfident.

**First experiment for round 7:**
- Try moving murals to z=70 (about halfway between spawn and the back
  wall). Murals normally placed at world z=108-110 — try a few
  intermediate z values to find the threshold at which rendering
  starts to fail. If there's a clean threshold, it's a precision or
  depth-clip issue. If failure is intermittent, it's something else.
- Alternative: flatten the mural quad (use z=109 for all four
  corners instead of bottom-z=110/top-z=108). Eliminates the tilt as
  a variable.
- If both fail: dump the actual mural depth values via a `discard;`
  shader experiment — force the shader to discard fragments whose
  computed depth is in a certain range, and bisect to find where
  the murals' fragments actually live.

---

## Open problem 2 — pins are correct, but visually tiny in current angles

Pins at world (pin_x + 5..8, 0..1.5, 111..115.5) at 0.25 scale are ~5 px
tall in the spawn view per the PIN-DIAG output. This is correct sizing.
The user's reported "oversized pins" complaint from
`WASM_DIAGNOSTICS_5.md:50` was inherited from the round-5 author's
misidentification. **No fix is needed** — pins render at the right
visual size.

`angle-7` was added to give a close-up view of the pin row so future
sessions can verify pin geometry. From eye `(0, 2, 106)` looking +Z, the
pin chamber at z=111-115 is right in front of the camera, and the
overlapping triangle pattern in the angle-7 PNG is consistent with
80 correctly-sized pins (10 per lane × 8 lanes) viewed up close.

If the user wants pins to look LARGER as a stylistic choice rather than
a bug fix, that's a deliberate change to the `0.25` scaling in
`bowling.c:778-790` — orthogonal to round-5's WASM correctness story.

---

## Open problem 3 — single-shot native parity

`make native-shot` is still permission-gated on the current macOS. The
runbook in `WASM_DIAGNOSTICS_5.md:194-228` covers the setup. Round 6
did not exercise it; native parity is currently inferred from
`make && ./final` rendering on the human-at-keyboard side rather than
from automated screenshots.

---

## Invariants (preserve into round 7)

- Native macOS build (`make clean && make && ./final`) renders
  identically — verified after round 6.
- Pin scaling is correct on WASM. **Do not "fix" `bowling_pin`'s
  `glScaled` based on round-5's incorrect hypothesis** — the matrix
  evidence falsifies it.
- All three `bowling.c` UB fixes from round 4 stay in.
- The `gluLookAt` macro override (`wasm_compat.h:143-172`) is
  load-bearing for camera.
- The `glBindTexture` shim in `wasm_compat.h` is still load-bearing
  for the wood lane's per-iteration rebind tracking (legacy
  immediate-mode QUADS path).
- `wasm_static_geom_draw` saves/restores `GL_CURRENT_PROGRAM`,
  `GL_ARRAY_BUFFER_BINDING`, `GL_ACTIVE_TEXTURE`,
  `GL_TEXTURE_BINDING_2D`. Do not remove.
- The probe sentinel pattern at `final.c:217-223` is the only defense
  against stale-wasm artifacts surviving across builds. Bump on every
  round-7 rebuild that touches generated output.
- Walls, floor, ceiling, ducts on WASM all sample their textures
  correctly via the VBO path. **The shader and binding code are
  not broken** — only the murals' specific geometry placement
  triggers the bug.

---

## Recommended round-7 entry sequence

1. Read this file + `WASM_DIAGNOSTICS_5.md`.
2. `source ~/emsdk/emsdk_env.sh && make wasm-probe` — confirm vB28 still
   runs clean.
3. Run the mural-distance bisection experiment (Open Problem 1, first
   experiment above). Start with z=70 and z=90. Bump sentinel to vB29.
4. Once root cause is found, fix `wasm_static_geom.c::build_murals` (or
   the shader / draw code) and re-run wasm-probe. Confirm angle-6 shows
   the four colored mural images on the back wall.
5. After mural fix lands, consider Open Problem 4 from round 5 (ball
   returns / dividers / caps missing on WASM). Out of scope for this
   round.
