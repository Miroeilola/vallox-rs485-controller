// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The panel's display as a 2D canvas: the WASM side keeps an RGBA copy of the
// 320 x 240 frame and the union of the rectangles it flushed; once per animation
// frame the dirty rectangle is copied with putImageData. The same canvas is
// shown flat in the side panel and drives the 3D view's CanvasTexture, so the
// smoke test can read pixels back without WebGL.
export class Display {
  constructor(sim, canvas) {
    this.sim = sim;
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d', { willReadFrequently: true });
    this.img = new ImageData(sim.W, sim.H);
    this.updates = 0;
    this.listeners = [];
    this.ctx.fillStyle = '#000'; this.ctx.fillRect(0, 0, sim.W, sim.H);
  }
  onUpdate(fn) { this.listeners.push(fn); }
  // Copy whatever changed since the last call. Returns the dirty rect or null.
  sync() {
    const d = this.sim.takeDirty();
    if (!d) return null;
    this.img.data.set(this.sim.fb());
    this.ctx.putImageData(this.img, 0, 0, d.x, d.y, d.w, d.h);
    this.updates++;
    for (const fn of this.listeners) fn(d);
    return d;
  }
  // Is any pixel non-black? (smoke test helper)
  isLit() {
    const px = this.ctx.getImageData(0, 0, this.sim.W, this.sim.H).data;
    for (let i = 0; i < px.length; i += 4) if (px[i] | px[i + 1] | px[i + 2]) return true;
    return false;
  }
}
