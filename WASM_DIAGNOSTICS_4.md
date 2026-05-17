# WASM build diagnostic handoff (round 4)

Read [`WASM_DIAGNOSTICS.md`](WASM_DIAGNOSTICS.md),
[`WASM_DIAGNOSTICS_2.md`](WASM_DIAGNOSTICS_2.md), and
[`WASM_DIAGNOSTICS_3.md`](WASM_DIAGNOSTICS_3.md) first for context. This is
the round-4 addendum.

This round took the round-3 probe harness, used it to **disprove the
round-3 hypothesis about why most textures don't render**, and isolated
the failure to one specific code path that the immediate-mode shim
can't fix from within — pointing at Phase B (VBO rewrite of static
geometry) as the next session's correct move.

For tooling docs read [`AGENT_RUNBOOK.md`](AGENT_RUNBOOK.md); this file
is findings.

---

## TL;DR — what is conclusively known now

1. **Three real source-level UB bugs in `bowling.c` were latent on
   every platform**, silently tolerated on Apple's GL implementation:
   - `bowling.c:19` (`cieling()`) — `glDisable(GL_TEXTURE_2D)` was
     between `glBegin(GL_QUADS)` and `glEnd()`. Fixed: moved after
     `glEnd()`.
   - `bowling.c:604` (`alley()`) — orphan `glBegin(GL_QUAD_STRIP)` was
     opened before the gutter loops but never matched; the loop body
     opens its own paired Begin/End. Fixed: removed.
   - `bowling.c:237` (`divider()`) — same orphan-Begin pattern as
     `alley()`. Fixed: removed.
   Native build verified visually unchanged. None of these alone
   restored wasm textures, but they tighten the diagnostic window and
   are bugs regardless.
2. **The round-3 hypothesis was wrong.** Round 3 said: "in-Begin/End
   `glMaterialfv` triggered via `glColor*` + `GL_COLOR_MATERIAL`
   clobbers the texture binding under `LEGACY_GL_EMULATION`." This
   round refactored `wasm_compat.h` to defer the real `glBegin` for
   `GL_QUADS` and emit each completed 4-vertex quad as its own
   self-contained real Begin/End — with `glBindTexture` re-issued and
   `glColor4f` applied **strictly outside** any real Begin/End.
   Probe data confirmed binding survives correctly into the draw.
   **The walls still render flat default-white.** Per-quad colour
   isn't the cause.
3. **The actual `glBindTexture` problem is different from round 3's
   model.** Round-3 probe (`tex_bind=11` for 18 walls/murals) was
   *not* the emulator silently stale-binding — it was that
   `bowling.c:alley()`'s wood-lane loop re-binds `floor_texture`
   (id 2) every iteration, and that bind sticks across helper
   boundaries. The new shim's `glBindTexture` wrapper captures every
   bind and re-issues it inside `wasm_flush_quad`; the per-quad bind
   id is now provably correct (`wall=8`, `murals=4..7`, etc.).
4. **The remaining failure is in `LEGACY_GL_EMULATION`'s textured
   immediate-mode TRIANGLES/TRIANGLE_STRIP path itself, when entered
   from buffered QUADS.** The wood lane goes through the
   non-buffered `GL_QUAD_STRIP → GL_TRIANGLE_STRIP` substitution and
   samples correctly. The walls/murals/cieling/duct_cube go through
   `GL_QUADS → 4-vertex buffer → per-quad real Begin/End` and do
   *not* sample, even though:
   - `glBindTexture` is re-issued immediately before the real
     `glBegin` (`got_bind` matches `last_bind` per probe).
   - `glColor4f` is issued **before** the real `glBegin` (so any
     implicit `COLOR_MATERIAL → glMaterialfv` happens outside
     Begin/End).
   - Tex coordinates *are* captured (`has_t=1`, `s0/t0` show expected
     `(0,0)`, `(1,1)` per first vertex).
   - Per-vertex `glNormal3f`, `glTexCoord2f`, `glVertex3f` are
     replayed in clean order inside the real Begin/End.
   - Emitting as `TRIANGLE_STRIP` (V0,V1,V3,V2) instead of
     `TRIANGLES` (V0,V1,V2,V0,V2,V3) — i.e. matching the wood
     lane's exact primitive — **also does not restore sampling**.
5. **Phase B is the recommended next move.** Eight rounds of shim
   tweaking with the round-3 harness produced clean probe data but
   no visible texture-sampling for the buffered-QUADS path. The
   round-1 fallback (VBO retained-mode for static geometry) is now
   the next session's correct entry point.

---

## What the shim looks like at the end of round 4

