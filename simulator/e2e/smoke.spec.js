// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The one browser test (spec §4): the page loads, the WASM starts, the display
// shows something, a keyboard press reaches the simulated machine, the side
// panel drives the model, and the bus log has lines. The 3D assertions are
// added with the scene.
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

  // keyboard: + (ArrowRight) three times
  const before = await page.evaluate(() => window.__vallox.sim.fanSpeed());
  for (let i = 0; i < 3; i++) {
    await page.keyboard.down('ArrowRight'); await page.waitForTimeout(120); await page.keyboard.up('ArrowRight'); await page.waitForTimeout(400);
  }
  await page.waitForFunction((b) => window.__vallox.sim.fanSpeed() === b + 3, before, { timeout: 15_000 });
  expect(await page.evaluate(() => window.__vallox.sim.reg(0x29))).toBe([0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF][before + 2]);

  // side panel: outdoor temperature reaches the model; fault injection lights the LED readout
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
