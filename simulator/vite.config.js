// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
import { defineConfig } from 'vite';

// On GitHub Pages the site lives under /<repo>/; CI sets VITE_BASE. Locally it is /.
export default defineConfig({
  base: process.env.VITE_BASE ?? '/',
  build: {
    target: 'es2022',
    sourcemap: false,
    chunkSizeWarningLimit: 1200,   // three.js is ~650 kB minified; one chunk is fine for one page
  },
  server: { port: 5173, strictPort: true },
  preview: { port: 4173, strictPort: true },
});
