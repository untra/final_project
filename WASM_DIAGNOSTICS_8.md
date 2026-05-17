# WASM build diagnostic handoff (round 8)

Read [`WASM_DIAGNOSTICS.md`](WASM_DIAGNOSTICS.md),
[`WASM_DIAGNOSTICS_2.md`](WASM_DIAGNOSTICS_2.md),
[`WASM_DIAGNOSTICS_3.md`](WASM_DIAGNOSTICS_3.md),
[`WASM_DIAGNOSTICS_4.md`](WASM_DIAGNOSTICS_4.md),
[`WASM_DIAGNOSTICS_5.md`](WASM_DIAGNOSTICS_5.md),
[`WASM_DIAGNOSTICS_6.md`](WASM_DIAGNOSTICS_6.md), and
[`WASM_DIAGNOSTICS_7.md`](WASM_DIAGNOSTICS_7.md) first. Tooling docs in
[`AGENT_RUNBOOK.md`](AGENT_RUNBOOK.md).

Round 8 (`vB30`) ports the four `double_lane` interior helpers from
`bowling.c` to the static-VBO pipeline in `wasm_static_geom.c`. The
WASM build now renders the lane separators (dividers), front/mid/inner
caps, far-end up-curves, and the multi-color ball-return towers that
were previously absent (the `#ifndef __EMSCRIPTEN__` gate at
`final.c:382-393` removes all four `double_lane()` calls on WASM).

---

## TL;DR — what round 8 established

1. **Four `build_*` functions added to `wasm_static_geom.c`:**
   `build_dividers`, `build_caps`, `build_upcurves`,
   `build_ball_returns`. Each opens its own batch with `tex=0` and
   `use_texture=GL_FALSE`. Per-instance transforms match the offsets
   in `bowling.c::double_lane` (`bowling.c:798-826`).
2. **Fragment shader extended with `uniform bool u_use_texture`.**
   When false, `gl_FragColor = v_color`; when true (default for all
   pre-round-8 batches), `gl_FragColor = texture2D(u_texture, v_uv) *
   v_color`. `WasmStaticBatch` gained a `GLboolean use_texture` field;
   `begin_batch` defaults it to `GL_TRUE`.
3. **`g_batches` capacity grown 8 → 16.** Round-7 already saturated
   the array (walls + 4 murals + floor + ceiling + ducts = 8). The
   four new builders bring the count to 12; 16 leaves headroom.
4. **Three new triangulation helpers:** `append_quad_strip` (handles
   the QUAD_STRIP v3/v2 swap), `append_fan` (for GL_TRIANGLE_FAN and
   convex GL_POLYGON), `append_triangle` (single-triangle convenience
   used by the ball-return downsplit faces). All zero UVs since the
   untextured batches bypass the texture sample.
5. **The `#ifndef __EMSCRIPTEN__` gate at `final.c:382-393` was NOT
   flipped.** The `double_lane()` calls in that block stay removed on
   WASM; the VBO pipeline provides the same geometry. The 6 left-wall
   calls in that same block (`wall(-110.5, ...)`) were already covered
   by `build_walls` at `wasm_static_geom.c:169-172`, so the block can
   stay gated without losing geometry.

Visible result in `make wasm-probe` (run from the spawn-view angle, in
`build/web/probe/latest.angle-0.png` and `latest.angle-1.png`): red /
green / blue / gray ball-return towers visible at the close end of
all four lanes, with the central black trough running away from the
camera. The lane wood is now flanked by gray divider/cap geometry
instead of the previous "stark white void".

---

## Round-8 changes