`wasm_compat.h` is left in a state where round-5 can either:
- Keep the shim and bypass it for static geometry (write VBO helpers
  in parallel, call them in place of the bowling.c helpers under
  `#ifdef __EMSCRIPTEN__`).
- Throw the shim out entirely and replace immediate-mode usage with
  retained mode (bigger rewrite).

The shim's verified-correct pieces:

| Component | What it does | Verified by |
|---|---|---|
| `gluLookAt` macro override | Builds view matrix in C, loads with `glLoadMatrixf`. | Round 3 A/B test, kept. |
| `wasm_glBindTexture` shim | Captures last `GL_TEXTURE_2D` id in `g_wasm_last_tex_2d`. | Round-4 `FLUSH[*]` probe shows id matches per helper. |
| `wasm_glBegin(GL_QUADS)` deferred | No real `glBegin` for QUADS — defers to per-quad emission in `wasm_flush_quad`. | Builds clean, no `Aborted(`. |
| `wasm_flush_quad` | On 4th buffered vertex: re-bind, set colour, real `glBegin(GL_TRIANGLE_STRIP)`, replay 4 vertices, real `glEnd`. | Probe shows correct bind/colour reach the real Begin. Texture still doesn't sample. |
| Per-vertex `glColor4f` hoisted out of `wasm_replay` | Replay only emits normal+texcoord+vertex. | Round-4 confirmed by removal — gates an in-Begin material update. |
| `wasm_glEnd` for QUADS is a no-op | Matches the deferred Begin in `wasm_glBegin`. | Builds clean, `wasm_glEnd` resets `g_wasm_mode = 0`. |
| `GL_QUAD_STRIP → GL_TRIANGLE_STRIP` direct substitution | Untouched. | Wood lane still renders. |

The current build sentinel is `vB4` in `final.c:217`. The harness'
inner loop (`make wasm-probe`) still works end-to-end.

---

## What was tried that didn't work (round-4 only — don't repeat)

| Attempt | Result | Conclusion |
|---|---|---|
| Phase 3a: hoist per-vertex `glColor4f` out of `wasm_replay`, issue once before real `glBegin` | No visible change. Probe clean. | Per-vertex colour in replay was not the cause. |
| Phase 3b: intercept `glBindTexture` and re-issue inside `wasm_flush_quad` | Probe `got_bind` now matches `last_bind` per helper. No visible texture sampling. | Binding is correct at draw time. Bind wasn't the proximate cause. |
| Phase 3c: force `GL_COLOR_MATERIAL` off on wasm | **Regressed to black scene + `WebGL: useProgram: object does not belong to this context`.** | The emulator's shader path depends on `COLOR_MATERIAL` being on; turning it off breaks the entire render. |
| vB2: skip `glColor4f` in `wasm_flush_quad` entirely | No visible change. Probe clean. | Per-quad colour setting was not the cause. |
| vB3: emit per-quad as `GL_TRIANGLE_STRIP` (4 verts) instead of `GL_TRIANGLES` (6 verts) | No visible change. centerPixel ~150. | Primitive type alone isn't the discriminator either. Same emulator path. |
| Initial Phase 3a refactor without naming the helper | Built but produced `GL_INVALID_ENUM: glDrawArrays: Invalid draw mode` errors. | Macro-recursion bug: `glBegin/glEnd` inside `wasm_vertex3f` re-expanded to the shims, clobbering `g_wasm_mode`. Fixed by extracting `wasm_flush_quad` as a helper defined *before* the macros take effect. |

## What was tried that exposed harness limits (round-4 only)

- `glIsEnabled(GL_TEXTURE_2D)` returned `0` for every glBegin probe,
  including the wood lane that demonstrably samples textures. Under
  WebGL2-backed `LEGACY_GL_EMULATION`, that getter doesn't accurately
  reflect the emulator's effective fixed-function state. Don't trust
  the `tex_enabled=0` line in any probe — only trust pixel-level
  visual evidence and `GL_TEXTURE_BINDING_2D`.

---

## Why Phase B is next

Round-3 documented the off-ramp condition:

> If the QUADS/texture-binding issues require fundamental rework of
> the immediate-mode shim, just write a small VBO renderer for the
> static geometry instead. The 90/10 rule from round 1 still applies
> — most geometry is static (walls, mural, ceiling, lanes); only the
> balls, pins, and animated effects need per-frame transforms. A VBO
> rewrite is targeted, not total.

Round-4 has now ruled out the following inside the shim:
- `glBindTexture` ordering (Phase 3b — verified correct via probe).
- Per-vertex `glColor4f` triggering implicit `COLOR_MATERIAL`
  (Phase 3a / vB2 — verified, not the cause).
