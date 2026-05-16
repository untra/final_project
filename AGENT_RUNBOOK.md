# Agent runbook — iterating on the WASM port

You're a Claude session picking up the WASM bowling-alley port. This document
is the operating manual for the probe harness, not a history doc. For
historical context read [`WASM_DIAGNOSTICS.md`](WASM_DIAGNOSTICS.md) and
[`WASM_DIAGNOSTICS_2.md`](WASM_DIAGNOSTICS_2.md), in that order — they
explain *what we know* about the bug. This file explains *how to iterate*.

**Prefer the probe over manual browser checks.** Three previous sessions
burned hours partly on stale-wasm artifacts in manual incognito-window
workflows. The probe forces a fresh ephemeral Chromium profile and
cache-busts the wasm fetch via `Module.locateFile`. If you find yourself
reaching for "open the page in Chrome to check," stop and run the probe
instead.

---

## Day-zero setup (one time per machine)

```sh
source ~/emsdk/emsdk_env.sh   # emsdk is NOT on PATH; source per-shell
make wasm-probe                # first run: npm install + chromium download (~260MB total, slow)
make native-shot               # first run: triggers macOS Accessibility + Screen Recording prompts
                               # — Settings → Privacy & Security → Accessibility & Screen Recording,
                               #   add the terminal/agent process, restart it, retry. Cannot be automated.
```

`tools/node_modules` and the Playwright browser cache (`~/Library/Caches/ms-playwright/`)
are local to this machine and gitignored. The make rule `tools/node_modules/.installed`
is the sentinel that gates the npm install.

## The inner loop

```sh
source ~/emsdk/emsdk_env.sh    # once per shell session
make wasm-probe                 # rebuild wasm if needed + run probe
```

Three artifacts land in `build/web/probe/`:

| File | What | How to read |
|---|---|---|
| `latest.log`        | Full browser console transcript, line-prefixed `[browser:LEVEL] [wasm] ...` | Read with the Read tool. Grep for `LoadTexFromMemory`, `Aborted(`, `exception thrown:`, your own printfs. |
| `latest.state.json` | Viewport, context attrs, GL version, **center-pixel sample** | Read it. The center pixel is your binary "is the canvas blank?" check before you even look at the PNG. |
| `latest.png`        | Canvas screenshot at probe completion | Read with the Read tool (multimodal). |

Timestamped sibling copies (`<ISO>.log`, `<ISO>.png`, `<ISO>.state.json`)
are written alongside each `latest.*` so you can keep history without
manually copying.

For side-by-side with the native build:

```sh
make           # builds ./final if needed
make native-shot
```

→ `build/native/probe/latest.png`. Open both PNGs via Read.

## What to check on every run

Cheap mental checklist. Run top-to-bottom; the first failure is your next bug.

1. **Exit code is zero.** Non-zero → grep `latest.log` for `Aborted(` or
   `exception thrown:`. Those are the only patterns that flip the probe's
   exit code; everything else (including `pageerror` warnings, Emscripten's
   baseline "GL emulation" notices, GPU stall messages) is informational.
2. **`contextAttrs.preserveDrawingBuffer === true`** in `latest.state.json`.
   If false, the `web/shell.html` patch didn't take effect and your
   screenshots will be blank — fix that before trusting any visual diff.
   Sanity-check: `viewport[2..3]` should match `canvas.clientWidth/Height`.
3. **`viewport` non-zero** and reasonable for the canvas. `[0,0,0,0]` means
   `reshape()` never fired with real dimensions.
4. **`centerPixel` is not `[0,0,0,0]` or `[0,0,0,255]`**. Either of those =
   canvas is fully empty / clear-color-only. The PNG can look wrong for
   other reasons (camera, geometry, lighting) but centerPixel is the
   pixels-actually-touched check.
5. **≥10 `LoadTexFromMemory:` lines, then `WASM_PROBE_TEXTURES_LOADED`.**
   Missing → texture pipeline regression (the loader prints in
   `loadteximg.c:43-48`; the sentinel is in `final.c` right after the
   `LoadTexFromMemory` block).
6. **Compare PNG to the native screenshot** (run `make native-shot` first).
   Acceptable degradations on wasm are documented in
   `WASM_DIAGNOSTICS_2.md:386-399` — flat shading, missing specular,
   default-white surfaces on color-material-only meshes. Anything beyond
   those = bug.

If 2-5 pass and the PNG still looks wrong, the failure is in scene
rendering. Likely culprits live in `wasm_compat.h`: the immediate-mode
QUADS buffering (lines 216-315), the `glColorMaterial` shim
(lines 78-100), the `gluLookAt` matrix construction (lines 143-168).

## CLI options

```sh
node tools/wasm-probe.mjs --duration=5000     # idle animation runs longer
node tools/wasm-probe.mjs --timeout=30000     # extend page.goto timeout
node tools/wasm-probe.mjs --out=tmp/probe     # change artifact directory
node tools/wasm-probe.mjs --headed            # show the browser window (debug)
node tools/wasm-probe.mjs --keep-server       # leave http server up after exit
```

## Cache-busting failure modes (round-2 footgun)

The probe defends against stale-wasm via two layers:

- Fresh ephemeral Chromium profile each `chromium.launch()` (no disk cache).
- Per-page-load query string on the wasm/data URLs via `Module.locateFile`
  in `web/shell.html`.

