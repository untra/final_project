#!/usr/bin/env node
// Headless-browser probe for the WASM build.
//
// Boots a local http server on a random port, opens build/web/index.html in
// headless Chromium, captures console output + a GL state dump + a canvas
// screenshot, and exits non-zero on Aborted(...) or thrown exceptions.
//
// Artifacts written under build/web/probe/ (override with --out):
//   latest.log         — full browser console transcript
//   latest.state.json  — viewport, context attrs, GL version, center-pixel sample
//   latest.png         — canvas screenshot
//   <ISO>.log/.json/.png — timestamped sibling copies for history
//
// Usage:
//   node tools/wasm-probe.mjs                         # default: 3s settle
//   node tools/wasm-probe.mjs --duration=5000
//   node tools/wasm-probe.mjs --headed                # visible Chromium
//   node tools/wasm-probe.mjs --keep-server           # leave http server up
//
// See AGENT_RUNBOOK.md for the iteration loop this fits into.

import { chromium } from 'playwright';
import { spawn } from 'node:child_process';
import { mkdir, writeFile, copyFile } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { randomUUID } from 'node:crypto';
import http from 'node:http';
import net from 'node:net';

const __dirname = dirname(fileURLToPath(import.meta.url));
const repoRoot  = resolve(__dirname, '..');

const args = Object.fromEntries(
  process.argv.slice(2).flatMap(a => {
    if (!a.startsWith('--')) return [];
    const eq = a.indexOf('=');
    if (eq === -1) return [[a.slice(2), 'true']];
    return [[a.slice(2, eq), a.slice(eq + 1)]];
  })
);
const duration = parseInt(args.duration || '3000', 10);
const timeout  = parseInt(args.timeout  || '15000', 10);
const outDir   = resolve(repoRoot, args.out || 'build/web/probe');
const headed   = args.headed === 'true';
const keepSrv  = args['keep-server'] === 'true';

function freePort() {
  return new Promise((res, rej) => {
    const srv = net.createServer();
    srv.unref();
    srv.on('error', rej);
    srv.listen(0, '127.0.0.1', () => {
      const { port } = srv.address();
      srv.close(() => res(port));
    });
  });
}

function fetchOnce(url) {
  return new Promise((res, rej) => {
    const req = http.get(url, r => {
      let body = '';
      r.on('data', d => body += d);
      r.on('end', () => res({ status: r.statusCode, body }));
    });
    req.on('error', rej);
    req.setTimeout(2000, () => req.destroy(new Error('fetchOnce timeout')));
  });
}

const buildWeb = resolve(repoRoot, 'build/web');
if (!existsSync(join(buildWeb, 'index.html'))) {
  console.error(`probe: ${join(buildWeb, 'index.html')} missing. Run 'make wasm' first.`);
  process.exit(2);
}
await mkdir(outDir, { recursive: true });

const port = await freePort();
console.log(`probe: spawning http server on :${port}`);
const server = spawn('/usr/bin/python3', ['-m', 'http.server', '-d', buildWeb, String(port)], {
  stdio: ['ignore', 'ignore', 'inherit'],
});
const cleanupServer = () => {
  if (!keepSrv) {
    try { server.kill('SIGTERM'); } catch {}
  } else {
    console.log(`probe: --keep-server set, leaving server alive on :${port} (pid ${server.pid})`);
  }
};
process.on('exit', cleanupServer);
process.on('SIGINT',  () => { cleanupServer(); process.exit(130); });
process.on('SIGTERM', () => { cleanupServer(); process.exit(143); });

// Wait for the server to be up AND serving the right content.
let ready = false;
for (let i = 0; i < 50 && !ready; i++) {
  await new Promise(r => setTimeout(r, 100));
  try {
    const r = await fetchOnce(`http://127.0.0.1:${port}/index.html`);
    if (r.status === 200 && r.body.includes('Bowling Alley')) {
      ready = true;
    }
  } catch {}
}
if (!ready) {
  console.error('probe: http server never came up or served wrong content');
  cleanupServer();
  process.exit(2);
}

const probeId = randomUUID();
const stamp   = new Date().toISOString().replace(/[:.]/g, '-');
const logLines = [];
let fatal = null;

function emit(level, text) {
  const line = `[browser:${level}] ${text}`;
  console.log(line);
  logLines.push(line);
  if (text.includes('Aborted(') || text.includes('exception thrown:')) {
    fatal = fatal || text;
  }
}

const browser = await chromium.launch({
  headless: !headed,
  args: ['--use-gl=swiftshader', '--ignore-gpu-blocklist'],
});
const ctx  = await browser.newContext({ viewport: { width: 1280, height: 900 } });
const page = await ctx.newPage();

page.on('console',   msg => emit(msg.type().toUpperCase(), msg.text()));
page.on('pageerror', err => emit('PAGEERROR', err.message));
page.on('crash',     ()  => { fatal = fatal || 'page crash'; emit('CRASH', 'page crashed'); });

const url = `http://127.0.0.1:${port}/?probe=${probeId}`;
console.log(`probe: opening ${url}`);