- Primitive type (vB3 — `TRIANGLES` and `TRIANGLE_STRIP` both fail).
- Texture coordinate emission ordering (probe `has_t=1`, sensible
  values).

The remaining hypothesis space is "Emscripten's
`LEGACY_GL_EMULATION` does not correctly route texture sampling for
immediate-mode draws issued from inside a state-buffering layer."
That's not fixable in the shim layer.

**Recommended Phase B scope** (for the next session):

1. Compile a small VBO+shader pipeline under `#ifdef __EMSCRIPTEN__`.
   `LEGACY_GL_EMULATION=1` + custom shaders has known interop issues
   per round-1 notes — confirm via `EMCC_FLAGS` whether the legacy
   path coexists or needs to drop. If the legacy mode blocks shader
   compilation, switching to `LEGACY_GL_EMULATION=0` and rewriting
   the immediate-mode usage is a bigger change.
2. Bake the static surfaces into VBOs at startup:
   - `wall_texture` walls (`wall()` × 12 — 12 quads).
   - `mural_texture[0..3]` murals (4 quads).
   - `cieling_texture` ceiling tiles (1 quad per call ×
     N-per-lane × 4 lanes).
   - `duct_texture` duct cubes (4 quads × N-per-lane × 4 lanes).
   - `floor_texture` floor panels.
   - Optionally `alley()`'s wood strip (12 quad strips × 4 lanes) —
     this one already works through the shim, so skipping it is a
     valid scope-cap.
3. Per-frame: draw the bowling balls and pins through the existing
   shim (it works for those primitive types); draw the VBOs in one
   pass each before/after.
4. Keep native build untouched — gate every Phase-B path behind
   `#ifdef __EMSCRIPTEN__`.

The texture pipeline (LoadTexFromMemory, asset embedding) is already
fine and reusable. Camera/projection/lighting is fine. The diff
should land entirely in `final.c` (or a new `wasm_static_geom.c`
under WASM_SRCS) plus a small shader-source string. Avoid touching
`bowling.c` further — the round-1 helpers exist for the native build
and shouldn't be VBO-rewritten in place.

---

## File map (current state, all paths absolute under repo root)

| File | Round-4 changes |
|---|---|
| `bowling.c:19`   | `glDisable(GL_TEXTURE_2D)` moved after `glEnd()` in `cieling()`. |
| `bowling.c:604`  | Removed orphan `glBegin(GL_QUAD_STRIP)` in `alley()`; gutter loops are now the only Begin sources for that block. |
| `bowling.c:237`  | Removed orphan `glBegin(GL_QUAD_STRIP)` in `divider()`. Same pattern as `alley()`. |
| `wasm_compat.h:208-232` | New `wasm_replay` (normal+texcoord+vertex only) and `wasm_flush_quad` (re-bind, colour, per-quad real Begin/End). Both defined before the macros so calls inside resolve to real GL entry points. |
| `wasm_compat.h:216` | New `g_wasm_last_tex_2d` static, captured by the new shim. |
| `wasm_compat.h:266-273` | New `wasm_glBindTexture` shim + `#define` macro. |
| `wasm_compat.h:280-298` | `wasm_glBegin` now early-returns for `GL_QUADS`. |
| `wasm_compat.h:300-308` | `wasm_glEnd` is a no-op for `GL_QUADS`. |
| `wasm_compat.h:370-380` | `wasm_vertex3f` now calls `wasm_flush_quad()` instead of inlining the replay. |
| `final.c:217` | Build-version sentinel bumped to `vB4`. Round-5 should bump on every rebuild. |
| `WASM_DIAGNOSTICS_4.md` | This file. |

`AGENT_RUNBOOK.md` is unchanged — the probe still works as documented.

---

## Invariants (preserve into round 5)

- Native macOS build (`make clean && make && ./final`) must render
  identically. Verified after each round-4 phase.
- The three `bowling.c` UB fixes are correct on every platform — do
  not revert them when adding Phase B.
- The `gluLookAt` macro override (`wasm_compat.h:169-172`) is
  load-bearing for camera, kept.
- The `glBindTexture` shim is also load-bearing for the wood lane's
  per-iteration rebind tracking and for any Phase B that wants to
  inspect bound textures. Keep.
- Do not pursue `Phase 3c` (`GL_COLOR_MATERIAL` off) — regressed to
  black, the emulator depends on it.
- The probe still works end-to-end; trust `centerPixel` and per-quad
  `FLUSH[*]` probes (if re-added), do **not** trust
  `glIsEnabled(GL_TEXTURE_2D)`.
