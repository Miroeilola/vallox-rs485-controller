// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Wiring: WASM module → Display (2D canvas) → Loop (time, keyboard) → 3D scene
// and side panel. window.__vallox exposes the pieces for the smoke test and for
// poking around in the console; nothing else reads it.
import { loadSim } from './sim.js';
import { Display } from './display.js';
import { Loop } from './loop.js';
import { BoardScene } from './scene.js';
import { SidePanel } from './panel.js';
import { restoreStore, persistStoreIfDirty } from './store.js';

const status = document.getElementById('status-line');
const say = (s) => { status.textContent = s; };

async function main() {
  say('loading WebAssembly…');
  const sim = await loadSim();
  restoreStore(sim);
  sim.init();

  const display = new Display(sim, document.getElementById('display2d'));
  const loop = new Loop(sim, display);
  const scene = new BoardScene(document.getElementById('view'), display.canvas, {
    onPress: (i) => loop.press(i), onRelease: (i) => loop.release(i),
  });
  display.onUpdate(() => { scene.displayTexture.needsUpdate = true; });

  const panel = new SidePanel(sim, document.getElementById('panel'), {
    onRestart: () => { loop.releaseAll(); sim.init(); panel.applyControls(); },
    onLang: () => {},
  });
  panel.applyControls();

  loop.onFrame.push(() => {
    scene.setBacklight(sim.backlight());
    scene.setLeds(sim.leds());
    panel.update();
    persistStoreIfDirty(sim);
  });
  loop.bindKeyboard(window);
  loop.onButton.push((i, down) => scene.pressByIndex(i, down));

  document.getElementById('btn-front').addEventListener('click', () => scene.frontView());
  document.getElementById('btn-3q').addEventListener('click', () => scene.threeQuarterView());
  window.addEventListener('keydown', (e) => { if (e.target.matches('input, select, textarea')) return; if (e.key === 'f') scene.frontView(); if (e.key === 'v') scene.threeQuarterView(); });

  loop.start();
  window.__vallox = { sim, display, loop, scene, panel, boardError: null };

  const base = import.meta.env.BASE_URL;
  say('loading board model…');
  try {
    await scene.loadBoard(`${base}board.glb`);
    say(`rev A · firmware ${sim.version()}`);
  } catch (e) {
    window.__vallox.boardError = String(e);
    say('board model missing (run `make glb`) — showing a plain slab');
  }
  // the enclosure toggle is armed only if an enclosure.glb exists next to the board
  const chk = document.getElementById('chk-enclosure');
  try {
    const head = await fetch(`${base}enclosure.glb`, { method: 'HEAD' });
    if (head.ok && (head.headers.get('content-type') || '').includes('model')) {
      await scene.loadEnclosure(`${base}enclosure.glb`);
      chk.disabled = false; chk.addEventListener('change', () => scene.setEnclosureVisible(chk.checked));
    }
  } catch { /* none: stays disabled */ }
}

main().catch((e) => { say(`failed: ${e}`); console.error(e); throw e; });