**But** if you edit `web/shell.html` itself, the HTML response can be cached
by any intermediate (browser, CDN if you've deployed). Run
`make clean-wasm && make wasm-probe` after shell edits to be sure.

If results look impossible given your last edit:
1. Confirm the build actually rebuilt:
   `shasum build/web/index.wasm` before and after `make wasm`.
2. Add a build-version sentinel to the C code:
   ```c
   #ifdef __EMSCRIPTEN__
   {
       static int v_logged = 0;
       if (!v_logged) { printf("WASM BUILD vN: <note>\n"); v_logged = 1; }
   }
   #endif
   ```
   Bump N every rebuild. Look for it in `latest.log`.

## Common loop patterns

### "Does this shim do anything?"
1. `make wasm-probe`. Save the screenshot as `before.png`.
2. Comment out the shim under `#ifdef __EMSCRIPTEN__` in
   `wasm_compat.h` or `final.c`.
3. `make wasm-probe`. Compare PNGs.

### "Did my last edit make it worse?"
Before rebuilding: `cp build/web/probe/latest.png build/web/probe/before-X.png`.
Run probe. Diff.

### "Is the modelview matrix actually changing per frame?"
In `display()`, right after the `gluLookAt` call:
```c
#ifdef __EMSCRIPTEN__
{
    static int mv_logged = 0;
    if (mv_logged < 3) {
        GLfloat m[16];
        glGetFloatv(GL_MODELVIEW_MATRIX, m);
        printf("MV[%d]: %.2f %.2f %.2f %.2f / %.2f %.2f %.2f %.2f / %.2f %.2f %.2f %.2f / %.2f %.2f %.2f %.2f\n",
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
`make wasm-probe`. Read `latest.log`. If the first row is `1 0 0 0` and the
fourth column is also `0 0 0 1`, the matrix is identity → `gluLookAt` shim
isn't taking. (The `wasm_compat.h:143-172` shim should produce a non-identity
matrix for any non-trivial camera.)

### "Why is a particular surface missing?"
The probe's center-pixel sample only checks one location. Extend the in-page
`page.evaluate` block in `tools/wasm-probe.mjs:state-dump` to sample several
points (top-left, top-right, bottom-left, bottom-right, center). If the
borders are non-zero but the center isn't, the camera's looking past the
geometry. If the center is non-zero but you can't see the surface in the
PNG, the surface is being drawn in the wrong color (probably the
`glMaterialfv`/`glColorMaterial` interaction inside Begin/End — see
`wasm_compat.h:263-294`).

### "I need to test interaction (key press, mouse drag)"
The probe is a one-shot snapshot. Extend it with a `--script=<file>` flag
that takes a JS file Playwright runs against the page between load and
state-dump — example:
```js
// tools/scripts/press-1.js
await page.locator('#canvas').click();
await page.keyboard.press('1');
await new Promise(r => setTimeout(r, 1500));
```
Add the flag to `tools/wasm-probe.mjs` rather than building a second
harness.

## When NOT to use the probe

- **Native build verification.** Use `make native-shot` instead. The probe
  knows nothing about the native binary.
- **Long-running animation correctness.** Probe defaults to 3000ms settle.
  Bump `--duration` if you need a specific animation phase, but the probe
  still gives you exactly one frame.
- **Diffing texture pixels.** The screenshot is full canvas. If you need to
  verify "this texture is sampled correctly on this surface," extend the
  state-dump with targeted `gl.readPixels` calls, don't try to read it out
  of the PNG.

## File map

| File | Purpose |
|---|---|
| `tools/package.json`           | Playwright dev dep, scoped here so `node_modules` doesn't pollute repo root |
| `tools/wasm-probe.mjs`         | The probe driver. Owns the http server, browser, console capture, state dump, screenshot |
| `tools/native-shot.sh`         | macOS-only. Launches `./final`, grabs its window via `AXWindowNumber`, kills it |
| `web/shell.html`               | Emscripten HTML shell. Patched with `webglContextAttributes`, `locateFile` cache-buster, `getContext` belt-and-suspenders, `Module.print`/`printErr` prefixing |
| `final.c:802-808`              | `WASM_PROBE_TEXTURES_LOADED` sentinel printf, `#ifdef __EMSCRIPTEN__`-gated |
| `wasm_compat.h:143-172`        | The `gluLookAt` shim that round 2's smoking gun pointed at. Not yet verified by a controlled with/without diff — that's your next experiment |
| `loadteximg.c:43-48`           | Per-texture diagnostic printfs the probe consumes |
| `makefile:wasm-probe`, `native-shot`, `clean-probe` | Targets added by this work |

## What the harness will NOT save you from

- Permission prompts on first `make native-shot` (macOS, one-time).
- A genuinely broken wasm build (compile errors → `make wasm` exits non-zero
  before the probe runs).
- Wasm-side `Fatal(...)` calls that abort the runtime — those appear in
  `latest.log` as `Aborted(...)` and the probe correctly exits non-zero,
  but the only diagnostic info is what `Fatal` printed before it aborted.
  If you need richer state, add `printf`s before the call site.
- Diff-on-the-server-side workflows. The probe writes to `build/web/probe/`
  on disk. If you want CI integration, compare PNGs and JSON outside the
  harness.

---

**Hand-off etiquette:** if you finish a substantial debug session, append a
short section to this file (or write `WASM_DIAGNOSTICS_3.md`) summarizing
what was tried, what worked, what didn't, and what the next session should
investigate first. The user expects this — it's how the project has stayed
debuggable across multiple Claude sessions.