| File | Change |
|---|---|
| `final.c:220` | Sentinel bumped to `vB30: round-8 port double_lane interior helpers (divider/upcurve/cap/ball_return_body) to static-VBO pipeline`. |
| `wasm_static_geom.c::WasmStaticBatch` (line 23) | Added `GLboolean use_texture` field. |
| `wasm_static_geom.c::k_fragment_shader` (lines 63-77) | Added `uniform bool u_use_texture`; branched output between texture-modulated and color-only paths. |
| `wasm_static_geom.c::g_batches` (line 40) | Capacity 8 → 16. |
| `wasm_static_geom.c::begin_batch` | Defaults `use_texture = GL_TRUE`. |
| `wasm_static_geom.c::build_program` | Looks up `u_use_texture` uniform; success check extended. |
| `wasm_static_geom.c::wasm_static_geom_draw` | `glUniform1i(g_u_use_texture, …)` per batch; `err_logged[16]`; log line includes `use_tex=…`. |
| `wasm_static_geom.c` new helpers | `append_quad_strip`, `append_fan`, `append_triangle`. |
| `wasm_static_geom.c` new builders | `build_dividers`, `build_caps`, `build_upcurves`, `build_ball_returns` — all wired into `wasm_static_geom_init` after `build_ducts`. |
| `WASM_DIAGNOSTICS_8.md` | This file. |

**No code changes** to `bowling.c`, `final.c::display()`, `wasm_compat.h`,
or `tools/wasm-probe.mjs`. The round-7 invariants from
`WASM_DIAGNOSTICS_7.md:189-213` are all preserved.

---

## How to verify the fix

```sh
source ~/emsdk/emsdk_env.sh
make wasm-probe
```

Expected console output (last init line + per-batch dump):

```
[wasm static geom] ready: 12264 vertices across 12 texture batches
[wasm static geom] batch  0: tex=8  first=0     count=72       (walls)
[wasm static geom] batch  1: tex=4  first=72    count=6        (mural[0])
[wasm static geom] batch  2: tex=5  first=78    count=6        (mural[1])
[wasm static geom] batch  3: tex=6  first=84    count=6        (mural[2])
[wasm static geom] batch  4: tex=7  first=90    count=6        (mural[3])
[wasm static geom] batch  5: tex=2  first=96    count=672      (floor panels)
[wasm static geom] batch  6: tex=10 first=768   count=6480     (ceiling)
[wasm static geom] batch  7: tex=11 first=7248  count=864      (ducts)
[wasm static geom] batch  8: tex=0  first=8112  count=1728     (dividers)   ← new
[wasm static geom] batch  9: tex=0  first=9840  count=504      (caps)       ← new
[wasm static geom] batch 10: tex=0  first=10344 count=1008     (upcurves)   ← new
[wasm static geom] batch 11: tex=0  first=11352 count=912      (ball_returns) ← new
```

`centerPixel` reads `[198, 198, 198, 255]` (same as round-7 — the
center of the spawn-view frame hits the back wall / ceiling, which
the new geometry does not occlude). To see the new geometry, open
PNGs:

- `build/web/probe/latest.angle-0.png` — spawn view. Red and green
  ball-return towers visible at the close end of each lane.
- `build/web/probe/latest.angle-1.png` — small offset from spawn.
  Same ball-return towers from a slightly different angle.
- `build/web/probe/latest.angle-6.png` — down-lane perspective.
  Lane wood now flanked by gray divider/cap geometry.

Native:

```sh
make clean && make && ./final
```

