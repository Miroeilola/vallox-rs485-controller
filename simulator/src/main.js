// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Wiring: WASM module → Display (2D canvas) → Loop (time, keyboard) → side
// panel. window.__vallox exposes the pieces for the smoke test and for poking
// around in the console; nothing else reads it. (The 3D scene arrives in the
// next task; until then the flat display in the side panel is the view.)
import { loadSim } from './sim.js';
import { Display } from './display.js';
import { Loop } from './loop.js';
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
  const panel = new SidePanel(sim, document.getElementById('panel'), {
    onRestart: () => { loop.releaseAll(); sim.init(); panel.applyControls(); },
    onLang: () => {},
  });
  panel.applyControls();
  loop.onFrame.push(() => { panel.update(); persistStoreIfDirty(sim); });
  loop.bindKeyboard(window);
  loop.start();
  window.__vallox = { sim, display, loop, panel };
  say(`firmware ${sim.version()} · 3D view not built yet`);
}

main().catch((e) => { say(`failed: ${e}`); console.error(e); throw e; });
