# WASM build diagnostic handoff (round 3)

Read [`WASM_DIAGNOSTICS.md`](WASM_DIAGNOSTICS.md) and
[`WASM_DIAGNOSTICS_2.md`](WASM_DIAGNOSTICS_2.md) first for context. This is
the round-3 addendum.

What round 3 actually did: **stood up an automated probe harness** so we can
read browser console output and capture canvas screenshots in one command,
without a human in the loop. Then used the harness to **conclusively verify
the round-2 `gluLookAt` hypothesis** via a controlled A/B test.

For workflow documentation — how to run the probe, what artifacts it writes,
how to iterate — read [`AGENT_RUNBOOK.md`](AGENT_RUNBOOK.md). This file is
the *findings* doc, not the *tools* doc.

---

## TL;DR — what is conclusively known now

1. **The `gluLookAt` shim works.** Round-3 A/B controlled test:
   - With `#define gluLookAt(...)` macro override active in
     `wasm_compat.h:169-172` → camera responds correctly, alley interior is
     visible from the first-person spawn point. Screenshot:
     `build/web/probe/with-shim.png`.
   - With the same `#define` commented out (Emscripten's broken native
     `gluLookAt` running) → only a brick-wall corner fragment visible in
     the upper-right, rest is empty clear-color. Modelview is identity, so
     the alley is behind/outside the frustum. Screenshot:
     `build/web/probe/without-shim.png`.
   - Both screenshots captured by `make wasm-probe` from a `make clean-wasm`
     baseline so cache effects are ruled out.
   - The round-2 smoking gun ("`gluLookAt` silently does nothing under
     `LEGACY_GL_EMULATION`") is confirmed. The `wasm_glu_lookat`
     replacement that constructs the view matrix and calls `glLoadMatrixf`
     is doing real work.
2. **The remaining wasm brokenness is downstream of the camera.** The
   with-shim screenshot shows the camera is correctly placed (you can see
   the wood lane texture running away from the viewer at center-bottom),
   but:
   - Most surfaces are flat gray triangles instead of the native's textured
     mural/wall/ceiling
   - Geometry has visible diagonal seams (the `GL_QUADS → GL_TRIANGLES`
     re-emission in `wasm_compat.h:299-310`)
   - The roof/dividers appear as overlapping wedges rather than coherent
     planes
3. **The probe is a verified iteration loop.** All the assertions from
   `AGENT_RUNBOOK.md`'s checklist pass on the current build:
   `centerPixel != 0`, `preserveDrawingBuffer == true`, `viewport` correct,
   10 `LoadTexFromMemory:` log lines, `WASM_PROBE_TEXTURES_LOADED` printed,
   exit code 0.

The previous diagnostic doc's hypothesis ranking was correct. The
gluLookAt shim is necessary; it is also not sufficient.

---

## What the harness changed (so you can use it)

| Added / modified | Why |
|---|---|
| `tools/wasm-probe.mjs` | Playwright-based driver: spawns http server on a random port, opens headless Chromium, captures console + GL state JSON + canvas PNG, exits nonzero on `Aborted(` / `exception thrown:` |
| `tools/native-shot.sh` | macOS-only. Launches `./final`, grabs window via AXWindowNumber, kills it. Needs Accessibility + Screen Recording permission once. |
| `tools/package.json` | Scoped Playwright dev dep so `node_modules` doesn't pollute the C-project root |
| `web/shell.html` | `Module.webglContextAttributes` for `preserveDrawingBuffer` (probe screenshots are blank without it), `Module.locateFile` cache-buster on wasm/data fetches (kills the round-2 stale-wasm footgun), `Module.print`/`printErr` prefixed `[wasm]` so the probe can grep |
| `final.c` (post-LoadTexFromMemory block) | Single `printf("WASM_PROBE_TEXTURES_LOADED\n")` sentinel, `#ifdef __EMSCRIPTEN__`-gated. Informational; the probe's frame-ready gate is a `requestAnimationFrame` ping, not this line |
| `makefile` | `make wasm-probe` + `make native-shot` + `make clean-probe` targets, plus `tools/node_modules/.installed` install rule |
| `.gitignore` | `tools/node_modules/`, `tools/package-lock.json` |

**A landmine I tripped that future you should not re-trip:** Don't write the
literal string `{{{ SCRIPT }}}` inside any JS comment in `web/shell.html`.
Emscripten substitutes that marker anywhere it appears, and the substitution
contains a literal `</script>` tag — which the browser parses as ending the
script block even inside JS comments. This produces an "Unexpected end of
input" page error and a non-functional Module. See `web/shell.html` for the
fixed comment wording.

---

## What's left (highest-signal first)

### 1. Quad triangulation is leaving visible seams

The fragmented gray triangles in `with-shim.png` aren't a hallucination —
they're the diagonal of each emulated `GL_QUADS` quad. `wasm_compat.h`
turns every 4-vertex quad into 2 triangles `(V0,V1,V2)` + `(V0,V2,V3)`. If
V0..V3 aren't coplanar (or are degenerate), the seam between the two
triangles is visible. The native build never had this problem because
`GL_QUADS` is a real primitive there.

Things to check:
- Are the quads emitted by `bowling.c` actually coplanar? Spot-check one
  helper that draws a wall (e.g. `wall()` or `mural()`) and verify all 4
  vertices share a plane.
- Is the texture coordinate replay (`wasm_compat.h:208-214`) attaching the
  right `(s, t)` to each replayed vertex? If `g_wasm_buf[0].t != g_wasm_buf[0].t`
  (i.e. the per-vertex texcoord captured at buffer time and at replay time)
  differ, surfaces will sample wrong.

### 2. Most textures don't appear on surfaces

Round 2 found that exactly one wood-grain quad rendered. Round 3 confirms
the wood-grain (lane) texture still renders; nothing else does. The other
9 textures load successfully (`LoadTexFromMemory:` log lines, all
non-zero ids), so binding state must be getting lost between
`glBindTexture` and `glBegin(GL_QUADS)`/draw.

Hypothesis (from round 2's notes, still untested): the per-vertex
`glColor3f` / `glMaterialfv` interaction inside `glBegin`/`glEnd`
(`wasm_compat.h:263-294`) clobbers the bound texture under
`LEGACY_GL_EMULATION`. The gate at line 278 (`g_wasm_cm_on && g_wasm_mode == 0`)
prevents the material update from firing inside Begin/End, but if any
remaining path *does* fire, only the first textured surface (drawn before
the clobber) is visible.

Next experiment: add a `glGetIntegerv(GL_TEXTURE_BINDING_2D, &t)` printf
inside the loop in `wasm_compat.h:wasm_vertex3f` (right before the replay
on `g_wasm_n == 4`) so the probe captures whether the binding survives
each quad. Likely it changes after the first quad.

### 3. The VBO/retained-mode rewrite is still on the table

Round 1 flagged this as the Phase-B off-ramp. With the gluLookAt shim
confirmed, the Phase-B threshold is now: "if the QUADS/texture-binding
issues require fundamental rework of the immediate-mode shim, just write
a small VBO renderer for the static geometry instead." The lighting is
also currently disabled on wasm (see `final.c` `#ifdef __EMSCRIPTEN__`
right before scene helpers), so most of the work would be in baking the
~10 surface meshes into VBOs at startup and writing one trivial
vertex+fragment shader pair.

The 90/10 rule from round 1 still applies — most geometry is static
(walls, mural, ceiling, lanes); only the balls, pins, and animated
effects need per-frame transforms. A VBO rewrite is targeted, not
total.

---

## How to reproduce the round-3 verification

```sh
source ~/emsdk/emsdk_env.sh
make clean-wasm && make wasm-probe
cp build/web/probe/latest.png build/web/probe/with-shim.png

# Comment out the #define gluLookAt block at wasm_compat.h:169-172
make clean-wasm && make wasm-probe
cp build/web/probe/latest.png build/web/probe/without-shim.png

# Restore the macro, build back to baseline
make clean-wasm && make wasm-probe
```

The artifacts are already in `build/web/probe/` if the directory survived.

---

## Invariants and caveats

- The shim restoration step is critical — never leave the macro
  commented out across a commit. The `make clean-wasm` between toggles is
  also critical (cached `index.wasm` will lie to you, see
  `WASM_DIAGNOSTICS_2.md:234-242`).
- `centerPixel` varies between probe runs at the same camera because the
  idle animation is running (ball + pin animation). Don't treat
  `centerPixel` as a deterministic fingerprint — only as a "is the pixel
  non-zero" sanity check.
- The viewport in the probe is `[0,0,934,622]` because the CSS in
  `web/style.css` sizes the canvas down from its 1200×720 attribute size
  and `reshape()` follows the client size. Native is the canvas's full
  declared size. This is cosmetic, not a bug — but if you ever need pixel
  parity for diffing, force the canvas's CSS to `width: 1200px; height: 720px`.
- `make native-shot` requires a one-time macOS Accessibility + Screen
  Recording grant for whatever terminal/process is invoking it. There is
  no permission-free path. Document this if you hand the project to a new
  machine.
