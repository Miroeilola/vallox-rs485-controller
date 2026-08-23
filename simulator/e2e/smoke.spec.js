// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The one browser test (spec §4): the page loads, the WASM starts, the display
// shows something, the board model loaded, a click on a board button reaches
// the machine, the keyboard does too, the side panel drives the model, the
// language survives a reload, and the bus log has lines. No pixel comparison
// of the 3D view — that is the host goldens' job.
import { test, expect } from '@playwright/test';

// Wait on observable state, not wall-clock: the runner's SwiftShader may render at a
// few frames per second, and the core only advances when a frame runs.
async function framesAdvance(page, n = 2) {
  const f = await page.evaluate(() => window.__vallox.loop.frames);
  await page.waitForFunction((x) => window.__vallox.loop.frames >= x, f + n, { timeout: 30_000 });
}
async function untilReleased(page) {
  await page.waitForFunction(() => window.__vallox.sim.buttonMv() === 3300, null, { timeout: 30_000 });
  await framesAdvance(page, 2);                    // the debounce must see "none" before the next press
}
async function pressAndRelease(page, down, up) {
  await down();
  await framesAdvance(page, 3);                    // ≥ HAL_WEB_MIN_HOLD_SAMPLES ticks happen within these frames
  await up();
  await untilReleased(page);
}

test('simulator boots, draws, and a button press reaches the simulated machine', async ({ page }) => {
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  if (process.env.SMOKE_CPU_THROTTLE) {
    // reproduces a slow CI runner locally: SMOKE_CPU_THROTTLE=8 npx playwright test
    const cdp = await page.context().newCDPSession(page);
    await cdp.send('Emulation.setCPUThrottlingRate', { rate: Number(process.env.SMOKE_CPU_THROTTLE) });
  }
  await page.goto('./');
  await page.waitForFunction(() => window.__vallox && window.__vallox.loop.frames > 5, null, { timeout: 60_000 });
  // WASM runs and the display has pixels
  await page.waitForFunction(() => window.__vallox.sim.time() > 1500, null, { timeout: 30_000 });
  expect(await page.evaluate(() => window.__vallox.display.isLit())).toBe(true);
  expect(await page.evaluate(() => window.__vallox.display.updates)).toBeGreaterThan(0);
  // the bus is alive and logged
  await page.waitForFunction(() => window.__vallox.sim.uiBusOk() === 1, null, { timeout: 30_000 });
  expect(await page.evaluate(() => window.__vallox.sim.logTotal())).toBeGreaterThan(4);
  await expect(page.locator('#bus-log')).toContainText('panel → machine');
  // the board GLB loaded (CI always exports it; locally run `make glb` first)
  await page.waitForFunction(() => window.__vallox.scene.boardLoaded || window.__vallox.boardError, null, { timeout: 60_000 });
  expect(await page.evaluate(() => window.__vallox.boardError)).toBeNull();
  expect(await page.evaluate(() => window.__vallox.scene.boardLoaded)).toBe(true);
  // LED materials follow sim_leds(): PWR on, FAULT off at boot (the off colour must be applied on the first update)
  const ledHex = await page.evaluate(() => window.__vallox.scene.leds.map((l) => l.mat.color.getHex()));
  expect(ledHex[0]).toBe(0x9dff3a);
  expect(ledHex[2]).toBe(0x2a2a2a);

  // press + (SW2) three times by clicking its hit box in the front view
  const before = await page.evaluate(() => window.__vallox.sim.fanSpeed());
  await page.evaluate(() => window.__vallox.scene.frontView(false));
  await framesAdvance(page, 2);
  const pt = await page.evaluate(() => {
    const s = window.__vallox.scene; s.camera.updateMatrixWorld(true);
    const v = s.hits[1].position.clone().project(s.camera);
    const r = s.canvas.getBoundingClientRect();
    return { x: r.left + (v.x + 1) / 2 * r.width, y: r.top + (1 - v.y) / 2 * r.height };
  });
  for (let i = 0; i < 3; i++) {
    await pressAndRelease(
      page,
      async () => { await page.mouse.move(pt.x, pt.y); await page.mouse.down(); },
      () => page.mouse.up(),
    );
  }
  await page.waitForFunction((b) => window.__vallox.sim.fanSpeed() === b + 3, before, { timeout: 30_000 });
  expect(await page.evaluate(() => window.__vallox.sim.reg(0x29))).toBe([0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF][before + 2]);

  // keyboard: − once
  await pressAndRelease(page, () => page.keyboard.down('ArrowLeft'), () => page.keyboard.up('ArrowLeft'));
  await page.waitForFunction((b) => window.__vallox.sim.fanSpeed() === b + 2, before, { timeout: 30_000 });

  // a key held across a window blur releases both the model and the 3D button
  const y0 = await page.evaluate(() => window.__vallox.scene.hits[0].position.y);
  await page.keyboard.down('ArrowLeft');
  await framesAdvance(page, 3);
  expect(await page.evaluate(() => window.__vallox.scene.hits[0].position.y)).toBeLessThan(y0);
  await page.evaluate(() => window.dispatchEvent(new Event('blur')));
  await framesAdvance(page, 2);
  expect(await page.evaluate(() => window.__vallox.scene.hits[0].position.y)).toBeCloseTo(y0, 9);
  await page.keyboard.up('ArrowLeft');
  await framesAdvance(page, 2);
  expect(await page.evaluate(() => window.__vallox.sim.buttonMv())).toBe(3300);

  // side panel: outdoor temperature reaches the model; fault injection lights the LED
  await page.locator('#in-outdoor').fill('-20');
  expect(await page.evaluate(() => window.__vallox.sim.temp(0))).toBeCloseTo(-20, 1);
  await page.locator('#in-fault').selectOption('5');
  await page.waitForFunction(() => (window.__vallox.sim.leds() & 4) === 4, null, { timeout: 15_000 });
  await expect(page.locator('#led-fault')).toHaveClass(/on/);
  expect(await page.evaluate(() => window.__vallox.scene.leds[2].mat.color.getHex())).toBe(0xff3a3a);
  // language survives a reload through localStorage
  await page.locator('#in-lang').selectOption('1');
  await framesAdvance(page, 3);
  await page.reload();
  await page.waitForFunction(() => window.__vallox && window.__vallox.loop.frames > 5, null, { timeout: 60_000 });
  expect(await page.evaluate(() => window.__vallox.sim.uiLang())).toBe(1);

  expect(errors).toEqual([]);
});
