// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The panel's settings (hal_store_*) mirrored to localStorage so the chosen
// language survives a reload, as NVS does on the device. Values are opaque
// bytes → hex. A browser that refuses localStorage (private mode) gets a
// session-only store and no error.
const PREFIX = 'vallox-panel:';

export function restoreStore(sim) {
  let n = 0;
  try {
    for (let i = 0; i < localStorage.length; i++) {
      const k = localStorage.key(i);
      if (!k.startsWith(PREFIX)) continue;
      const bytes = (localStorage.getItem(k).match(/../g) || []).map((h) => parseInt(h, 16));
      if (sim.storePutBytes(k.slice(PREFIX.length), bytes)) n++;
    }
  } catch { /* no localStorage: fine */ }
  return n;
}

export function persistStoreIfDirty(sim) {
  if (!sim.storeTakeDirty()) return false;
  try {
    for (const { key, bytes } of sim.storeEntries()) localStorage.setItem(PREFIX + key, bytes.map((b) => b.toString(16).padStart(2, '0')).join(''));
  } catch { /* no localStorage: fine */ }
  return true;
}
