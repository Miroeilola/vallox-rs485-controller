// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
import { defineConfig } from '@playwright/test';

const base = process.env.VITE_BASE ?? '/';

// One smoke test against the built dist/ (vite preview). WebGL in headless
// Chromium runs on SwiftShader — slow but deterministic enough for "did it
// start"; no pixel comparison of the 3D view (spec §4).
export default defineConfig({
  testDir: './e2e',
  timeout: 90_000,
  retries: 0,
  reporter: [['list']],
  use: {
    baseURL: `http://localhost:4173${base}`,
    browserName: 'chromium',
    viewport: { width: 1280, height: 800 },
    launchOptions: { args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] },
    trace: 'retain-on-failure',
    screenshot: 'only-on-failure',
  },
  webServer: {
    command: 'npx vite preview --port 4173 --strictPort',
    url: `http://localhost:4173${base}`,
    reuseExistingServer: false,
    timeout: 30_000,
  },
});
