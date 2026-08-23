// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The one browser test (spec §4): the page loads, the WASM starts, the display
// shows something, the board model loaded, a click on a board button reaches
// the machine, the keyboard does too, the side panel drives the model, the
// language survives a reload, and the bus log has lines. No pixel comparison
// of the 3D view — that is the host goldens' job.
import { test, expect } from '@playwright/test';

test('simulator boots, draws, and a button press reaches the simulated machine', async ({ page }) => {
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
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

  // press + (SW2) three times by clicking its hit box in the front view
  const before = await page.evaluate(() => window.__vallox.sim.fanSpeed());
  await page.evaluate(() => window.__vallox.scene.frontView(false));
  await page.waitForTimeout(300);
  const pt = await page.evaluate(() => {
    const s = window.__vallox.scene; const v = s.hits[1].position.clone().project(s.camera);
    const r = s.canvas.getBoundingClientRect();
    return { x: r.left + (v.x + 1) / 2 * r.width, y: r.top + (1 - v.y) / 2 * r.height };
  });
  for (let i = 0; i < 3; i++) {
    await page.mouse.move(pt.x, pt.y); await page.mouse.down(); await page.waitForTimeout(120); await page.mouse.up(); await page.waitForTimeout(400);
  }
  await page.waitForFunction((b) => window.__vallox.sim.fanSpeed() === b + 3, before, { timeout: 15_000 });
  expect(await page.evaluate(() => window.__vallox.sim.reg(0x29))).toBe([0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF][before + 2]);

  // keyboard: − once
  await page.keyboard.down('ArrowLeft'); await page.waitForTimeout(120); await page.keyboard.up('ArrowLeft');
  await page.waitForFunction((b) => window.__vallox.sim.fanSpeed() === b + 2, before, { timeout: 15_000 });

  // side panel: outdoor temperature reaches the model; fault injection lights the LED
  await page.locator('#in-outdoor').fill('-20');
  expect(await page.evaluate(() => window.__vallox.sim.temp(0))).toBeCloseTo(-20, 1);
  await page.locator('#in-fault').selectOption('5');
  await page.waitForFunction(() => (window.__vallox.sim.leds() & 4) === 4, null, { timeout: 15_000 });
  await expect(page.locator('#led-fault')).toHaveClass(/on/);
  // language survives a reload through localStorage
  await page.locator('#in-lang').selectOption('1');
  await page.waitForTimeout(300);
  await page.reload();
  await page.waitForFunction(() => window.__vallox && window.__vallox.loop.frames > 5, null, { timeout: 60_000 });
  expect(await page.evaluate(() => window.__vallox.sim.uiLang())).toBe(1);

  expect(errors).toEqual([]);
});