Compiles cleanly with the one pre-existing
`-Wunused-but-set-variable` warning on `f` in `bowling_pin` that
predates round 7. **Native binary is unchanged by construction** —
every round-8 source edit lives inside
`wasm_static_geom.c`, which is wrapped entirely in
`#ifdef __EMSCRIPTEN__ … #endif` (the file's first and last lines).

---

## Open problems carried into round 9+

The user explicitly scoped round 8 to the four named interior
helpers. Several related problems remain.

1. **Murals at z=108-110 still do not rasterize on WASM.** (Round-6
   Open Problem 1, still open after round 7's scoping decision.) The
   `build_murals` batches load with correct world-space positions and
   UVs and the textures bind correctly, but only a ~5 px speck of one
   mural rasterizes. Round 6's leading hypothesis is a WebGL
   depth-precision artifact compounded by the 2-unit z-tilt of the
   mural quad (bottom z=110, top z=108) plus the `mediump` precision
   in the fragment shader. The recommended first round-9 experiment
   is bisection: relocate one mural to z=70, another to z=90 and find
   the threshold where rasterization breaks.

2. **`alley()` gutters and arrows are missing from the WASM build.**
   `build_floor_panels` in `wasm_static_geom.c` captures the
   textured lane wood (per-lane 12-strip floor) and the inter-lane
   floor panels, but not the colored gutter geometry
   (`bowling.c:609-636`) or the red arrow triangles
   (`bowling.c:658-677`). They are in the same "double_lane interior
   visually-missing" family as the four helpers ported in round 8 and
   are the natural round-9 continuation. Add a `build_alley_extras`
   (or split into `build_gutters` + `build_arrows`) using the same
   untextured-batch pattern.

3. **Lighting on the new untextured geometry is constant.** The
   static-VBO fragment shader writes `gl_FragColor = v_color` (or
   `texture × v_color`) — no normals attribute, no diffuse/ambient
   math. Native lights its dividers/caps/upcurves/ball-returns via
   fixed-function GL lighting (the helpers issue `glNormal3f` calls);
   WASM renders them at constant brightness. If round-9+ wants
   light-responsive interior geometry, the VBO vertex layout must
   grow a normals attribute and the fragment shader must learn
   `uniform vec3 u_ambient/u_diffuse/u_light_dir`. Not urgent —
   visually the constant-brightness ball returns read fine because
   the per-vertex colors are saturated.

4. **Batch 0 (walls, `tex=8`) logs `glGetError=0x500` (GL_INVALID_ENUM)
   after its `glDrawArrays`.** Pre-existing condition from round 7
   (the round-7 baseline log shows the same error). Did not regress
   in round 8, and the walls visibly render correctly. Probably an
   incidental side-effect of the texture-binding sequence. Carry
   forward as a low-priority unresolved item — investigate only if
   another batch later starts throwing the same error and visually
   degrades.

---

## Invariants (preserve into round 9)

- Native macOS build (`make clean && make`) compiles cleanly with
  only the pre-existing `-Wunused-but-set-variable` on `f` in
  `bowling_pin`. Unchanged by construction (all round-8 source edits
  are inside `#ifdef __EMSCRIPTEN__` in `wasm_static_geom.c`).
- All round-7 invariants from `WASM_DIAGNOSTICS_7.md:189-213` still
  hold: `bowling_pin` glColor3f hoist stays; `wasm_static_geom_draw`
  saves/restores `GL_CURRENT_COLOR` plus four other state slots;
  three `bowling.c` UB fixes from round 4 stay; `gluLookAt` macro
  override and `glBindTexture` shim from `wasm_compat.h` stay; pin
  geometry math is correct on both platforms; trust pixels over
  `glGetFloatv(GL_CURRENT_COLOR, ...)` and `glIsEnabled(...)` return
  values on WASM.
- `WasmStaticBatch::use_texture` defaults to `GL_TRUE` in
  `begin_batch`. Every existing batch that omits to set the field
  continues to sample its texture. New untextured batches must
  explicitly set `batch->use_texture = GL_FALSE;` after
  `begin_batch(0)`.
- The `#ifndef __EMSCRIPTEN__` gate at `final.c:382-393` stays as-is.
  Adding helpers happens entirely in `wasm_static_geom.c`'s
  `wasm_static_geom_init`.
- The probe sentinel pattern at `final.c:217-223` is the only defense
  against stale-wasm artifacts surviving across builds. Bump on
  every round-9 rebuild that touches generated output.

---

## Recommended round-9 entry sequence

1. Read this file + `WASM_DIAGNOSTICS_6.md` (mural rasterization
   problem). For ambitious scope, also `WASM_DIAGNOSTICS_5.md` for
   the historical view of `double_lane` work.
2. `source ~/emsdk/emsdk_env.sh && make wasm-probe` — confirm vB30
   still runs clean and the 12-batch layout matches the "How to
   verify" section above.
3. Pick **one** of the four open problems with the user. Mural
   rasterization is the natural round-9 target (already deferred
   twice). Gutters/arrows are a small mechanical port that mirrors
   round 8 closely. Lighting is the largest scope (shader rewrite +
   normals attribute).
4. Bump sentinel to vB31 on entry.
