// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Thin wrapper over the Emscripten module: typed accessors, no raw pointers outside this file.
import createPanelModule from './wasm/panel.js';
import wasmUrl from './wasm/panel.wasm?url';

export async function loadSim() {
  const M = await createPanelModule({ locateFile: (p) => (p.endsWith('.wasm') ? wasmUrl : p) });
  const c = (name, ret, args) => M.cwrap(name, ret, args);
  const fn = {
    init: c('sim_init', null, []), run: c('sim_run', null, ['number']), time: c('sim_time_ms', 'number', []),
    version: c('sim_version', 'string', []),
    fbPtr: c('sim_fb_rgba', 'number', []), fbTakeDirty: c('sim_fb_take_dirty', 'number', ['number', 'number', 'number', 'number']),
    flushes: c('sim_fb_flushes', 'number', []), backlight: c('sim_backlight', 'number', []), leds: c('sim_leds', 'number', []),
    button: c('sim_button', null, ['number', 'number']), buttonMv: c('sim_button_mv', 'number', []),
    setOutdoor: c('sim_machine_set_outdoor', null, ['number']), setTimeScale: c('sim_machine_set_time_scale', null, ['number']),
    setReplyDelay: c('sim_machine_set_reply_delay', null, ['number']),
    fault: c('sim_machine_fault', null, ['number']), faultClear: c('sim_machine_fault_clear', null, []),
    temp: c('sim_machine_temp', 'number', ['number']), reg: c('sim_machine_reg', 'number', ['number']),
    fanSpeed: c('sim_machine_fan_speed', 'number', []), flags: c('sim_machine_flags', 'number', []),
    uiPage: c('sim_ui_page', 'number', []), uiDepth: c('sim_ui_depth', 'number', []), uiDimmed: c('sim_ui_dimmed', 'number', []),
    uiBusOk: c('sim_ui_bus_ok', 'number', []), uiLang: c('sim_ui_lang', 'number', []), uiSetLang: c('sim_ui_set_lang', null, ['number']),
    logTotal: c('sim_log_total', 'number', []), logEntry: c('sim_log_entry', 'number', ['number']),
    regName: c('sim_reg_name', 'string', ['number']), faultName: c('sim_fault_name', 'string', ['number']),
    storeCount: c('sim_store_count', 'number', []), storeKey: c('sim_store_key', 'string', ['number']),
    storeValue: c('sim_store_value', 'number', ['number', 'number']), storePut: c('sim_store_put', 'number', ['string', 'number', 'number']),
    storeTakeDirty: c('sim_store_take_dirty', 'number', []),
  };
  const scratch = M._malloc(16);   // four ints for take_dirty / store_value
  const W = 320, H = 240;
  return {
    M, ...fn, W, H,
    fb() { const p = fn.fbPtr(); return new Uint8ClampedArray(M.HEAPU8.buffer, p, W * H * 4); },
    takeDirty() {
      if (!fn.fbTakeDirty(scratch, scratch + 4, scratch + 8, scratch + 12)) return null;
      const v = new Int32Array(M.HEAPU8.buffer, scratch, 4);
      return { x: v[0], y: v[1], w: v[2], h: v[3] };
    },
    log(seq) {
      const p = fn.logEntry(seq); if (!p) return null;
      const b = M.HEAPU8.subarray(p, p + 12);
      return { t: (b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)) >>> 0, dir: b[4], raw: Array.from(b.subarray(5, 11)) };
    },
    storeEntries() {
      const out = []; const n = fn.storeCount();
      for (let i = 0; i < n; i++) { const p = fn.storeValue(i, scratch); const len = new Int32Array(M.HEAPU8.buffer, scratch, 1)[0];
        out.push({ key: fn.storeKey(i), bytes: Array.from(M.HEAPU8.subarray(p, p + len)) }); }
      return out;
    },
    storePutBytes(key, bytes) { const p = M._malloc(Math.max(1, bytes.length)); M.HEAPU8.set(bytes, p); const ok = fn.storePut(key, p, bytes.length); M._free(p); return !!ok; },
  };
}
