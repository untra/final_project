# WASM build diagnostic handoff (round 7)

Read [`WASM_DIAGNOSTICS.md`](WASM_DIAGNOSTICS.md),
[`WASM_DIAGNOSTICS_2.md`](WASM_DIAGNOSTICS_2.md),
[`WASM_DIAGNOSTICS_3.md`](WASM_DIAGNOSTICS_3.md),
[`WASM_DIAGNOSTICS_4.md`](WASM_DIAGNOSTICS_4.md),
[`WASM_DIAGNOSTICS_5.md`](WASM_DIAGNOSTICS_5.md), and
[`WASM_DIAGNOSTICS_6.md`](WASM_DIAGNOSTICS_6.md) first. Tooling docs in
[`AGENT_RUNBOOK.md`](AGENT_RUNBOOK.md).

Round 7 (`vB29`) **falsified round 6's conclusion** that bowling pins
render correctly on WASM, and **fixed the actual invisibility bug**. The
fix is a one-line semantic refactor in `bowling.c::bowling_pin()`: hoist
every `glColor3f` outside its enclosing `glBegin`/`glEnd` block.

---

## TL;DR — what round 7 established

1. **Round 6's pin matrix probe was misleading.** `mvdiag=(-0.25, 0.25,
   -0.25)` and NDC `(-0.162, -0.005)` were geometrically correct but did
   not prove pins reached the rasterizer. The probe never checked
   fragments. Visual reasoning falsified the conclusion: pins at z=111–115
   sit *behind* the murals at z=108–110, so the missing-mural void from
   round 6 Open Problem 1 should have made pins visible through the gap.
   None were. So pins were either not rasterizing or rasterizing
   invisibly.
2. **Round-7 discriminator probes (A=force-magenta + B=invocation-count +
   C=GL-state-dump in pin + D=same-state-dump in ball) settled it
   immediately.** Pin loop ran 80 times per frame as expected (Probe B).
   Force-magenta pins were fully visible filling the close-up angle-7
   frame (Probe A). State dump showed `GL_CURRENT_COLOR = (0,0,0,0)` at
   pin entry — same as ball entry — and the ball renders fine because
   `bowling_ball()` calls `glColor3f(1,1,0)` *before* its first
   `glBegin` (at `bowling.c:1000`), while `bowling_pin()` historically
   called `glColor3f` *inside* its first `glBegin`.
3. **Root cause: Emscripten's `LEGACY_GL_EMULATION` does not honor
   `glColor3f` calls issued inside `glBegin`/`glEnd`.** The first vertex
   of each Begin block is emitted with `GL_CURRENT_COLOR` as it stood at
   `glBegin` time, and that state stays for the whole block — in-Begin
   `glColor3f` calls are silently dropped. Pin code at `bowling.c:81`,
   `:106`, `:110`, etc. set the color *inside* each `glBegin`, so the
   color stuck at whatever it was beforehand (which the VBO draw clobbered
   to alpha=0). White-with-alpha-0 pins produced no fragments.
4. **Fix:** moved every `glColor3f` in `bowling_pin()` from inside its
   `glBegin` to immediately before it. The native build's behavior is
   unchanged because `glColor3f` before `glBegin` updates
   `GL_CURRENT_COLOR`, which the next vertex emission uses — identical
   semantics on a spec-compliant GL implementation. Verified:
   `make wasm-probe` shows pins now visible across angles 1/3/5/7 with
   white body + red stripes; `make && ./final` builds clean and renders
   unchanged.

---

## Round-7 changes (final, all probes reverted)

| File | Change |
|---|---|
| `bowling.c::bowling_pin` (lines 65–215) | Hoisted 10 `glColor3f` calls outside their `glBegin` blocks. The k=13 red-stripe conditional moved out of the loop's `glBegin` body. Added one explanatory comment block. |
| `wasm_static_geom.c::wasm_static_geom_draw` (lines 414–490) | Snapshot `GL_CURRENT_COLOR` at entry and restore at exit via `glColor4f`. Defensive against future immediate-mode draws that rely on `GL_CURRENT_COLOR` propagating across the VBO draw call. **Not load-bearing for the pin fix** — left in because it costs nothing and protects future shim work. |
| `final.c:220` | Sentinel bumped to `vB29: round-7 pin visibility fix — hoist glColor3f outside glBegin in bowling_pin (LEGACY_GL_EMULATION drops in-Begin color)`. |
| `WASM_DIAGNOSTICS_7.md` | This file. |

**No code changes** to `wasm_compat.h`, `final.c::display()`,
`tools/wasm-probe.mjs`. The round-6 invariants from
`WASM_DIAGNOSTICS_6.md:222-244` are all preserved.

---

## Why round 6 was wrong

Round 6's pin probe instrumented the matrix state at the first pin
draw and concluded "the scale is correct, the projection is correct,
therefore pins render correctly but small." The probe did not test
whether the rasterizer produced fragments at the projected location.
A correctly-projected primitive with `alpha=0` per-vertex color produces
zero fragments under whatever alpha-test or shader-discard path the
emulator uses, and `glGetFloatv(GL_MODELVIEW_MATRIX)` cannot detect
that.

The round-6 claim "angle-7 shows the overlapping triangle pattern …
consistent with 80 correctly-sized pins viewed up close"
(`WASM_DIAGNOSTICS_6.md:202-204`) is contradicted by the actual
`latest.angle-7.png` artifacts from round 6's own probe run — which
show two converging wood lanes and no pin shapes at all. The "triangle
pattern" was the lane wood receding to the vanishing point, not pins.

This is the second consecutive round of overconfident visual claims
(round 5 made one too — `WASM_DIAGNOSTICS_5.md:50` misidentified the
yellow light-marker as oversized pins). Future rounds: when the
"correctness claim" depends on a visual reading, also read the PNG
independently of the probe data that supports it. The pixels are
ground truth; matrix dumps and counts are inference.

---

## How to verify the fix

```sh
source ~/emsdk/emsdk_env.sh
make wasm-probe
```

Read `build/web/probe/latest.angle-7.png`: the upper-left half of the
frame should be filled with polygonal pin geometry showing light/shadow
gradients (lit by the scene lights). `latest.angle-1.png` should show a
small triangular pin cluster near frame center with visible red-stripe
detail. `latest.angle-5.png` should show pins as a discrete row at the
far end of the wood lane.

`centerPixel` reads `[198, 198, 198, 255]` after the fix (near-white
pin against the back wall, not the all-grey `[210, 210, 210]` of the
broken state).

Native:

```sh
make clean && make && ./final
```

Compiles clean (one pre-existing `-Wunused-but-set-variable` warning
about `f` in `bowling_pin` that predates round 7). Visual render
unchanged from prior rounds.

---

## Open problems carried into round 8+

The user explicitly scoped round 7 to pin visibility. Two problems
from prior rounds remain:

1. **Murals at z=108–110 do not rasterize on WASM.** (Round-6 Open
   Problem 1.) Geometry is queued, batches log correctly, but only a
   single ~5 px speck of the mural row appears. Round 6's leading
   hypothesis is a WebGL depth-precision artifact compounded by the
   2-unit z-tilt of the mural quad (bottom z=110, top z=108).
   Recommended first experiment: relocate murals to z=70, z=90 to find
   the failure threshold (round 6 already proved a control mural at
   z=–43 renders fully). The pin-visibility fix means pins will now be
   visible *through* the missing-mural void — that may visually mask
   the mural absence at the spawn-view angle, so prioritize the
   back-wall framing (`angle-6`) for visual confirmation when working
   the mural fix.
2. **Ball returns / dividers / caps / upcurves are missing on WASM.**
   (Round-5 Open Problem 4.) `double_lane()` is still gated out at
   `final.c:382-393`; the VBO replacement in `wasm_static_geom.c` does
   not include the dividers, ball returns, upcurves, or caps. The user
   confirmed in round 7 that "lane interior empty (no separators /
   returns visible)" is the visible-from-spawn complaint that motivated
   the round-7 scoping discussion. Adding `build_ball_returns`,
   `build_dividers`, `build_caps`, `build_upcurves` to
   `wasm_static_geom.c` is the round-8 work. The shader must learn
   `uniform bool u_use_texture` (or accept a sentinel texture of
   all-white pixels) since these helpers use only `glColor3f` — and
   per the round-7 finding, *that* must be glColor3f-before-glBegin if
   they're ever ported to the immediate-mode shim path instead. The
   VBO path bakes color into vertex data, so the shader path is the
   safest choice.

---

## Possibly-related future cleanup (not blocking)

The `glColor3f`-inside-`glBegin` bug almost certainly affects other
helpers in `bowling.c` that use the same pattern:

| Helper | Likely affected? |
|---|---|
| `divider()` (`bowling.c:277`) | YES — `glColor3f` inside `glBegin` at line 281 |
| `upcurve()` (`bowling.c:319`) | YES — same pattern |
| `cap()` (`bowling.c:583`) | YES — same pattern |
| `ball_return_body()` (`bowling.c:375`) | YES — many in-Begin `glColor3f` |
| `cube()` (`bowling.c:633`+) | Likely |
| `pyramid()` (`bowling.c:654`+) | Likely |
| `bowling_ball()` (`bowling.c:984`) | NO — color is set *before* glBegin |
| `alley()` (textured) | NO — textures dominate over per-vertex color |

These don't matter today because (a) `double_lane()` is gated out on
WASM, so divider/upcurve/cap/ball_return_body never run there; and
(b) `cube()`/`pyramid()` aren't called from `display()` directly
without being inside something else. When round 8 ports the
double_lane interior to VBOs, those helpers won't be reused via the
immediate-mode path anyway — the VBO approach bakes color into vertex
data, sidestepping the bug entirely.

---

## Invariants (preserve into round 8)

- Native macOS build (`make clean && make && ./final`) renders
  identically — verified after round 7.
- The round-7 `bowling_pin` color hoist is cross-platform-safe; do not
  revert. Native and WASM both rely on `glColor3f` being called *before*
  each `glBegin`.
- `wasm_static_geom_draw` now saves/restores `GL_CURRENT_COLOR` in
  addition to `GL_CURRENT_PROGRAM`/`GL_ARRAY_BUFFER_BINDING`/
  `GL_ACTIVE_TEXTURE`/`GL_TEXTURE_BINDING_2D`. Keep all five saves.
- The three `bowling.c` UB fixes from round 4 stay in.
- The `gluLookAt` macro override (`wasm_compat.h`) and the
  `glBindTexture` shim stay.
- Do not "fix" `bowling_pin`'s `glScaled` based on the round-5
  oversized-pin hypothesis — pin geometry math is correct on both
  platforms; the round-7 fix addressed the actual visibility bug.
- `glGetFloatv(GL_CURRENT_COLOR, ...)` may return stale data under
  `LEGACY_GL_EMULATION`. Round 7 saw it report `(0,0,0,0)` even after
  the in-`wasm_static_geom_draw` save/restore set it to `(1,1,1,1)` —
  consistent with the round-4 caveat that `glIsEnabled(GL_TEXTURE_2D)`
  also lies under the emulator. Trust pixels (`centerPixel`, PNGs)
  over getter return values when probing GL state on WASM.
- The probe sentinel pattern at `final.c:217-223` is the only defense
  against stale-wasm artifacts surviving across builds. Bump on every
  round-8 rebuild that touches generated output.

---

## Recommended round-8 entry sequence

1. Read this file + `WASM_DIAGNOSTICS_6.md` (for the mural-rasterization
   problem) and `WASM_DIAGNOSTICS_5.md:236-260` (for the double_lane
   port scope).
2. `source ~/emsdk/emsdk_env.sh && make wasm-probe` — confirm vB29 still
   runs clean and pins remain visible.
3. Pick **one** of the two open problems with the user — mural fix is
   smaller (one VBO geometry tweak, possibly a shader change); ball-
   return port is bigger (4 new build_* helpers + shader changes for
   untextured colored geometry).
4. Bump sentinel to vB30 on entry.