try {
  await page.goto(url, { waitUntil: 'load', timeout });

  // Frame-ready: two rAFs guarantees one fully-composited frame.
  await page.evaluate(() =>
    new Promise(r => requestAnimationFrame(() => requestAnimationFrame(r)))
  );

  // Let the idle animation run.
  await new Promise(r => setTimeout(r, duration));

  // GL state dump.
  const state = await page.evaluate(() => {
    const c = document.getElementById('canvas');
    if (!c) return { error: 'no canvas element' };
    const gl = c.getContext('webgl2') || c.getContext('webgl');
    if (!gl) return { error: 'no gl context' };
    const px = new Uint8Array(4);
    try {
      gl.readPixels(c.width >> 1, c.height >> 1, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, px);
    } catch (e) {
      // readPixels can throw on some headless configs; continue.
    }
    return {
      canvas: {
        width:        c.width,
        height:       c.height,
        clientWidth:  c.clientWidth,
        clientHeight: c.clientHeight,
      },
      viewport:     Array.from(gl.getParameter(gl.VIEWPORT)),
      contextAttrs: gl.getContextAttributes(),
      version:      gl.getParameter(gl.VERSION),
      vendor:       gl.getParameter(gl.VENDOR),
      renderer:     gl.getParameter(gl.RENDERER),
      shadingLang:  gl.getParameter(gl.SHADING_LANGUAGE_VERSION),
      centerPixel:  Array.from(px),
    };
  });

  const stateText = JSON.stringify(state, null, 2);
  await writeFile(join(outDir, 'latest.state.json'), stateText);
  await writeFile(join(outDir, `${stamp}.state.json`), stateText);

  // Multi-angle canvas screenshots.
  //
  // angle-0: spawn pose (default first-person camera, no input).
  // angle-1: press 'p' twice to cycle mode 1 -> 2 -> 0 (orbit), which resets
  //          th=ph=0, xOffset=yOffset=0, zOffset=2*dim and gives a clean
  //          centered orbit view of the whole alley.
  // angle-2: from angle-1 orbit pose, ArrowRight x8 -> th += 40 deg, rotating
  //          horizontally to see the far end and side walls.
  // angle-3: from angle-2 pose, ArrowUp x6 -> ph += 30 deg, tilting the view
  //          up toward the ceiling/duct row.
  //
  // Key events are dispatched to the focused canvas; GLUT-on-Emscripten
  // intercepts them via the document-level handler. waitForTimeout(80) per key
  // gives the idle callback room to redraw between presses; the final 200ms
  // settle before each screenshot is for the next frame's compositor pass.
  const canvasLoc = page.locator('#canvas');
  await canvasLoc.focus();

  const angles = [
    { name: 'angle-0', keys: [] },
    { name: 'angle-1', keys: ['KeyP', 'KeyP'] },
    { name: 'angle-2', keys: ['ArrowRight', 'ArrowRight', 'ArrowRight', 'ArrowRight',
                              'ArrowRight', 'ArrowRight', 'ArrowRight', 'ArrowRight'] },
    { name: 'angle-3', keys: ['ArrowUp', 'ArrowUp', 'ArrowUp', 'ArrowUp', 'ArrowUp', 'ArrowUp'] },
  ];

  // angle-0 keeps the original `latest.png` filename for backward compat with
  // pre-multi-angle consumers; all four angles also get explicit suffixed names.
  let firstShotPath = null;
  for (const a of angles) {
    for (const k of a.keys) {
      await page.keyboard.press(k);
      await page.waitForTimeout(80);
    }
    await page.waitForTimeout(200);
    const namedPath = join(outDir, `latest.${a.name}.png`);
    await canvasLoc.screenshot({ path: namedPath });
    await copyFile(namedPath, join(outDir, `${stamp}.${a.name}.png`));
    if (firstShotPath === null) firstShotPath = namedPath;
  }
  const pngPath = join(outDir, 'latest.png');
  await copyFile(firstShotPath, pngPath);
  await copyFile(firstShotPath, join(outDir, `${stamp}.png`));

  console.log(`probe: state -> ${join(outDir, 'latest.state.json')}`);
  console.log(`probe: shots -> ${angles.map(a => `latest.${a.name}.png`).join(', ')}`);
  console.log(`probe: centerPixel = ${JSON.stringify(state.centerPixel)}`);
  console.log(`probe: viewport    = ${JSON.stringify(state.viewport)}`);
  console.log(`probe: preserveDrawingBuffer = ${state.contextAttrs && state.contextAttrs.preserveDrawingBuffer}`);
} catch (err) {
  fatal = fatal || err.message;
  emit('PROBE-ERROR', err.stack || err.message);
} finally {
  const logText = logLines.join('\n') + '\n';
  await writeFile(join(outDir, 'latest.log'),     logText);
  await writeFile(join(outDir, `${stamp}.log`),   logText);
  await browser.close();
  cleanupServer();
}

if (fatal) {
  console.error(`probe: FAILED - ${fatal}`);
  process.exit(1);
}
console.log(`probe: ok`);
