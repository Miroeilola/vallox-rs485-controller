// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The simulator's clock: requestAnimationFrame owns time, the core is advanced
// by the real elapsed milliseconds (capped inside sim_run at 200 ms, so a tab
// that was asleep does not race to catch up), then the display is synced and
// the listeners (3D view, side panel) are told. Keyboard → buttons lives here
// too, because it is about the same four inputs as the clicks.
const KEYS = { ArrowLeft: 0, ArrowRight: 1, Enter: 2, Backspace: 3 };   // − + OK ←

export class Loop {
  constructor(sim, display) {
    this.sim = sim; this.display = display;
    this.last = null; this.frames = 0; this.running = false;
    this.onFrame = [];
    this._pressed = new Set();
    this._raf = this._raf.bind(this);
  }
  start() { if (!this.running) { this.running = true; this.last = null; requestAnimationFrame(this._raf); } }
  stop() { this.running = false; }
  _raf(now) {
    if (!this.running) return;
    if (this.last !== null) this.sim.run(Math.max(0, Math.round(now - this.last)));
    this.last = now;
    this.display.sync();
    this.frames++;
    for (const fn of this.onFrame) fn();
    requestAnimationFrame(this._raf);
  }
  // Buttons from any source (3D click, 2D canvas, keyboard) go through here.
  press(idx) { this._pressed.add(idx); this.sim.button(idx, 1); }
  release(idx) { this._pressed.delete(idx); this.sim.button(idx, 0); }
  releaseAll() { for (const i of [...this._pressed]) this.release(i); }
  bindKeyboard(target = window) {
    target.addEventListener('keydown', (e) => {
      if (e.repeat || e.target.matches('input, select, textarea')) return;
      const idx = KEYS[e.key]; if (idx === undefined) return;
      e.preventDefault(); this.press(idx);
    });
    target.addEventListener('keyup', (e) => {
      const idx = KEYS[e.key]; if (idx === undefined) return;
      e.preventDefault(); this.release(idx);
    });
    target.addEventListener('blur', () => this.releaseAll());
  }
}
