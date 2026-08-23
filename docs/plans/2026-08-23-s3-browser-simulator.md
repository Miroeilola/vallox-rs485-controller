# S3 — Browser simulator: Emscripten build, three.js board, live display, buttons, LEDs, side panel, bus log, Pages deploy: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Put the firmware's UI core (`panel_ui`) and its machine emulator (`vallox_machine`) in a browser — compiled unchanged to WebAssembly, drawn on the rev A board's 3D model, driven by clicks, keys and a side panel — and publish it on GitHub Pages from `main`, so a reader can use the device before a board exists.

**Architecture:** A fourth host for the same C core: `simulator/c/hal_web.c` implements `panel_hal.h` for the browser (RGBA framebuffer + dirty-rectangle union, button ladder from clicks, memory bus to the machine model inside the same module with a frame log, RAM settings store) and `simulator/c/sim.c` exports a small plain-C API (`sim_api.h`) that JavaScript calls through `cwrap`. The page is vanilla JS bundled by Vite: `sim.js` wraps the module, `display.js` copies the dirty rectangle to a 320 × 240 canvas, `loop.js` owns time (`requestAnimationFrame` → `sim_run`) and the keyboard, `scene.js` is the three.js view (GLB board, display plane with the canvas as texture, glass, LED glow, hit boxes), `panel.js` the side panel and bus log, `store.js` the `localStorage` mirror. The C host has native tests; the page has one Playwright smoke test; `simulator.yml` exports the GLB in the KiCad container, builds WASM + site, runs the smoke test and deploys Pages from `main`.

**Tech Stack:** C11 (`cc -std=c11 -Wall -Wextra -Werror` natively, Emscripten **6.0.8** for WASM, `-O2`, no exceptions, no filesystem), Node 22, **three 0.185.1**, **vite 7.3.6**, **@playwright/test 1.62.1** (Chromium on SwiftShader), **@gltf-transform/cli 4.4.2** (meshopt), `kicad-cli` 10 (`kicad/kicad:10.0` container in CI), GitHub Actions + Pages (`mymindstorm/setup-emsdk@v14`, `actions/upload-pages-artifact@v3`, `actions/deploy-pages@v4`).

**Spec:** `docs/design/2026-08-23-panel-simulator-design.md` — §3.1 (HAL, ladder levels), §3.4 (browser simulator: build, scene, side panel, realism), §4 (browser test: one Playwright smoke — page loads, WASM starts, display texture non-empty, a click changes the view; `simulator.yml`: GLB export in the KiCad container, Emscripten + Vite, Playwright, Pages from `main`; README badge; CI green is not a measurement), §5 (S3 row: "Smoke test in CI, demo link in the README, enclosure checkbox present (empty)"), §6 (out of scope), §7 (display active-area origin from the library model). S1 delivered `panel_hal.h`, `vallox_machine`, the host HAL (`firmware/test/host/hal_host.c`, the template for the web HAL) and `check.h`; S2 delivered `panel_ui` with its hooks (`panel_ui_client`, `panel_ui_current_page`, …) and `panel_host.c` (the tick loop this plan copies).

## Global Constraints

- Every source file starts with `// SPDX-License-Identifier: MIT` and `// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet` (CSS: `/* … */`; YAML/Makefile: a `#` comment header instead — SPDX lines are not put in YAML).
- **`firmware/components/` is not modified.** The simulator compiles `vallox_protocol`, `vallox_machine`, `panel_hal` and `panel_ui` exactly as they are on `main` (93dd0de or later). If something in the core looks wrong, it is a finding for the review, not a change in this branch.
- The core sees the world only through `panel_hal.h`; the browser host implements those functions in `simulator/c/hal_web.c` and nothing else in the core. The machine model is driven exactly as `firmware/test/host/panel_host.c` drives it: feed → `vlx_machine_tick` → `panel_ui_tick`, in `PANEL_UI_TICK_MS` (20 ms) steps.
- Every register write still goes through `vlx_client`'s allow-list inside the core; the simulator offers no other path to a register (the side panel writes machine *parameters* and calls `vlx_machine_reg_set` only through `vlx_machine_fault`/`fault_clear` — no generic "poke a register" control).
- C under `simulator/c/` compiles with `-std=c11 -Wall -Wextra -Werror` on Apple clang, gcc-15 and emcc. Same GCC pitfalls as S2: no two differently-guarded statements on one line, no extensions, whole-struct zero-init. Run `CC=gcc-15 make -B test-c` once before each commit when `/opt/homebrew/bin/gcc-15` exists.
- JavaScript is ES2022 modules, no framework, no TypeScript, no bundled dependencies other than `three` (the only runtime dependency). Dev dependencies exactly: `vite 7.3.6`, `@playwright/test 1.62.1`. Versions pinned (no `^`).
- Nothing generated is committed: `simulator/src/wasm/`, `simulator/public/board.glb`, `simulator/dist/`, `simulator/node_modules/`, `test_hal_web` are gitignored. `package-lock.json` **is** committed (CI uses `npm ci`).
- All code, comments, UI texts, commit messages in English; Conventional Commits; commits by Miro Eilola, no AI attribution anywhere (git.md). The page's texts say the machine is **simulated** and unverified (mittaukset.md).
- Work on branch `feat/s3-browser-simulator` off `main` (after the `docs(plan)` PR for this file is merged); one PR at the end; CI green (`firmware`, `docs`, `simulator`) before squash-merge.
- No unresolved-placeholder tokens in any `.md`/`.yaml`/`.yml` — the two words `docs.yml` greps for (its `placeholders` job) must not appear, not even in comments.
- Board coordinates in JS come from `simulator/src/board.js` only (values below), never from GLB node names; node names are used for one optional animation and only when present.

## Decisions made in this plan (not in the spec, or the spec leaves them open)

| Decision | Value | Why |
|---|---|---|
| Display transport to the GPU | `hal_display_flush` converts RGB565 → RGBA8888 into a full-frame buffer in WASM memory and keeps the **union** of dirty rectangles; once per animation frame JS copies that rectangle to a 320 × 240 **2D canvas** (`putImageData`) and marks a three.js `CanvasTexture` for upload | Spec §3.4 says "`texSubImage2D` for the dirty rectangle". The dirty rectangle is preserved where it costs (conversion and copy); the GPU upload is the whole 307 kB texture, unmeasurable at this size. In return the display is readable as pixels without WebGL (deterministic smoke test, a flat view in the side panel, works on a machine without GPU), and no three.js internals are touched. Recorded as a deviation in `simulator/README.md`. |
| Module boundary | One Emscripten module: `-sMODULARIZE -sEXPORT_ES6 -sENVIRONMENT=web -sEXPORTED_FUNCTIONS=<every sim_* in sim_api.h>,_malloc,_free -sEXPORTED_RUNTIME_METHODS=cwrap,ccall,UTF8ToString,HEAPU8 -sINCOMING_MODULE_JS_API=locateFile,print,printErr -sINITIAL_MEMORY=4MB -sALLOW_MEMORY_GROWTH=0 -sFILESYSTEM=0 --no-entry -O2`; the wasm URL comes from Vite (`import wasmUrl from './wasm/panel.wasm?url'`) through `locateFile` | Plain C ABI, no Embind/C++ runtime; `locateFile` is the one incoming API needed because Vite hashes the asset name (validated: without `-sINCOMING_MODULE_JS_API=locateFile` Emscripten 6.0.8 ignores `Module.locateFile` and fetches `assets/panel.wasm`, which 404s to `index.html`). Result: `panel.wasm` ≈ 69 kB (30 kB gz). |
| Time | `requestAnimationFrame` delta → `sim_run(elapsed_ms)`, clamped to 200 ms per call, advanced in 20 ms ticks with a sub-tick carry | A tab that slept must not spin; the core's timing is tick-based like everywhere else. The machine's `time_scale` (1×/10×/60×) only speeds the physics, as in the host tests. |
| Button hold | The C host honours a release only after **3 samples** (`HAL_WEB_MIN_HOLD_SAMPLES`) of `hal_buttons_read_mv` have seen the press | Measured: with SwiftShader at ~30 fps a 60 ms click spans one tick and the core's two-sample debounce never sees it (speed 3 → 5 on three presses in the first spike). Holding longer is unaffected. |
| Ladder levels | none 3300, SW1 0, SW2 430, SW3 819, SW4 1336 mV; the lowest-voltage pressed switch wins | Spec §3.1; on the real ladder a lower tap shorts the upper ones. |
| GLB | `kicad-cli pcb export glb --subst-models --no-dnp --include-tracks --include-zones --include-pads --include-silkscreen --include-soldermask --user-origin 0x0mm` (9.7 MB) → `gltf-transform optimize --compress meshopt --simplify false --flatten false --texture-compress false` (**0.88 MB**); generated in CI, gitignored | Measured 2026-08-23 on rev A. `--simplify false` keeps silkscreen text; `--flatten false` keeps the footprint node names (`SW1`…); palette materials keep KiCad's colours. `--user-origin 0x0mm` makes scene x = board x, scene +z = board y, scene +y = height, board top face at y = 1.595 mm — verified by rendering. |
| Display plane | Active area board x **10.64–51.44**, y **10.8–41.4** mm (DS1 footprint at (33.9, 26.1) rot 90°, AA local x ±15.3, y −23.26…17.54 → board `(33.9 + y_local, 26.1 − x_local)`), module top **2.05 mm** above the board; plane at top + 0.03 mm, `rotation.x = −π/2`, `CanvasTexture` (flipY default) → pixel (0,0) at the top-left as seen from the front; glass plane at top + 0.08 mm | Spec §7 open item closed from the library footprint/STEP source (`LCD_2.0in_HSD_HS20HS072RX.py`: T_TOTAL 2.05, AA 30.6 × 40.8) — and verified with a red/blue corner-marker render. 0.1275 mm per pixel both ways. |
| Buttons and LEDs | Hit boxes 6.4 × 6.4 mm at SW1–4 x = 14.5/27.5/40.5/53.5, y = 52; LEDs D4 PWR (0x9dff3a) y 7.8, D6 BUS (0xffd23a) y 10.8, D5 FAULT (0xff3a3a) y 13.8 at x 63.8: emissive plane + additive glow sprite (no bloom pass) | Board coordinates from `kicad-cli pcb export pos`; a sprite costs nothing and needs no post-processing chain. |
| Lighting | `RoomEnvironment` (procedural) through `PMREMGenerator`, one shadow-casting directional light, `ShadowMaterial` floor | Spec's "one small HDRI": the procedural room gives the same reflections with no asset and no licence. |
| Camera | Perspective 32°, `OrbitControls` with damping, ¾ view offset (0.02, 0.125, 0.15) m from the board centre, front view (0, 0.19, 0.0008); buttons `Front view` / `¾ view`, keys `F` / `V`; `maxPolarAngle` keeps the camera above the table | Validated framing at 1400 × 860 and 1000 × 700. |
| Side panel | Machine state table; outdoor −25…+30 °C slider; time scale; response delay 0/10/200/never; panel language; fault select (0x05–0x0A); `Restart panel`; LED and backlight readouts; flat display; bus log (last 200 lines, sticky scroll, `time  hex  sender → receiver  register = value`); updated every 6th frame | Spec §3.4 list; "Restart panel" re-runs `sim_init` (store survives, like NVS). No register poke control. |
| Settings persistence | `hal_store_*` RAM table; JS restores `localStorage` (`vallox-panel:<key>` = hex bytes) into it **before** `sim_init()` and writes it back when the C side reports dirty | The UI reads the store at init; a JS-originated put is not reported back as dirty. Validated by a reload in the smoke test. |
| Enclosure checkbox | Present, disabled, titled "Reserved: the enclosure's STEP does not exist yet"; arms itself and toggles a second GLB when `enclosure.glb` is served next to `board.glb` | Spec S3 row "enclosure checkbox present (empty)" — the code path is real, the file is not. |
| Without a GLB | A plain 104 × 66 × 1.595 mm slab is drawn and the status line says `run make glb`; the smoke test still **requires** the GLB (CI always has it) | Local dev without KiCad works; CI catches a broken export. |
| Pages base path | `vite.config.js` `base: process.env.VITE_BASE ?? '/'`; CI sets `VITE_BASE=/<repo>/`; Playwright's `baseURL` follows | GitHub Pages project sites live under `/<repo>/`. |
| Link checker | `docs.yml` lychee gets `--exclude 'miroeilola\.github\.io/vallox-rs485-controller'` until the first deploy | The README link exists before the site does; the PR would otherwise be red on a link that is about to be live. Remove the exclusion once Pages answers. |

---

## File structure

| Path | Responsibility |
|---|---|
| `simulator/c/hal_web.h` / `hal_web.c` | `panel_hal.h` for the browser + the machine's end of the memory bus, the RGBA framebuffer and dirty union, ladder from clicks with minimum hold, LEDs, backlight, frame log, settings store. |
| `simulator/c/sim_api.h` / `sim.c` | The exported C API: init/run/time, framebuffer, buttons, machine controls and readouts, UI hooks, bus log, store. The tick loop (copied from `panel_host.c`). |
| `simulator/test/test_hal_web.c` | Native tests of both files, run by `make test-c` and CI. |
| `simulator/Makefile` | `glb`, `wasm`, `test-c`, `build`, `smoke`, `test`, `clean`. |
| `simulator/package.json`, `package-lock.json`, `vite.config.js`, `playwright.config.js` | Pinned tooling; base path; smoke test runner with SwiftShader flags and `vite preview` as web server. |
| `simulator/index.html`, `src/styles.css` | The page: 3D canvas + tools, side panel markup, honesty note. |
| `simulator/src/board.js` | Board coordinates (mm) and the scene frame. |
| `simulator/src/sim.js` | `cwrap` wrapper; the only file that sees pointers. |
| `simulator/src/display.js` | Dirty rectangle → 2D canvas; `isLit()` for the test. |
| `simulator/src/loop.js` | `requestAnimationFrame` → `sim_run`; keyboard → buttons; press/release funnel. |
| `simulator/src/store.js` | `localStorage` mirror of the settings. |
| `simulator/src/panel.js` | Side panel: state, controls, fault, language, LEDs, bus log formatting. |
| `simulator/src/scene.js` | three.js: GLB, display plane + glass, LEDs, hit boxes, camera views, slab fallback, enclosure slot. |
| `simulator/src/main.js` | Wiring + `window.__vallox` for the smoke test. |
| `simulator/e2e/smoke.spec.js` | The one Playwright test. |
| `simulator/README.md` | What, how, coordinates, pins, tests, known gaps. |
| `.github/workflows/simulator.yml` | GLB (KiCad container) → WASM + Vite + smoke → Pages. |
| `.github/workflows/docs.yml`, `.gitignore`, `LICENSE.md`, `README.md` | Lychee exclusion; ignores; three.js third-party line; demo link + badge + honesty paragraph. |

---

### Task 1: The browser host in C — `hal_web`, `sim_api`, native tests, `make wasm`

**Files:**
- Create: `simulator/c/hal_web.h`, `simulator/c/hal_web.c`, `simulator/c/sim_api.h`, `simulator/c/sim.c`
- Create: `simulator/test/test_hal_web.c`
- Create: `simulator/Makefile` (complete; later tasks add nothing to it)
- Modify: `.gitignore` (append the block below)

**Interfaces:**
- Consumes (unchanged, from `main`): `panel_hal.h` (all `hal_*`), `panel_ui.h` (`panel_ui_init/tick/set_version/client/current_page/page_depth/is_dimmed`), `texts.h` (`text_lang/text_set_lang`), `vallox_machine.h` (`vlx_machine_*`, `VLX_MACHINE_NEVER`, `m.p.t_outdoor/time_scale`, `m.t_supply/t_extract/t_exhaust`, `m.reply_delay_ms`), `vallox_protocol.h` (`vlx_parser_*`, `vlx_make_poll/write`, `vlx_fan_speed_from_raw/to_raw`, `vlx_register_name`, `vlx_fault_name`, `VLX_REG_*`, `VLX_STATUS_*`, `VLX_IO2_*`), `firmware/test/host/check.h`.
- Produces for Task 2: every `sim_*` in `sim_api.h` below (the JS wrapper names map 1:1), `SIM_LOG_ENTRY_BYTES = 12` layout `u32 LE time_ms, u8 dir (0 panel→machine, 1 machine→panel), 6 raw bytes, 1 pad`, the RGBA buffer `320*240*4` top-left first, `sim_leds()` bits `1 PWR, 2 BUS, 4 FAULT`, `sim_machine_flags()` bits `1 heating, 2 summer bypass, 4 supply fan stopped, 8 fault`, `sim_machine_temp(0..3)` = outdoor/supply/extract/exhaust.

- [ ] **Step 1: Write the failing test**

`simulator/test/test_hal_web.c`:

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The browser HAL and the simulator entry points, compiled natively: the same
// C the WebAssembly module runs, checked without a browser. The e2e over the
// memory bus ("+ three times → register 0x29") is repeated here through the
// sim_* API so the exported surface JS relies on is the thing under test.
#include <string.h>
#include "check.h"
#include "hal_web.h"
#include "panel_hal.h"
#include "sim_api.h"
#include "vallox_protocol.h"

static void test_ladder_levels_and_lowest_pressed_wins(void)
{
    hal_web_reset();
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_NONE_MV);
    hal_web_set_button(2, true);
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_SW3_MV);
    hal_web_set_button(0, true);                 // SW1 (0 mV) wins over SW3
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_SW1_MV);
    hal_web_set_button(9, true);                 // out of range: ignored
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_SW1_MV);
}

static void test_release_waits_for_min_hold_samples(void)
{
    hal_web_reset();
    hal_web_set_button(1, true);
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_SW2_MV);   // sample 1
    hal_web_set_button(1, false);                              // released too early
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_SW2_MV);   // sample 2: still down
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_SW2_MV);   // sample 3: still down
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_NONE_MV);  // released after 3 samples
    // a long hold releases at once
    hal_web_set_button(1, true);
    for (int i = 0; i < 10; i++) hal_buttons_read_mv();
    hal_web_set_button(1, false);
    CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_NONE_MV);
    // re-press during a pending release restarts the hold
    hal_web_set_button(3, true); hal_buttons_read_mv();
    hal_web_set_button(3, false); hal_web_set_button(3, true);
    for (int i = 0; i < 5; i++) CHECK_EQ(hal_buttons_read_mv(), HAL_WEB_LADDER_SW4_MV);
}

static void test_flush_converts_rgb565_and_unions_dirty_rects(void)
{
    hal_web_reset();
    int x, y, w, h;
    CHECK(!hal_web_take_dirty(&x, &y, &w, &h));
    uint16_t px[4] = { 0xF800, 0x07E0, 0x001F, 0xFFFF };   // red, green, blue, white
    hal_display_flush(10, 20, 2, 2, px);
    const uint8_t *fb = hal_web_rgba();
    const uint8_t *p = &fb[(20 * HAL_DISPLAY_W + 10) * 4];
    CHECK(p[0] == 255 && p[1] == 0 && p[2] == 0 && p[3] == 255);
    CHECK(p[4] == 0 && p[5] == 255 && p[6] == 0);
    p = &fb[(21 * HAL_DISPLAY_W + 10) * 4];
    CHECK(p[0] == 0 && p[1] == 0 && p[2] == 255);
    CHECK(p[4] == 255 && p[5] == 255 && p[6] == 255);
    uint16_t one = 0x0000;
    hal_display_flush(300, 230, 1, 1, &one);
    CHECK(hal_web_take_dirty(&x, &y, &w, &h));
    CHECK_EQ(x, 10); CHECK_EQ(y, 20); CHECK_EQ(w, 291); CHECK_EQ(h, 211);
    CHECK(!hal_web_take_dirty(&x, &y, &w, &h));
    CHECK_EQ(hal_web_flushes(), 2);
    // clipped at the edge: no write past the buffer, union clamped
    uint16_t row[8]; memset(row, 0xFF, sizeof row);
    hal_display_flush(316, 238, 8, 8, row);
    CHECK(hal_web_take_dirty(&x, &y, &w, &h));
    CHECK_EQ(x, 316); CHECK_EQ(y, 238); CHECK_EQ(w, 4); CHECK_EQ(h, 2);
}

static void test_leds_backlight_and_store(void)
{
    hal_web_reset();
    hal_leds_set(true, false, true);
    CHECK_EQ(hal_web_leds(), 1 | 4);
    hal_backlight_set(26);
    CHECK_EQ(hal_web_backlight(), 26);
    uint8_t one = 1, got = 9;
    CHECK(hal_web_store_take_dirty() == false);
    CHECK(hal_store_put("lang", &one, 1));
    CHECK(hal_web_store_take_dirty());
    CHECK(!hal_web_store_take_dirty());
    CHECK(hal_store_get("lang", &got, 1)); CHECK_EQ(got, 1);
    CHECK(!hal_store_get("lang", &got, 2));           // length mismatch
    CHECK_EQ(hal_web_store_count(), 1);
    CHECK(strcmp(hal_web_store_key(0), "lang") == 0);
    int len = 0; const uint8_t *v = hal_web_store_value(0, &len);
    CHECK(v && len == 1 && v[0] == 1);
    CHECK(hal_web_store_key(1) == NULL);
    hal_web_reset();                                   // the store survives a reset (it is the NVS)
    CHECK(hal_store_get("lang", &got, 1));
}

static void test_bus_log_records_frames_in_both_directions(void)
{
    hal_web_reset();
    uint8_t f[VLX_FRAME_LEN];
    vlx_make_poll(VLX_ADDR_PANEL_DEFAULT, VLX_ADDR_MAINBOARD_1, VLX_REG_FAN_SPEED, f);
    hal_web_set_time_ms(1234);
    CHECK_EQ(hal_bus_write(f, 3), 3);                  // half a frame: nothing logged yet
    CHECK_EQ(hal_web_log_total(), 0);
    CHECK_EQ(hal_bus_write(f + 3, 3), 3);
    CHECK_EQ(hal_web_log_total(), 1);
    const uint8_t *e = hal_web_log_entry(0);
    CHECK(e != NULL);
    CHECK_EQ(e[0] | (e[1] << 8), 1234); CHECK_EQ(e[4], 0);
    CHECK(memcmp(e + 5, f, VLX_FRAME_LEN) == 0);
    // the machine's answer, logged as direction 1 and readable by the panel
    vlx_make_write(VLX_ADDR_MAINBOARD_1, VLX_ADDR_PANEL_DEFAULT, VLX_REG_FAN_SPEED, 0x0F, f);
    hal_web_machine_write(f, VLX_FRAME_LEN);
    CHECK_EQ(hal_web_log_total(), 2);
    e = hal_web_log_entry(1);
    CHECK(e && e[4] == 1 && e[8] == VLX_REG_FAN_SPEED && e[9] == 0x0F);
    uint8_t rx[8]; CHECK_EQ(hal_bus_read(rx, sizeof rx), VLX_FRAME_LEN);
    // the panel's bytes reach the machine's end
    uint8_t m[8]; CHECK_EQ(hal_web_machine_read(m, sizeof m), VLX_FRAME_LEN);
    // ring: old entries fall off
    for (int i = 0; i < HAL_WEB_LOG_MAX + 5; i++) hal_web_machine_write(f, VLX_FRAME_LEN);
    CHECK_EQ(hal_web_log_total(), 2 + HAL_WEB_LOG_MAX + 5);
    CHECK(hal_web_log_entry(0) == NULL);
    CHECK(hal_web_log_entry(7) != NULL);
    CHECK(hal_web_log_entry(hal_web_log_total()) == NULL);
}

static void run_ms(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 20) sim_run(20); }
static void press(int idx) { sim_button(idx, 1); run_ms(60); sim_button(idx, 0); run_ms(300); }

static void test_sim_plus_three_times_reaches_the_machine(void)
{
    sim_init();
    CHECK_EQ(sim_time_ms(), 0);
    run_ms(3000);
    CHECK_EQ(sim_time_ms(), 3000);
    CHECK(sim_ui_bus_ok());
    CHECK(sim_fb_flushes() > 0);
    int x, y, w, h;
    CHECK(sim_fb_take_dirty(&x, &y, &w, &h));
    CHECK_EQ(w, HAL_DISPLAY_W); CHECK_EQ(h, HAL_DISPLAY_H);   // the first render is a full frame
    CHECK_EQ(sim_backlight(), 255);
    CHECK_EQ(sim_leds() & 1, 1);                               // PWR
    int before = sim_machine_fan_speed();
    CHECK(before >= 1 && before <= 8);
    press(1); press(1); press(1);
    CHECK_EQ(sim_machine_fan_speed(), before + 3);
    CHECK_EQ(sim_machine_reg(VLX_REG_FAN_SPEED), vlx_fan_speed_to_raw(before + 3));
    CHECK(sim_log_total() > 10);
    CHECK(sim_log_entry(sim_log_total() - 1) != NULL);
    CHECK_EQ(sim_machine_reg(0xFE), -1);                       // not a register the model knows
    CHECK(strcmp(sim_reg_name(VLX_REG_FAN_SPEED), "") != 0);
    CHECK(strcmp(sim_fault_name(VLX_FAULT_SUPPLY_AIR_SENSOR), "") != 0);
}

static void test_sim_run_is_clamped_and_carries_the_remainder(void)
{
    sim_init();
    sim_run(5000);
    CHECK_EQ(sim_time_ms(), SIM_MAX_STEP_MS);
    sim_run(7); sim_run(7); sim_run(7);        // 21 ms → one tick, 1 ms carried
    CHECK_EQ(sim_time_ms(), SIM_MAX_STEP_MS + 20);
}

static void test_sim_machine_controls_and_fault(void)
{
    sim_init();
    sim_machine_set_outdoor(-20.0f);
    CHECK(sim_machine_temp(0) < -19.9f && sim_machine_temp(0) > -20.1f);
    sim_machine_set_time_scale(60.0f);
    run_ms(5000);
    CHECK(sim_machine_temp(1) < sim_machine_temp(2));          // supply below extract at -20 °C out
    CHECK_EQ(sim_machine_flags() & 8, 0);
    sim_machine_fault(VLX_FAULT_SUPPLY_AIR_SENSOR);
    run_ms(4000);                                              // the poll round visits 0x36 within 2.75 s
    CHECK_EQ(sim_machine_flags() & 8, 8);
    CHECK_EQ(sim_leds() & 4, 4);                               // FAULT LED follows
    sim_machine_fault_clear();
    run_ms(4000);
    CHECK_EQ(sim_machine_flags() & 8, 0);
    sim_machine_set_reply_delay(-1);                           // never answer
    run_ms(6000);
    CHECK(!sim_ui_bus_ok());
    sim_machine_set_reply_delay(0);
    run_ms(3000);
    CHECK(sim_ui_bus_ok());
}

static void test_sim_language_and_store_mirror(void)
{
    // the store is the device's NVS: it survives sim_init(), so JS restores
    // localStorage into it BEFORE sim_init() and the UI reads it at init
    uint8_t zero = 0;
    CHECK(sim_store_put("lang", &zero, 1));
    sim_init();
    CHECK_EQ(sim_ui_lang(), 0);
    sim_ui_set_lang(1);
    CHECK_EQ(sim_ui_lang(), 1);
    CHECK(sim_store_take_dirty());
    CHECK_EQ(sim_store_count(), 1);
    CHECK(strcmp(sim_store_key(0), "lang") == 0);
    int len = 0; const uint8_t *v = sim_store_value(0, &len);
    CHECK(v && len == 1 && v[0] == 1);
    // a put from JS is not reported back to JS as dirty
    CHECK(sim_store_put("lang", &zero, 1));
    CHECK(!sim_store_take_dirty());
    sim_init();                                                // the UI reads the store at init
    CHECK_EQ(sim_ui_lang(), 0);
}

int main(void)
{
    test_ladder_levels_and_lowest_pressed_wins();
    test_release_waits_for_min_hold_samples();
    test_flush_converts_rgb565_and_unions_dirty_rects();
    test_leds_backlight_and_store();
    test_bus_log_records_frames_in_both_directions();
    test_sim_plus_three_times_reaches_the_machine();
    test_sim_run_is_clamped_and_carries_the_remainder();
    test_sim_machine_controls_and_fault();
    test_sim_language_and_store_mirror();
    return REPORT();
}
```

- [ ] **Step 2: Write the Makefile and run the test to see it fail**

`simulator/Makefile` (complete — `glb`, `wasm`, `build`, `smoke` are used by later tasks as-is):

```makefile
# Browser simulator: the panel UI core and the machine model compiled to
# WebAssembly, drawn on the board's GLB with three.js. `make test` is what CI
# runs (native C tests, WASM build, Vite build, Playwright smoke).
#
#   make glb     board.glb from the committed KiCad board (needs kicad-cli; CI does this in the KiCad container)
#   make wasm    src/wasm/panel.{js,wasm} with emcc
#   make test-c  native tests of the C host (no browser)
#   make build   npm ci + vite build → dist/
#   make smoke   Playwright smoke test against the built dist/
#   make test    test-c + wasm + build + smoke
CC       ?= cc
CFLAGS   ?= -std=c11 -Wall -Wextra -Werror -O1 -g
EMCC     ?= emcc
KICAD_CLI ?= $(shell command -v kicad-cli 2>/dev/null || echo /Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli)
GLTF_TRANSFORM ?= npx --yes @gltf-transform/cli@4.4.2
SIM_VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)

FW    := ../firmware
COMP  := $(FW)/components
PROTO := $(COMP)/vallox_protocol
MACH  := $(COMP)/vallox_machine
HAL   := $(COMP)/panel_hal
UI    := $(COMP)/panel_ui
INC   := -I$(PROTO)/include -I$(MACH)/include -I$(HAL)/include -I$(UI)/include -I$(UI) -Ic -I$(FW)/test/host

CORE_SRCS := $(PROTO)/vallox_protocol.c $(MACH)/vallox_machine.c $(MACH)/vallox_machine_physics.c \
             $(UI)/font.c $(UI)/fonts/font_inter_12.c $(UI)/fonts/font_inter_18.c $(UI)/fonts/font_inter_36.c \
             $(UI)/gfx.c $(UI)/icons.c $(UI)/texts.c $(UI)/texts_en.c $(UI)/texts_fi.c \
             $(UI)/buttons.c $(UI)/vlx_client.c $(UI)/pages.c $(UI)/panel_ui.c
WEB_SRCS  := c/hal_web.c c/sim.c

# Every sim_* declared in sim_api.h is exported (the list is built in the recipe);
# _malloc/_free for the JS wrapper's scratch buffers.
EMFLAGS := -std=c11 -Wall -Wextra -Werror -O2 -DSIM_VERSION='"$(SIM_VERSION)"' \
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createPanelModule -sENVIRONMENT=web \
  -sEXPORTED_RUNTIME_METHODS=cwrap,ccall,UTF8ToString,HEAPU8 \
  -sINCOMING_MODULE_JS_API=locateFile,print,printErr \
  -sALLOW_MEMORY_GROWTH=0 -sINITIAL_MEMORY=4MB -sSTACK_SIZE=256KB -sFILESYSTEM=0 --no-entry -sASSERTIONS=0

PCB := ../hardware/vallox-rs485-controller.kicad_pcb

.PHONY: all glb wasm test-c build smoke test clean

all: wasm build

src/wasm/panel.js src/wasm/panel.wasm: $(CORE_SRCS) $(WEB_SRCS) c/hal_web.h c/sim_api.h Makefile
	mkdir -p src/wasm
	EXPORTS=$$(grep -o 'sim_[a-z_]*(' c/sim_api.h | tr -d '(' | sort -u | sed 's/^/_/' | paste -sd, -); \
	$(EMCC) $(EMFLAGS) -sEXPORTED_FUNCTIONS="$$EXPORTS,_malloc,_free" $(INC) $(CORE_SRCS) $(WEB_SRCS) -o src/wasm/panel.js
wasm: src/wasm/panel.js

# Full detail (tracks, zones, pads, silk, mask) is ~9.7 MB raw; gltf-transform
# with meshopt, no simplification (silkscreen text must survive) and no
# flattening (node names SW1..SW4 are used for the press animation when present)
# brings it to ~0.9 MB. Measured 2026-08-23 on rev A.
public/board.glb: $(PCB)
	$(KICAD_CLI) pcb export glb --force --subst-models --no-dnp \
	  --include-tracks --include-zones --include-pads --include-silkscreen --include-soldermask \
	  --user-origin 0x0mm --output public/board-raw.glb $(PCB)
	$(GLTF_TRANSFORM) optimize public/board-raw.glb public/board.glb \
	  --compress meshopt --simplify false --flatten false --texture-compress false
	rm -f public/board-raw.glb
glb: public/board.glb

test_hal_web: $(CORE_SRCS) $(WEB_SRCS) test/test_hal_web.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^
test-c: test_hal_web
	./test_hal_web

node_modules/.package-lock.json: package.json package-lock.json
	npm ci --no-audit --no-fund
	touch $@
build: node_modules/.package-lock.json src/wasm/panel.js
	npx vite build
smoke: node_modules/.package-lock.json
	npx playwright test
test: test-c build smoke

clean:
	rm -rf test_hal_web *.dSYM src/wasm dist test-results playwright-report public/board-raw.glb
```

Append to `.gitignore`:

```

# --- browser simulator (generated) ---
simulator/node_modules/
simulator/dist/
simulator/src/wasm/
simulator/public/board.glb
simulator/public/board-raw.glb
simulator/public/enclosure.glb
simulator/test_hal_web
simulator/test-results/
simulator/playwright-report/
```

Run: `cd simulator && make test-c`
Expected: FAIL — `hal_web.h: No such file or directory` (the host does not exist yet).

- [ ] **Step 3: Write the browser HAL**

`simulator/c/hal_web.h`:

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The browser host's side of panel_hal.h: what sim.c and the tests poke.
// Mirrors hal_host.h from the host tests; the ladder levels are spec §3.1.
#ifndef HAL_WEB_H
#define HAL_WEB_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HAL_WEB_LADDER_NONE_MV 3300u
#define HAL_WEB_LADDER_SW1_MV     0u
#define HAL_WEB_LADDER_SW2_MV   430u
#define HAL_WEB_LADDER_SW3_MV   819u
#define HAL_WEB_LADDER_SW4_MV  1336u

#define HAL_WEB_MIN_HOLD_SAMPLES 3   // a press is sampled at least this often before a release counts
#define HAL_WEB_STORE_MAX       16
#define HAL_WEB_LOG_MAX         256
#define HAL_WEB_LOG_ENTRY_BYTES 12

void     hal_web_reset(void);                 // everything but the store
void     hal_web_set_time_ms(uint32_t now_ms);
void     hal_web_set_button(int idx, bool down);   // 0..3 = SW1..SW4
uint16_t hal_web_button_mv(void);

// The machine's end of the memory bus (the panel's end is hal_bus_write/read).
size_t   hal_web_machine_read(uint8_t *buf, size_t max);
size_t   hal_web_machine_write(const uint8_t *buf, size_t len);

const uint8_t *hal_web_rgba(void);            // HAL_DISPLAY_W*HAL_DISPLAY_H*4
bool     hal_web_take_dirty(int *x, int *y, int *w, int *h);
int      hal_web_flushes(void);
uint8_t  hal_web_leds(void);                  // bit0 pwr, bit1 bus, bit2 fault
uint8_t  hal_web_backlight(void);

int      hal_web_log_total(void);
const uint8_t *hal_web_log_entry(int seq);    // NULL if seq left the ring

int      hal_web_store_count(void);
const char *hal_web_store_key(int i);
const uint8_t *hal_web_store_value(int i, int *len);
bool     hal_web_store_take_dirty(void);
#endif
```

`simulator/c/hal_web.c`:

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// panel_hal.h for the browser: the display lands in an RGBA8888 buffer JS
// copies to a canvas, the buttons are set from clicks and keys, the bus is a
// pair of byte rings between the UI core and the machine model inside the same
// module (every byte in either direction is also fed to the bus log), the
// clock is whatever sim_run() says, and the settings live in a RAM table JS
// mirrors to localStorage. Nothing here knows about three.js.
#include "hal_web.h"
#include <string.h>
#include "panel_hal.h"
#include "vallox_protocol.h"

#define RING 512
typedef struct { uint8_t buf[RING]; size_t head, tail; } ring_t;

static size_t ring_put(ring_t *r, const uint8_t *d, size_t n)
{
    size_t put = 0;
    while (put < n) {
        size_t next = (r->head + 1) % RING;
        if (next == r->tail) break;          // full: drop the rest, like a UART FIFO
        r->buf[r->head] = d[put++];
        r->head = next;
    }
    return put;
}
static size_t ring_get(ring_t *r, uint8_t *d, size_t max)
{
    size_t got = 0;
    while (got < max && r->tail != r->head) {
        d[got++] = r->buf[r->tail];
        r->tail = (r->tail + 1) % RING;
    }
    return got;
}

static ring_t   s_to_machine, s_to_panel;
static uint32_t s_now_ms;
static uint16_t s_buttons_mv = HAL_WEB_LADDER_NONE_MV;
static const uint16_t k_ladder_mv[4] = { HAL_WEB_LADDER_SW1_MV, HAL_WEB_LADDER_SW2_MV,
                                         HAL_WEB_LADDER_SW3_MV, HAL_WEB_LADDER_SW4_MV };
static bool s_down[4], s_want_up[4];
static int  s_samples[4];          // hal_buttons_read_mv calls since the press
static uint8_t  s_rgba[HAL_DISPLAY_W * HAL_DISPLAY_H * 4];
static int      s_dirty_x0, s_dirty_y0, s_dirty_x1, s_dirty_y1;   // union, x1/y1 exclusive
static bool     s_dirty;
static int      s_flushes;
static uint8_t  s_leds;
static uint8_t  s_backlight;

typedef struct { char key[16]; uint8_t val[32]; size_t len; bool used; } slot_t;
static slot_t s_store[HAL_WEB_STORE_MAX];
static bool   s_store_dirty;

// bus log: two parsers, one per direction, so alignment is per stream
static uint8_t  s_log[HAL_WEB_LOG_MAX][HAL_WEB_LOG_ENTRY_BYTES];
static int      s_log_total;
static vlx_parser_t s_log_parser[2];

static void log_frame(const vlx_frame_t *f, void *ctx)
{
    uint8_t dir = (uint8_t)(uintptr_t)ctx;
    uint8_t *e = s_log[s_log_total % HAL_WEB_LOG_MAX];
    e[0] = (uint8_t)(s_now_ms); e[1] = (uint8_t)(s_now_ms >> 8);
    e[2] = (uint8_t)(s_now_ms >> 16); e[3] = (uint8_t)(s_now_ms >> 24);
    e[4] = dir;
    e[5] = f->domain; e[6] = f->sender; e[7] = f->receiver; e[8] = f->reg; e[9] = f->value; e[10] = f->checksum;
    e[11] = 0;
    s_log_total++;
}

void hal_web_reset(void)
{
    memset(&s_to_machine, 0, sizeof s_to_machine);
    memset(&s_to_panel, 0, sizeof s_to_panel);
    s_now_ms = 0;
    s_buttons_mv = HAL_WEB_LADDER_NONE_MV;
    memset(s_down, 0, sizeof s_down); memset(s_want_up, 0, sizeof s_want_up); memset(s_samples, 0, sizeof s_samples);
    memset(s_rgba, 0, sizeof s_rgba);
    for (size_t i = 3; i < sizeof s_rgba; i += 4) s_rgba[i] = 255;
    s_dirty = false; s_flushes = 0;
    s_leds = 0; s_backlight = 0;
    // the store survives a reset on purpose: it is the device's NVS
    s_log_total = 0;
    vlx_parser_init(&s_log_parser[0], log_frame, (void *)(uintptr_t)0);
    vlx_parser_init(&s_log_parser[1], log_frame, (void *)(uintptr_t)1);
}

void hal_web_set_time_ms(uint32_t now_ms) { s_now_ms = now_ms; }


static void ladder_recompute(void)
{
    // the lowest-voltage pressed switch wins, as on the real ladder
    s_buttons_mv = HAL_WEB_LADDER_NONE_MV;
    for (int i = 0; i < 4; i++) if (s_down[i]) { s_buttons_mv = k_ladder_mv[i]; break; }
}

// A click in a slow browser tab can be shorter than the core's debounce (two
// 20 ms samples), so a release is honoured only after HAL_WEB_MIN_HOLD_SAMPLES
// samples have seen the press. Holding longer is unaffected.
void hal_web_set_button(int idx, bool down)
{
    if (idx < 0 || idx > 3) return;
    if (down) { s_down[idx] = true; s_want_up[idx] = false; s_samples[idx] = 0; }
    else if (s_down[idx]) {
        if (s_samples[idx] >= HAL_WEB_MIN_HOLD_SAMPLES) s_down[idx] = false;
        else s_want_up[idx] = true;
    }
    ladder_recompute();
}
uint16_t hal_web_button_mv(void) { return s_buttons_mv; }

size_t hal_web_machine_read(uint8_t *buf, size_t max) { return ring_get(&s_to_machine, buf, max); }
size_t hal_web_machine_write(const uint8_t *buf, size_t len)
{
    vlx_parser_feed_buffer(&s_log_parser[1], buf, len);
    return ring_put(&s_to_panel, buf, len);
}

const uint8_t *hal_web_rgba(void) { return s_rgba; }
bool hal_web_take_dirty(int *x, int *y, int *w, int *h)
{
    if (!s_dirty) return false;
    *x = s_dirty_x0; *y = s_dirty_y0; *w = s_dirty_x1 - s_dirty_x0; *h = s_dirty_y1 - s_dirty_y0;
    s_dirty = false;
    return true;
}
int     hal_web_flushes(void)   { return s_flushes; }
uint8_t hal_web_leds(void)      { return s_leds; }
uint8_t hal_web_backlight(void) { return s_backlight; }

int hal_web_log_total(void) { return s_log_total; }
const uint8_t *hal_web_log_entry(int seq)
{
    if (seq < 0 || seq >= s_log_total || seq < s_log_total - HAL_WEB_LOG_MAX) return NULL;
    return s_log[seq % HAL_WEB_LOG_MAX];
}

// ---- panel_hal.h --------------------------------------------------------

void hal_display_flush(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *rgb565)
{
    s_flushes++;
    for (uint16_t r = 0; r < h; r++) {
        if (y + r >= HAL_DISPLAY_H) break;
        uint8_t *dst = &s_rgba[((y + r) * HAL_DISPLAY_W + x) * 4];
        for (uint16_t c = 0; c < w; c++) {
            if (x + c >= HAL_DISPLAY_W) break;
            uint16_t p = rgb565[r * w + c];
            uint8_t r5 = (uint8_t)(p >> 11), g6 = (uint8_t)((p >> 5) & 0x3F), b5 = (uint8_t)(p & 0x1F);
            dst[0] = (uint8_t)((r5 << 3) | (r5 >> 2));
            dst[1] = (uint8_t)((g6 << 2) | (g6 >> 4));
            dst[2] = (uint8_t)((b5 << 3) | (b5 >> 2));
            dst[3] = 255;
            dst += 4;
        }
    }
    int x1 = x + w > HAL_DISPLAY_W ? HAL_DISPLAY_W : x + w;
    int y1 = y + h > HAL_DISPLAY_H ? HAL_DISPLAY_H : y + h;
    if (!s_dirty) { s_dirty_x0 = x; s_dirty_y0 = y; s_dirty_x1 = x1; s_dirty_y1 = y1; s_dirty = true; }
    else {
        if (x < s_dirty_x0) s_dirty_x0 = x;
        if (y < s_dirty_y0) s_dirty_y0 = y;
        if (x1 > s_dirty_x1) s_dirty_x1 = x1;
        if (y1 > s_dirty_y1) s_dirty_y1 = y1;
    }
}

uint16_t hal_buttons_read_mv(void)
{
    uint16_t mv = s_buttons_mv;            // this sample still sees the press
    for (int i = 0; i < 4; i++) {
        if (!s_down[i]) continue;
        s_samples[i]++;
        if (s_want_up[i] && s_samples[i] >= HAL_WEB_MIN_HOLD_SAMPLES) { s_down[i] = false; s_want_up[i] = false; }
    }
    ladder_recompute();
    return mv;
}
void hal_leds_set(bool pwr, bool bus, bool fault) { s_leds = (uint8_t)((pwr ? 1 : 0) | (bus ? 2 : 0) | (fault ? 4 : 0)); }
void hal_backlight_set(uint8_t level) { s_backlight = level; }
size_t hal_bus_write(const uint8_t *buf, size_t len)
{
    vlx_parser_feed_buffer(&s_log_parser[0], buf, len);
    return ring_put(&s_to_machine, buf, len);
}
size_t hal_bus_read(uint8_t *buf, size_t max) { return ring_get(&s_to_panel, buf, max); }
uint32_t hal_time_ms(void) { return s_now_ms; }

static slot_t *find(const char *key)
{
    for (int i = 0; i < HAL_WEB_STORE_MAX; i++)
        if (s_store[i].used && strcmp(s_store[i].key, key) == 0) return &s_store[i];
    return NULL;
}
bool hal_store_get(const char *key, void *buf, size_t len)
{
    slot_t *s = find(key);
    if (!s || s->len != len) return false;
    memcpy(buf, s->val, len);
    return true;
}
bool hal_store_put(const char *key, const void *buf, size_t len)
{
    if (len > sizeof ((slot_t *)0)->val || strlen(key) >= sizeof ((slot_t *)0)->key) return false;
    slot_t *s = find(key);
    if (!s) {
        for (int i = 0; i < HAL_WEB_STORE_MAX && !s; i++)
            if (!s_store[i].used) s = &s_store[i];
        if (!s) return false;
        s->used = true;
        strcpy(s->key, key);
    }
    memcpy(s->val, buf, len);
    s->len = len;
    s_store_dirty = true;
    return true;
}

int hal_web_store_count(void)
{
    int n = 0;
    for (int i = 0; i < HAL_WEB_STORE_MAX; i++) if (s_store[i].used) n++;
    return n;
}
static slot_t *nth(int i)
{
    for (int k = 0; k < HAL_WEB_STORE_MAX; k++)
        if (s_store[k].used && i-- == 0) return &s_store[k];
    return NULL;
}
const char *hal_web_store_key(int i) { slot_t *s = nth(i); return s ? s->key : NULL; }
const uint8_t *hal_web_store_value(int i, int *len)
{
    slot_t *s = nth(i);
    if (!s) { *len = 0; return NULL; }
    *len = (int)s->len;
    return s->val;
}
bool hal_web_store_take_dirty(void) { bool d = s_store_dirty; s_store_dirty = false; return d; }
```

- [ ] **Step 4: Write the exported API**

`simulator/c/sim_api.h`:

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The browser simulator's C entry points: what the JavaScript side calls into
// the WebAssembly module. Plain C ABI — integers, floats and pointers into the
// module's memory — so no Embind and no C++ runtime. One simulator instance per
// module (the page has one board).
#ifndef SIM_API_H
#define SIM_API_H
#include <stdint.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define SIM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define SIM_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Lifecycle and time. sim_run() advances the world in PANEL_UI_TICK_MS steps;
// elapsed_ms is clamped to SIM_MAX_STEP_MS so a tab that slept does not spin.
#define SIM_MAX_STEP_MS 200u
SIM_EXPORT void     sim_init(void);
SIM_EXPORT void     sim_run(uint32_t elapsed_ms);
SIM_EXPORT uint32_t sim_time_ms(void);
SIM_EXPORT const char *sim_version(void);

// Display: a full RGBA8888 copy of the panel, 320 x 240, row-major, top-left
// first, kept up to date by hal_display_flush. sim_fb_take_dirty() returns the
// union of the rectangles flushed since the previous call (1) or nothing (0).
SIM_EXPORT const uint8_t *sim_fb_rgba(void);
SIM_EXPORT int      sim_fb_take_dirty(int *x, int *y, int *w, int *h);
SIM_EXPORT int      sim_fb_flushes(void);       // hal_display_flush calls so far
SIM_EXPORT int      sim_backlight(void);        // 0..255
SIM_EXPORT int      sim_leds(void);             // bit0 PWR (D4), bit1 BUS (D6), bit2 FAULT (D5)

// Buttons: idx 0..3 = SW1..SW4 (− + OK ←), down 0/1. The ladder voltage the
// core sees is the rev A level of that switch (see spec §3.1).
SIM_EXPORT void     sim_button(int idx, int down);
SIM_EXPORT int      sim_button_mv(void);

// Machine side (vallox_machine) — the side panel.
SIM_EXPORT void     sim_machine_set_outdoor(float celsius);
SIM_EXPORT void     sim_machine_set_time_scale(float scale);
SIM_EXPORT void     sim_machine_set_reply_delay(int ms);    // 0, 10, 200; -1 = never
SIM_EXPORT void     sim_machine_fault(int code);
SIM_EXPORT void     sim_machine_fault_clear(void);
SIM_EXPORT float    sim_machine_temp(int which);            // 0 outdoor, 1 supply, 2 extract, 3 exhaust (model state, °C)
SIM_EXPORT int      sim_machine_reg(int reg);               // raw byte, or -1 when the model does not know the register
SIM_EXPORT int      sim_machine_fan_speed(void);            // 1..8
SIM_EXPORT int      sim_machine_flags(void);                // bit0 heating, bit1 summer bypass, bit2 supply fan stopped (frost), bit3 fault

// UI hooks for the side panel and the smoke test.
SIM_EXPORT int      sim_ui_page(void);
SIM_EXPORT int      sim_ui_depth(void);
SIM_EXPORT int      sim_ui_dimmed(void);
SIM_EXPORT int      sim_ui_bus_ok(void);
SIM_EXPORT int      sim_ui_lang(void);
SIM_EXPORT void     sim_ui_set_lang(int lang);              // 0 en, 1 fi; stored like the menu does it

// Bus log: a ring of the last SIM_LOG_MAX frames in either direction.
// Each entry is SIM_LOG_ENTRY_BYTES: u32 LE time_ms, u8 dir (0 panel→machine,
// 1 machine→panel), 6 raw bytes, 1 pad. sim_log_total() counts every frame
// ever logged; entries older than SIM_LOG_MAX are gone.
#define SIM_LOG_MAX          256
#define SIM_LOG_ENTRY_BYTES  12
SIM_EXPORT int      sim_log_total(void);
SIM_EXPORT const uint8_t *sim_log_entry(int seq);           // NULL when seq is out of the ring
SIM_EXPORT const char *sim_reg_name(int reg);
SIM_EXPORT const char *sim_fault_name(int code);

// Settings store: the RAM table behind hal_store_*, mirrored to localStorage
// by JS. Keys are NUL-terminated ASCII; values opaque bytes.
#define SIM_STORE_MAX 16
SIM_EXPORT int      sim_store_count(void);
SIM_EXPORT const char *sim_store_key(int i);
SIM_EXPORT const uint8_t *sim_store_value(int i, int *len);
SIM_EXPORT int      sim_store_put(const char *key, const uint8_t *val, int len);
SIM_EXPORT int      sim_store_take_dirty(void);             // 1 once after any put

#ifdef __cplusplus
}
#endif
#endif
```

`simulator/c/sim.c`:

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The simulator: the UI core and the machine model over the browser HAL's
// memory bus, advanced in PANEL_UI_TICK_MS steps by sim_run(). This is the
// same loop as firmware/test/host/panel_host.c, exported to JavaScript.
#include "sim_api.h"
#include <string.h>
#include "hal_web.h"
#include "panel_hal.h"
#include "panel_ui.h"
#include "texts.h"
#include "vallox_machine.h"
#include "vallox_protocol.h"

#ifndef SIM_VERSION
#define SIM_VERSION "dev"
#endif

static vlx_machine_t s_m;
static uint32_t      s_now_ms;
static uint32_t      s_carry_ms;      // sub-tick remainder between sim_run calls

static void tick_once(void)
{
    uint8_t buf[64];
    size_t n = hal_web_machine_read(buf, sizeof buf);
    if (n) vlx_machine_feed(&s_m, buf, n);
    s_now_ms += PANEL_UI_TICK_MS;
    hal_web_set_time_ms(s_now_ms);
    n = vlx_machine_tick(&s_m, s_now_ms, buf, sizeof buf);
    if (n) hal_web_machine_write(buf, n);
    panel_ui_tick(s_now_ms);
}

void sim_init(void)
{
    hal_web_reset();
    s_now_ms = 0; s_carry_ms = 0;
    vlx_machine_init(&s_m);
    panel_ui_set_version(SIM_VERSION);
    panel_ui_init();
}

void sim_run(uint32_t elapsed_ms)
{
    if (elapsed_ms > SIM_MAX_STEP_MS) elapsed_ms = SIM_MAX_STEP_MS;
    s_carry_ms += elapsed_ms;
    while (s_carry_ms >= PANEL_UI_TICK_MS) { tick_once(); s_carry_ms -= PANEL_UI_TICK_MS; }
}
uint32_t    sim_time_ms(void) { return s_now_ms; }
const char *sim_version(void) { return SIM_VERSION; }

const uint8_t *sim_fb_rgba(void) { return hal_web_rgba(); }
int sim_fb_take_dirty(int *x, int *y, int *w, int *h) { return hal_web_take_dirty(x, y, w, h) ? 1 : 0; }
int sim_fb_flushes(void) { return hal_web_flushes(); }
int sim_backlight(void)  { return hal_web_backlight(); }
int sim_leds(void)       { return hal_web_leds(); }

void sim_button(int idx, int down) { hal_web_set_button(idx, down != 0); }
int  sim_button_mv(void) { return hal_web_button_mv(); }

void sim_machine_set_outdoor(float c)     { s_m.p.t_outdoor = c; }
void sim_machine_set_time_scale(float s)  { if (s > 0.0f) s_m.p.time_scale = s; }
void sim_machine_set_reply_delay(int ms)  { s_m.reply_delay_ms = ms < 0 ? VLX_MACHINE_NEVER : (uint16_t)ms; }
void sim_machine_fault(int code)          { vlx_machine_fault(&s_m, (vlx_fault_t)code); }
void sim_machine_fault_clear(void)        { vlx_machine_fault_clear(&s_m); }
float sim_machine_temp(int which)
{
    switch (which) {
    case 0: return s_m.p.t_outdoor;
    case 1: return s_m.t_supply;
    case 2: return s_m.t_extract;
    case 3: return s_m.t_exhaust;
    default: return 0.0f;
    }
}
int sim_machine_reg(int reg)
{
    if (reg < 0 || reg > 255 || !vlx_machine_reg_known(&s_m, (uint8_t)reg)) return -1;
    return vlx_machine_reg_get(&s_m, (uint8_t)reg);
}
int sim_machine_fan_speed(void) { return vlx_fan_speed_from_raw(vlx_machine_reg_get(&s_m, VLX_REG_FAN_SPEED)); }
int sim_machine_flags(void)
{
    uint8_t st = vlx_machine_reg_get(&s_m, VLX_REG_STATUS);
    uint8_t io = vlx_machine_reg_get(&s_m, VLX_REG_IO_MULTI_2);
    int f = 0;
    if (st & VLX_STATUS_HEATING)       f |= 1;
    if (!(st & VLX_STATUS_WINTER_MODE)) f |= 2;
    if (io & VLX_IO2_SUPPLY_FAN_OFF)   f |= 4;
    if (st & VLX_STATUS_FAULT)         f |= 8;
    return f;
}

int  sim_ui_page(void)   { return (int)panel_ui_current_page(); }
int  sim_ui_depth(void)  { return panel_ui_page_depth(); }
int  sim_ui_dimmed(void) { return panel_ui_is_dimmed() ? 1 : 0; }
int  sim_ui_bus_ok(void) { return vlx_client_bus_ok(panel_ui_client(), s_now_ms) ? 1 : 0; }
int  sim_ui_lang(void)   { return (int)text_lang(); }
void sim_ui_set_lang(int lang)
{
    uint8_t l = (uint8_t)(lang == 1 ? 1 : 0);
    hal_store_put("lang", &l, 1);
    text_set_lang((lang_t)l);
}

int sim_log_total(void) { return hal_web_log_total(); }
const uint8_t *sim_log_entry(int seq) { return hal_web_log_entry(seq); }
const char *sim_reg_name(int reg)   { return (reg < 0 || reg > 255) ? "" : vlx_register_name((uint8_t)reg); }
const char *sim_fault_name(int code) { return (code < 0 || code > 255) ? "" : vlx_fault_name((uint8_t)code); }

int sim_store_count(void) { return hal_web_store_count(); }
const char *sim_store_key(int i) { return hal_web_store_key(i); }
const uint8_t *sim_store_value(int i, int *len) { return hal_web_store_value(i, len); }
int sim_store_put(const char *key, const uint8_t *val, int len)
{
    if (len < 0) return 0;
    bool ok = hal_store_put(key, val, (size_t)len);
    hal_web_store_take_dirty();      // a JS-originated put is not news to JS
    return ok ? 1 : 0;
}
int sim_store_take_dirty(void) { return hal_web_store_take_dirty() ? 1 : 0; }
```

- [ ] **Step 5: Run the tests natively, with both compilers**

Run: `cd simulator && make test-c && CC=gcc-15 make -B test-c` (skip the second if gcc-15 is not installed)
Expected: `98 checks, 0 failures` twice. Note the `e2e` inside: `+` three times through `sim_button` moves the machine's fan speed by 3 and register 0x29 to the matching raw value — the same claim as S2's host e2e, now through the exported API.

- [ ] **Step 6: Build the WebAssembly module**

Emscripten: `brew install emscripten` (6.0.8) or `emsdk install 6.0.8 && emsdk activate 6.0.8`. Then:

Run: `cd simulator && make wasm && ls -la src/wasm/`
Expected: `panel.js` ≈ 11 kB, `panel.wasm` ≈ 69 kB, no warnings (`-Werror` is on for emcc too). `grep -c 'function locateFile(path){if(Module\["locateFile"\])' src/wasm/panel.js` prints `1` — that is the line that makes Vite's hashed wasm URL reachable.

- [ ] **Step 7: Commit**

```bash
git add simulator/c simulator/test simulator/Makefile .gitignore
git commit -m "feat(sim): browser host in C — panel_hal for the web, exported sim API, native tests

The UI core and the machine model get their fourth host: an RGBA framebuffer
with a dirty-rectangle union, a button ladder fed from clicks (a release is
honoured only after three samples so a short click in a slow tab still
debounces), a memory bus between the two with a frame log, and a RAM settings
store JS mirrors to localStorage. Plain-C exports only; Emscripten flags in the
Makefile; the same files are tested natively."
```

---

### Task 2: The page without 3D — Vite, WASM wrapper, display canvas, loop, store, side panel, smoke test

**Files:**
- Create: `simulator/package.json`, `simulator/package-lock.json` (from `npm install`), `simulator/vite.config.js`, `simulator/playwright.config.js`
- Create: `simulator/index.html`, `simulator/src/styles.css`
- Create: `simulator/src/board.js`, `simulator/src/sim.js`, `simulator/src/display.js`, `simulator/src/loop.js`, `simulator/src/store.js`, `simulator/src/panel.js`
- Create: `simulator/src/main.js` (interim: no 3D scene — Task 3 replaces it)
- Create: `simulator/e2e/smoke.spec.js` (interim — Task 3 replaces it)

**Interfaces:**
- Consumes: Task 1's `sim_*` exports and `src/wasm/panel.js` / `panel.wasm` (`make wasm`).
- Produces for Task 3: `loadSim()` → object with the methods listed in `sim.js` (`init, run, time, version, fb(), takeDirty(), flushes, backlight, leds, button(idx, down), setOutdoor, setTimeScale, setReplyDelay, fault, faultClear, temp(i), reg(r), fanSpeed, flags, uiPage, uiDepth, uiDimmed, uiBusOk, uiLang, uiSetLang, logTotal, log(seq), regName, faultName, storeEntries(), storePutBytes(), storeTakeDirty, W, H`); `Display` (`canvas`, `sync()`, `onUpdate(fn)`, `isLit()`, `updates`); `Loop` (`start/stop`, `press/release/releaseAll`, `bindKeyboard`, `onFrame[]`, `frames`); `SidePanel` (`applyControls()`, `update()`); `restoreStore/persistStoreIfDirty`; `BOARD/DISPLAY/BUTTONS/LEDS/MM` from `board.js`; `window.__vallox = { sim, display, loop, panel }`; DOM ids in `index.html` (`#view`, `#btn-front`, `#btn-3q`, `#chk-enclosure`, `#status-line`, `#display2d`, `#panel`, `#in-*`, `#ms-*`, `#led-*`, `#bus-log`, `#log-count`).

- [ ] **Step 1: Tooling files**

`simulator/package.json`:

```json
{
  "name": "vallox-panel-simulator",
  "private": true,
  "version": "0.0.0",
  "type": "module",
  "description": "Browser simulator of the Vallox RS-485 controller's panel: the firmware's UI core and a simulated machine, compiled to WebAssembly, on the board's 3D model.",
  "license": "MIT",
  "scripts": {
    "dev": "vite",
    "build": "vite build",
    "preview": "vite preview --port 4173 --strictPort",
    "test": "playwright test"
  },
  "dependencies": {
    "three": "0.185.1"
  },
  "devDependencies": {
    "@playwright/test": "1.62.1",
    "vite": "7.3.6"
  }
}
```

`simulator/vite.config.js`:

```javascript
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
```

`simulator/playwright.config.js`:

```javascript
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
```

Run: `cd simulator && npm install --no-audit --no-fund && npx playwright install chromium && git status --short simulator/`
Expected: `package-lock.json` created (commit it), `node_modules/` ignored.

- [ ] **Step 2: Write the failing smoke test (interim — keyboard only, no 3D)**

`simulator/e2e/smoke.spec.js`:

```javascript
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
```

Run: `cd simulator && make wasm && npx vite build && npx playwright test`
Expected: FAIL — `vite build` errors because `index.html` does not exist (`Could not resolve entry module "index.html"`).

- [ ] **Step 3: The page and its styles**

`simulator/index.html`:

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Vallox RS-485 Controller — panel simulator</title>
  <meta name="description" content="The controller's panel firmware running in the browser against a simulated Vallox machine, on the rev A board's 3D model.">
  <link rel="stylesheet" href="/src/styles.css">
</head>
<body>
  <main id="app">
    <section id="view-wrap">
      <canvas id="view" aria-label="3D view of the board"></canvas>
      <div id="view-tools">
        <button id="btn-front" type="button" title="Front view (F)">Front view</button>
        <button id="btn-3q" type="button" title="¾ view (V)">¾ view</button>
        <label title="Reserved: the enclosure's STEP does not exist yet"><input id="chk-enclosure" type="checkbox" disabled> Enclosure</label>
        <span id="status-line" aria-live="polite">loading…</span>
      </div>
      <p id="view-hint">Click the buttons on the board, or use the keys ← → Enter Backspace. Drag to orbit, wheel to zoom.</p>
    </section>
    <aside id="panel">
      <h1>Vallox RS-485 Controller <small>panel simulator</small></h1>
      <p class="note">This is the firmware's UI core (<code>panel_ui</code>) and its machine emulator (<code>vallox_machine</code>) compiled to WebAssembly — the same C that runs on the device. The machine is <strong>simulated</strong> from <code>docs/research/protocol.md</code>; nothing here has been verified against a real unit yet.</p>

      <h2>Machine</h2>
      <table id="machine-state" class="kv">
        <tr><th>Fan speed</th><td id="ms-speed">–</td></tr>
        <tr><th>Outdoor</th><td id="ms-t0">–</td></tr>
        <tr><th>Supply</th><td id="ms-t1">–</td></tr>
        <tr><th>Extract</th><td id="ms-t2">–</td></tr>
        <tr><th>Exhaust</th><td id="ms-t3">–</td></tr>
        <tr><th>Heater</th><td id="ms-heater">–</td></tr>
        <tr><th>Bypass</th><td id="ms-bypass">–</td></tr>
        <tr><th>Frost stop</th><td id="ms-frost">–</td></tr>
        <tr><th>Fault</th><td id="ms-fault">–</td></tr>
      </table>

      <h2>Controls</h2>
      <div class="controls">
        <label>Outdoor temperature <output id="out-outdoor">5 °C</output>
          <input id="in-outdoor" type="range" min="-25" max="30" step="1" value="5"></label>
        <label>Time scale
          <select id="in-timescale"><option value="1">1×</option><option value="10">10×</option><option value="60">60×</option></select></label>
        <label>Machine response delay
          <select id="in-delay"><option value="0">0 ms</option><option value="10">10 ms</option><option value="200">200 ms</option><option value="-1">never (bus fault)</option></select></label>
        <label>Panel language
          <select id="in-lang"><option value="0">English</option><option value="1">Suomi</option></select></label>
        <label>Inject fault
          <select id="in-fault">
            <option value="0">none</option><option value="5">0x05 supply air sensor</option><option value="6">0x06 CO₂ alarm</option>
            <option value="7">0x07 outdoor air sensor</option><option value="8">0x08 extract air sensor</option>
            <option value="9">0x09 water coil frost</option><option value="10">0x0A exhaust air sensor</option>
          </select></label>
        <button id="btn-reset" type="button" title="Power-cycle the panel (settings survive, like NVS)">Restart panel</button>
      </div>

      <h2>Panel</h2>
      <table class="kv">
        <tr><th>Display</th><td><canvas id="display2d" width="320" height="240" aria-label="The panel's display, flat"></canvas></td></tr>
        <tr><th>LEDs</th><td id="leds"><span class="led" id="led-pwr" title="D4 PWR">PWR</span> <span class="led" id="led-bus" title="D6 BUS">BUS</span> <span class="led" id="led-fault" title="D5 FAULT">FAULT</span></td></tr>
        <tr><th>Backlight</th><td id="ms-backlight">–</td></tr>
        <tr><th>Firmware</th><td id="ms-version">–</td></tr>
      </table>

      <h2>Bus log <small id="log-count"></small></h2>
      <pre id="bus-log" aria-live="off"></pre>

      <p class="note">Source and hardware: <a href="https://github.com/Miroeilola/vallox-rs485-controller">github.com/Miroeilola/vallox-rs485-controller</a>. three.js (MIT), Inter (OFL 1.1). Rendering is KiCad's GLB export of rev A; the enclosure arrives with its STEP.</p>
    </aside>
  </main>
  <script type="module" src="/src/main.js"></script>
</body>
</html>
```

`simulator/src/styles.css`:

```css
/* SPDX-License-Identifier: MIT
   SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet */
:root {
  --bg: #15171c; --panel: #1e2128; --line: #2c3038; --fg: #e6e8ec; --muted: #9aa0aa; --accent: #6ec1ff;
  --led-off: #3a3d44;
  font-family: Inter, system-ui, -apple-system, "Segoe UI", Roboto, sans-serif; font-size: 14px; line-height: 1.4;
  color-scheme: dark;
}
* { box-sizing: border-box; }
html, body { margin: 0; height: 100%; background: var(--bg); color: var(--fg); }
#app { display: grid; grid-template-columns: 1fr 380px; height: 100%; }
#view-wrap { position: relative; min-width: 0; }
#view { display: block; width: 100%; height: 100%; touch-action: none; }
#view-tools { position: absolute; top: 12px; left: 12px; display: flex; gap: 8px; align-items: center; flex-wrap: wrap; }
#view-tools button, #panel button { background: var(--panel); color: var(--fg); border: 1px solid var(--line); border-radius: 6px; padding: 6px 10px; cursor: pointer; font: inherit; }
#view-tools button:hover, #panel button:hover { border-color: var(--accent); }
#view-tools label { color: var(--muted); display: inline-flex; gap: 6px; align-items: center; }
#status-line { color: var(--muted); font-size: 12px; }
#view-hint { position: absolute; bottom: 8px; left: 12px; margin: 0; color: var(--muted); font-size: 12px; }
#panel { background: var(--panel); border-left: 1px solid var(--line); padding: 16px 18px 24px; overflow-y: auto; }
#panel h1 { font-size: 18px; margin: 0 0 8px; font-weight: 600; }
#panel h1 small { color: var(--muted); font-weight: 400; font-size: 13px; margin-left: 6px; }
#panel h2 { font-size: 13px; text-transform: uppercase; letter-spacing: 0.06em; color: var(--muted); margin: 18px 0 8px; }
#panel h2 small { text-transform: none; letter-spacing: 0; margin-left: 6px; }
.note { color: var(--muted); font-size: 12.5px; margin: 0 0 4px; }
.note code { color: var(--fg); }
a { color: var(--accent); }
table.kv { width: 100%; border-collapse: collapse; }
table.kv th { text-align: left; font-weight: 500; color: var(--muted); padding: 3px 8px 3px 0; width: 38%; vertical-align: top; }
table.kv td { padding: 3px 0; font-variant-numeric: tabular-nums; }
.controls label { display: block; margin: 6px 0; color: var(--muted); }
.controls input[type=range] { width: 100%; }
.controls select { width: 100%; margin-top: 2px; background: var(--bg); color: var(--fg); border: 1px solid var(--line); border-radius: 6px; padding: 5px 8px; font: inherit; }
.controls output { float: right; color: var(--fg); }
#display2d { width: 160px; height: 120px; image-rendering: pixelated; border: 1px solid var(--line); border-radius: 3px; background: #000; display: block; }
.led { display: inline-block; padding: 1px 8px; border-radius: 10px; font-size: 11px; background: var(--led-off); color: #bbb; margin-right: 4px; }
.led.on#led-pwr { background: #9dff3a; color: #123; }
.led.on#led-bus { background: #ffd23a; color: #321; }
.led.on#led-fault { background: #ff3a3a; color: #fff; }
#bus-log { margin: 0; padding: 8px; background: var(--bg); border: 1px solid var(--line); border-radius: 6px; height: 220px; overflow-y: auto; font: 11.5px/1.45 ui-monospace, SFMono-Regular, Menlo, monospace; white-space: pre; }
#bus-log .m2p { color: var(--accent); }
@media (max-width: 900px) {
  #app { grid-template-columns: 1fr; grid-template-rows: 55vh 1fr; }
  #panel { border-left: none; border-top: 1px solid var(--line); }
}
```

- [ ] **Step 4: Board coordinates and the WASM wrapper**

`simulator/src/board.js`:

```javascript
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Where things are on the rev A board, in KiCad board coordinates: millimetres,
// origin at the top-left corner of the outline, x to the right, y DOWN (the
// KiCad convention). Read from the committed hardware/vallox-rs485-controller.kicad_pcb
// (kicad-cli pcb export pos) and the DS1 footprint; not from the GLB's node
// names, which the optimiser may drop.
//
// The GLB exported with `--user-origin 0x0mm` puts board x on its x axis, board
// y on its +z axis and height on +y (board top face at y = BOARD.thick mm).
export const BOARD = { w: 104, h: 66, thick: 1.595 };

// DS1 active area (30.6 x 40.8 mm module AA, landscape on the board): footprint
// at (33.9, 26.1) rotated 90°, AA rect local x ±15.3, y -23.26..17.54.
// Pixel (0,0) is the top-left corner; 0.1275 mm per pixel both ways.
export const DISPLAY = { x0: 10.64, y0: 10.8, w: 40.8, h: 30.6, top: 2.05, glassTop: 2.05 };

// SW1..SW4 = − + OK ←, 13 mm pitch, PTS645 6 x 6 mm tact, cap top ~5 mm above the board.
export const BUTTONS = [14.5, 27.5, 40.5, 53.5].map((x, i) => ({ idx: i, x, y: 52, w: 6.4, h: 6.4, top: 5.0, name: ['SW1', 'SW2', 'SW3', 'SW4'][i] }));

// D4 PWR (yellow-green), D6 BUS (yellow), D5 FAULT (red); 0603, lens ~0.6 mm up.
export const LEDS = [
  { bit: 1, x: 63.8, y: 7.8,  colour: 0x9dff3a, name: 'D4 PWR' },
  { bit: 2, x: 63.8, y: 10.8, colour: 0xffd23a, name: 'D6 BUS' },
  { bit: 4, x: 63.8, y: 13.8, colour: 0xff3a3a, name: 'D5 FAULT' },
];

export const MM = 0.001;   // the GLB is in metres
```

`simulator/src/sim.js`:

```javascript
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
      return { t: b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24), dir: b[4], raw: Array.from(b.subarray(5, 11)) };
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
```

- [ ] **Step 5: Display, loop, store**

`simulator/src/display.js`:

```javascript
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
```

`simulator/src/loop.js`:

```javascript
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
```

`simulator/src/store.js`:

```javascript
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
```

- [ ] **Step 6: Side panel and the interim `main.js`**

`simulator/src/panel.js`:

```javascript
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The side panel: machine state read from the model, controls written to it,
// fault injection, panel language, the LED and backlight readouts, and the bus
// log formatted from the raw six-byte frames. Updated from the animation loop
// at a lower rate than the display (every 6th frame ≈ 10 Hz); the log appends
// only new entries.
const ADDR_NAMES = { 0x10: 'all machines', 0x11: 'machine', 0x20: 'all panels', 0x21: 'panel', 0x28: 'LON' };
const hex = (b) => b.toString(16).toUpperCase().padStart(2, '0');
const addrName = (a) => ADDR_NAMES[a] ?? (a >= 0x21 && a <= 0x29 ? `panel ${a - 0x20}` : `0x${hex(a)}`);

export function formatFrame(sim, e) {
  const [, sender, receiver, reg, value] = e.raw;
  const raw = e.raw.map(hex).join(' ');
  const t = (e.t / 1000).toFixed(2).padStart(8);
  let what;
  if (reg === 0x00) what = `poll ${sim.regName(value) || '0x' + hex(value)}`;
  else what = `${sim.regName(reg) || '0x' + hex(reg)} = 0x${hex(value)}`;
  const arrow = e.dir === 0 ? '→' : '←';
  return `${t}  ${raw}  ${addrName(sender)} ${arrow} ${addrName(receiver)}  ${what}`;
}

export class SidePanel {
  constructor(sim, root, { onRestart, onLang }) {
    this.sim = sim; this.root = root; this.onRestart = onRestart; this.onLang = onLang;
    this.$ = (id) => root.querySelector('#' + id);
    this.lastLogSeq = 0; this.frame = 0;
    this.$('ms-version').textContent = sim.version();
    // controls → machine
    const outdoor = this.$('in-outdoor'), outOutdoor = this.$('out-outdoor');
    const applyOutdoor = () => { sim.setOutdoor(Number(outdoor.value)); outOutdoor.textContent = `${outdoor.value} °C`; };
    outdoor.addEventListener('input', applyOutdoor); applyOutdoor();
    this.$('in-timescale').addEventListener('change', (e) => sim.setTimeScale(Number(e.target.value)));
    this.$('in-delay').addEventListener('change', (e) => sim.setReplyDelay(Number(e.target.value)));
    this.$('in-lang').addEventListener('change', (e) => { sim.uiSetLang(Number(e.target.value)); onLang?.(Number(e.target.value)); });
    this.$('in-fault').addEventListener('change', (e) => { const c = Number(e.target.value); if (c) sim.fault(c); else sim.faultClear(); });
    this.$('btn-reset').addEventListener('click', () => onRestart?.());
  }
  // Called after sim_init() (and on restart): controls re-applied to the fresh model.
  applyControls() {
    this.sim.setOutdoor(Number(this.$('in-outdoor').value));
    this.sim.setTimeScale(Number(this.$('in-timescale').value));
    this.sim.setReplyDelay(Number(this.$('in-delay').value));
    const c = Number(this.$('in-fault').value); if (c) this.sim.fault(c);
    this.$('in-lang').value = String(this.sim.uiLang());
    this.lastLogSeq = 0; this.$('bus-log').textContent = '';
  }
  update() {
    if (++this.frame % 6) return;
    const sim = this.sim, $ = this.$;
    const speed = sim.fanSpeed();
    $('ms-speed').textContent = speed >= 1 && speed <= 8 ? `${speed} / 8` : '–';
    for (let i = 0; i < 4; i++) $(`ms-t${i}`).textContent = `${sim.temp(i).toFixed(1)} °C`;
    const f = sim.flags();
    $('ms-heater').textContent = f & 1 ? 'on' : 'off';
    $('ms-bypass').textContent = f & 2 ? 'summer bypass' : 'heat recovery';
    $('ms-frost').textContent = f & 4 ? 'supply fan stopped' : 'no';
    const fault = sim.reg(0x36);
    $('ms-fault').textContent = fault > 0 ? `0x${hex(fault)} ${sim.faultName(fault)}` : 'none';
    const leds = sim.leds();
    $('led-pwr').classList.toggle('on', !!(leds & 1));
    $('led-bus').classList.toggle('on', !!(leds & 2));
    $('led-fault').classList.toggle('on', !!(leds & 4));
    $('ms-backlight').textContent = `${sim.backlight()} / 255${sim.uiDimmed() ? ' (dimmed)' : ''}`;
    // bus log: append what is new, keep the last 200 lines
    const total = sim.logTotal();
    if (total > this.lastLogSeq) {
      const from = Math.max(this.lastLogSeq, total - 200);
      const lines = [];
      for (let s = from; s < total; s++) { const e = sim.log(s); if (e) lines.push(formatFrame(sim, e)); }
      const pre = $('bus-log');
      const stick = pre.scrollTop + pre.clientHeight >= pre.scrollHeight - 4;
      pre.textContent += (pre.textContent ? '\n' : '') + lines.join('\n');
      const all = pre.textContent.split('\n'); if (all.length > 200) pre.textContent = all.slice(-200).join('\n');
      if (stick) pre.scrollTop = pre.scrollHeight;
      this.lastLogSeq = total;
      $('log-count').textContent = `${total} frames`;
    }
  }
}
```

`simulator/src/main.js` (interim; Task 3 replaces it whole):

```javascript
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
```

- [ ] **Step 7: Build and run the smoke test**

Run: `cd simulator && npx vite build && npx playwright test`
Expected: `1 passed`. The build prints `dist/assets/panel-<hash>.wasm ≈ 68.7 kB` and an `index-<hash>.js` of a few kB (three.js is not in yet). If Playwright reports `http://localhost:4173/ is already used`, a previous preview is still running: `lsof -ti :4173 | xargs kill`.

Also look at it: `npx vite preview` and open http://localhost:4173/ — the side panel shows the dashboard in the flat display, the bus log scrolls, `← → Enter Backspace` work, `Restart panel` restarts, the language selector switches the display's texts and survives a reload.

- [ ] **Step 8: Commit**

```bash
git add simulator/package.json simulator/package-lock.json simulator/vite.config.js simulator/playwright.config.js simulator/index.html simulator/src simulator/e2e
git commit -m "feat(sim): the page without 3D — WASM wrapper, display canvas, loop, side panel, bus log, smoke test

Vite + vanilla JS around the module from the previous commit: the dirty
rectangle goes to a 2D canvas once per animation frame, time comes from
requestAnimationFrame, the side panel drives the machine model and shows the
bus frame by frame, and the settings store is mirrored to localStorage. One
Playwright test on SwiftShader checks it boots, draws, and a key press reaches
register 0x29."
```

---

### Task 3: The 3D view — GLB board, display plane, glass, LEDs, hit boxes, camera views, enclosure slot

**Files:**
- Create: `simulator/src/scene.js`
- Modify (replace whole): `simulator/src/main.js`, `simulator/e2e/smoke.spec.js`
- Generated, gitignored: `simulator/public/board.glb` (`make glb`)

**Interfaces:**
- Consumes: Task 2's modules; `board.js` constants.
- Produces: `BoardScene` (`loadBoard(url)`, `loadEnclosure(url)`, `setEnclosureVisible`, `setBacklight(level)`, `setLeds(bits)`, `threeQuarterView()`, `frontView()`, `pressByIndex(idx, down)`, fields `hits[]`, `camera`, `canvas`, `displayTexture`, `boardLoaded`, `enclosureLoaded`); `window.__vallox = { sim, display, loop, scene, panel, boardError }` for the test.

- [ ] **Step 1: Export the board model**

Run: `cd simulator && make glb && ls -la public/board.glb`
Expected: kicad-cli prints two `File not found` lines for `USB_C_Receptacle_HRO_TYPE-C-31-M-12.step` and `Fuse_1812_4532Metric.step` (those models are not in KiCad's library — accepted, the footprints are drawn without bodies), gltf-transform prints `board-raw.glb (9.6 MB) → board.glb (~880 KB)`, `public/board.glb` ≈ 0.9 MB. On macOS without `kicad-cli` on PATH the Makefile falls back to `/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli`.

- [ ] **Step 2: Write the failing smoke test (final)**

`simulator/e2e/smoke.spec.js` (replace whole):

```javascript
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
```

Run: `cd simulator && npx vite build && npx playwright test`
Expected: FAIL at `window.__vallox.scene.boardLoaded || window.__vallox.boardError` (there is no `scene` yet; the wait times out).

- [ ] **Step 3: The scene**

`simulator/src/scene.js`:

```javascript
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The 3D view: the board's GLB (KiCad export, meshopt-compressed), a plane on
// the display's active area carrying the panel canvas as a texture, a glass
// plane over it, three LED planes with a soft additive glow, and invisible hit
// boxes over the four switches for clicks. Positions come from board.js
// (board coordinates), never from GLB node names; if the GLB does carry a
// node named SW1..SW4 the press animation sinks it 0.3 mm, otherwise only the
// hit box sinks (invisible) and the display still reacts.
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
import { MeshoptDecoder } from 'three/addons/libs/meshopt_decoder.module.js';
import { BOARD, DISPLAY, BUTTONS, LEDS, MM } from './board.js';

const PRESS_DEPTH_MM = 0.3;
const toScene = (bx, by, h) => new THREE.Vector3(bx * MM, (BOARD.thick + h) * MM, by * MM);

export class BoardScene {
  constructor(canvas, displayCanvas, { onPress, onRelease }) {
    this.canvas = canvas; this.onPress = onPress; this.onRelease = onRelease;
    this.boardLoaded = false; this.enclosureLoaded = false;
    const renderer = this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true, powerPreference: 'high-performance' });
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 1.0;
    renderer.outputColorSpace = THREE.SRGBColorSpace;
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = THREE.PCFSoftShadowMap;

    const scene = this.scene = new THREE.Scene();
    scene.background = new THREE.Color(0x15171c);
    const pmrem = new THREE.PMREMGenerator(renderer);
    scene.environment = pmrem.fromScene(new RoomEnvironment(), 0.04).texture;   // procedural studio light, no HDRI file
    pmrem.dispose();

    this.target = toScene(BOARD.w / 2, BOARD.h / 2 - 2, 0);
    const camera = this.camera = new THREE.PerspectiveCamera(32, 1, 0.003, 3);
    const controls = this.controls = new OrbitControls(camera, canvas);
    controls.target.copy(this.target);
    controls.enableDamping = true; controls.dampingFactor = 0.12;
    controls.minDistance = 0.03; controls.maxDistance = 0.6;
    controls.maxPolarAngle = Math.PI / 2 - 0.05;   // never under the table
    this.threeQuarterView(false);

    const key = new THREE.DirectionalLight(0xffffff, 2.2);
    key.position.set(0.08, 0.25, 0.18); key.castShadow = true;
    key.shadow.mapSize.set(1024, 1024);
    key.shadow.camera.near = 0.05; key.shadow.camera.far = 0.8;
    key.shadow.camera.left = key.shadow.camera.bottom = -0.1; key.shadow.camera.right = key.shadow.camera.top = 0.1;
    key.shadow.bias = -0.0002;
    scene.add(key);
    scene.add(new THREE.AmbientLight(0xffffff, 0.25));
    // a "table" that only receives the contact shadow
    const floor = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), new THREE.ShadowMaterial({ opacity: 0.35 }));
    floor.rotation.x = -Math.PI / 2; floor.position.copy(toScene(BOARD.w / 2, BOARD.h / 2, -BOARD.thick - 0.0005));
    floor.receiveShadow = true; scene.add(floor);

    // display plane: the panel canvas as a texture
    const tex = this.displayTexture = new THREE.CanvasTexture(displayCanvas);
    tex.colorSpace = THREE.SRGBColorSpace; tex.minFilter = THREE.LinearFilter; tex.magFilter = THREE.LinearFilter;
    tex.generateMipmaps = false; tex.anisotropy = Math.min(8, renderer.capabilities.getMaxAnisotropy());
    this.displayMaterial = new THREE.MeshBasicMaterial({ map: tex, toneMapped: false });
    const plane = new THREE.Mesh(new THREE.PlaneGeometry(DISPLAY.w * MM, DISPLAY.h * MM), this.displayMaterial);
    plane.rotation.x = -Math.PI / 2;       // face up; the plane's +y (v = 1, top row) points to -z = the board's top edge
    plane.position.copy(toScene(DISPLAY.x0 + DISPLAY.w / 2, DISPLAY.y0 + DISPLAY.h / 2, DISPLAY.top + 0.03));
    plane.name = 'display'; scene.add(plane);
    // glass over it: a faint reflection from the environment
    const glass = new THREE.Mesh(new THREE.PlaneGeometry((DISPLAY.w + 3) * MM, (DISPLAY.h + 3) * MM),
      new THREE.MeshPhysicalMaterial({ color: 0xffffff, transparent: true, opacity: 0.10, roughness: 0.05, metalness: 0, envMapIntensity: 1.2, depthWrite: false }));
    glass.rotation.x = -Math.PI / 2; glass.position.copy(toScene(DISPLAY.x0 + DISPLAY.w / 2, DISPLAY.y0 + DISPLAY.h / 2, DISPLAY.glassTop + 0.08));
    scene.add(glass);

    // LEDs: a small emissive plane plus an additive glow sprite
    this.leds = LEDS.map((l) => {
      const mat = new THREE.MeshBasicMaterial({ color: l.colour, toneMapped: false });
      const m = new THREE.Mesh(new THREE.PlaneGeometry(1.2 * MM, 0.7 * MM), mat);
      m.rotation.x = -Math.PI / 2; m.position.copy(toScene(l.x, l.y, 0.75)); m.name = l.name; scene.add(m);
      const glow = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTexture(), color: l.colour, transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, opacity: 0.0 }));
      glow.scale.set(6 * MM, 6 * MM, 1); glow.position.copy(toScene(l.x, l.y, 1.2)); scene.add(glow);
      return { ...l, mesh: m, mat, glow, on: false };
    });
    this.setLeds(0);

    // buttons: invisible hit boxes from board coordinates
    this.raycaster = new THREE.Raycaster();
    this.pointer = new THREE.Vector2();
    this.hits = BUTTONS.map((b) => {
      const m = new THREE.Mesh(new THREE.BoxGeometry(b.w * MM, 3 * MM, b.h * MM), new THREE.MeshBasicMaterial({ visible: false }));
      m.position.copy(toScene(b.x, b.y, b.top - 1.5)); m.userData.idx = b.idx; m.name = `hit-${b.name}`; scene.add(m);
      return m;
    });
    this.pressedIdx = null; this.buttonNodes = [null, null, null, null];
    canvas.addEventListener('pointerdown', (e) => this._pointerDown(e));
    window.addEventListener('pointerup', () => this._pointerUp());
    window.addEventListener('pointercancel', () => this._pointerUp());

    // board: placeholder slab until (and unless) the GLB loads
    const slab = this.placeholder = new THREE.Mesh(new THREE.BoxGeometry(BOARD.w * MM, BOARD.thick * MM, BOARD.h * MM),
      new THREE.MeshStandardMaterial({ color: 0x2f6b3a, roughness: 0.6, metalness: 0.1 }));
    slab.position.copy(toScene(BOARD.w / 2, BOARD.h / 2, -BOARD.thick / 2)); slab.receiveShadow = true; slab.castShadow = true;
    scene.add(slab);

    this._resize();
    window.addEventListener('resize', () => this._resize());
    renderer.setAnimationLoop(() => { controls.update(); renderer.render(scene, camera); });
  }

  async loadBoard(url) {
    const loader = new GLTFLoader(); loader.setMeshoptDecoder(MeshoptDecoder);
    const gltf = await loader.loadAsync(url);
    gltf.scene.traverse((o) => { if (o.isMesh) { o.castShadow = true; o.receiveShadow = true; } });
    this.scene.remove(this.placeholder);
    this.scene.add(gltf.scene);
    this.board = gltf.scene;
    for (let i = 0; i < 4; i++) this.buttonNodes[i] = gltf.scene.getObjectByName(BUTTONS[i].name) || null;
    this.boardLoaded = true;
    return gltf;
  }
  // Reserved for the enclosure: a second GLB in the same frame, toggled by the checkbox.
  async loadEnclosure(url) {
    const loader = new GLTFLoader(); loader.setMeshoptDecoder(MeshoptDecoder);
    const gltf = await loader.loadAsync(url);
    this.enclosure = gltf.scene; this.enclosure.visible = false; this.scene.add(this.enclosure);
    this.enclosureLoaded = true;
    return gltf;
  }
  setEnclosureVisible(v) { if (this.enclosure) this.enclosure.visible = !!v; }

  // display brightness follows the backlight PWM (26/255 = the dimmed state)
  setBacklight(level) { const k = Math.max(0.04, level / 255); this.displayMaterial.color.setScalar(k); }
  setLeds(bits) {
    for (const l of this.leds) {
      const on = (bits & l.bit) !== 0;
      if (on === l.on) continue;
      l.on = on; l.mat.color.set(on ? l.colour : 0x2a2a2a); l.glow.material.opacity = on ? 0.55 : 0.0;
    }
  }
  threeQuarterView(animate = true) { this._moveCamera(new THREE.Vector3(0.02, 0.125, 0.15), animate); }
  frontView(animate = true) { this._moveCamera(new THREE.Vector3(0, 0.19, 0.0008), animate); }
  _moveCamera(offset, animate) {
    const to = this.target.clone().add(offset);
    if (!animate) { this.camera.position.copy(to); this.controls.update(); return; }
    const from = this.camera.position.clone(); const t0 = performance.now();
    const step = (t) => { const k = Math.min(1, (t - t0) / 400); const e = 1 - Math.pow(1 - k, 3);
      this.camera.position.lerpVectors(from, to, e); this.controls.update(); if (k < 1) requestAnimationFrame(step); };
    requestAnimationFrame(step);
  }
  _resize() {
    const w = this.canvas.clientWidth || 800, h = this.canvas.clientHeight || 600;
    this.renderer.setSize(w, h, false);
    this.camera.aspect = w / h; this.camera.updateProjectionMatrix();
  }
  _pickButton(e) {
    const r = this.canvas.getBoundingClientRect();
    this.pointer.set(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
    this.raycaster.setFromCamera(this.pointer, this.camera);
    const hit = this.raycaster.intersectObjects(this.hits, false)[0];
    return hit ? hit.object.userData.idx : null;
  }
  _pointerDown(e) {
    const idx = this._pickButton(e);
    if (idx === null) return;
    e.preventDefault();
    this.controls.enabled = false;          // a press is not an orbit
    this.pressedIdx = idx;
    this._sink(idx, true);
    this.onPress(idx);
  }
  _pointerUp() {
    if (this.pressedIdx === null) return;
    const idx = this.pressedIdx; this.pressedIdx = null;
    this._sink(idx, false);
    this.controls.enabled = true;
    this.onRelease(idx);
  }
  _sink(idx, down) {
    const dy = (down ? -PRESS_DEPTH_MM : PRESS_DEPTH_MM) * MM;
    this.hits[idx].position.y += dy;
    if (this.buttonNodes[idx]) this.buttonNodes[idx].position.y += dy;
  }
  // Same as clicking the hit box, for the keyboard and the smoke test.
  pressByIndex(idx, down) { this._sink(idx, down); }
}

let _glowTex = null;
function glowTexture() {
  if (_glowTex) return _glowTex;
  const c = document.createElement('canvas'); c.width = c.height = 64;
  const g = c.getContext('2d'); const grd = g.createRadialGradient(32, 32, 0, 32, 32, 32);
  grd.addColorStop(0, 'rgba(255,255,255,1)'); grd.addColorStop(0.35, 'rgba(255,255,255,0.35)'); grd.addColorStop(1, 'rgba(255,255,255,0)');
  g.fillStyle = grd; g.fillRect(0, 0, 64, 64);
  _glowTex = new THREE.CanvasTexture(c); return _glowTex;
}
```

- [ ] **Step 4: Wire it — `main.js` (final)**

`simulator/src/main.js` (replace whole):

```javascript
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Wiring: WASM module → Display (2D canvas) → Loop (time, keyboard) → 3D scene
// and side panel. window.__vallox exposes the pieces for the smoke test and for
// poking around in the console; nothing else reads it.
import { loadSim } from './sim.js';
import { Display } from './display.js';
import { Loop } from './loop.js';
import { BoardScene } from './scene.js';
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
  const scene = new BoardScene(document.getElementById('view'), display.canvas, {
    onPress: (i) => loop.press(i), onRelease: (i) => loop.release(i),
  });
  display.onUpdate(() => { scene.displayTexture.needsUpdate = true; });

  const panel = new SidePanel(sim, document.getElementById('panel'), {
    onRestart: () => { loop.releaseAll(); sim.init(); panel.applyControls(); },
    onLang: () => {},
  });
  panel.applyControls();

  loop.onFrame.push(() => {
    scene.setBacklight(sim.backlight());
    scene.setLeds(sim.leds());
    panel.update();
    persistStoreIfDirty(sim);
  });
  loop.bindKeyboard(window);
  // keyboard presses also sink the 3D button
  window.addEventListener('keydown', (e) => { const i = { ArrowLeft: 0, ArrowRight: 1, Enter: 2, Backspace: 3 }[e.key]; if (i !== undefined && !e.repeat && !e.target.matches('input, select, textarea')) scene.pressByIndex(i, true); });
  window.addEventListener('keyup', (e) => { const i = { ArrowLeft: 0, ArrowRight: 1, Enter: 2, Backspace: 3 }[e.key]; if (i !== undefined) scene.pressByIndex(i, false); });

  document.getElementById('btn-front').addEventListener('click', () => scene.frontView());
  document.getElementById('btn-3q').addEventListener('click', () => scene.threeQuarterView());
  window.addEventListener('keydown', (e) => { if (e.target.matches('input, select, textarea')) return; if (e.key === 'f') scene.frontView(); if (e.key === 'v') scene.threeQuarterView(); });

  loop.start();
  window.__vallox = { sim, display, loop, scene, panel, boardError: null };

  const base = import.meta.env.BASE_URL;
  say('loading board model…');
  try {
    await scene.loadBoard(`${base}board.glb`);
    say(`rev A · firmware ${sim.version()}`);
  } catch (e) {
    window.__vallox.boardError = String(e);
    say('board model missing (run `make glb`) — showing a plain slab');
  }
  // the enclosure toggle is armed only if an enclosure.glb exists next to the board
  const chk = document.getElementById('chk-enclosure');
  try {
    const head = await fetch(`${base}enclosure.glb`, { method: 'HEAD' });
    if (head.ok && (head.headers.get('content-type') || '').includes('model')) {
      await scene.loadEnclosure(`${base}enclosure.glb`);
      chk.disabled = false; chk.addEventListener('change', () => scene.setEnclosureVisible(chk.checked));
    }
  } catch { /* none: stays disabled */ }
}

main().catch((e) => { say(`failed: ${e}`); console.error(e); throw e; });
```

- [ ] **Step 5: Build, test, look**

Run: `cd simulator && npx vite build && npx playwright test`
Expected: `1 passed` in 10–15 s; the build prints `index-<hash>.js ≈ 681 kB │ gzip: 177 kB` (three.js).

Then look at it — this is visual work and the test does not judge looks: `npx vite preview`, open http://localhost:4173/, and check against the spec: the board in ¾ view with a soft contact shadow; the dashboard on the display, right way up, top-left pixel at the top-left, bottom bar above the four switches; `Front view` gives the straight-down view with the whole board in frame; clicking a switch sinks it and changes the display; LEDs PWR green, BUS yellow when the bus is alive, FAULT red after injecting a fault; dimmed display after 5 min (or `Restart panel`, wait) is ~10 % brightness; orbiting works on touch (Chrome devtools device mode). Take one screenshot of the ¾ view and one of the front view and attach them to the PR description.

- [ ] **Step 6: Commit**

```bash
git add simulator/src/scene.js simulator/src/main.js simulator/e2e/smoke.spec.js
git commit -m "feat(sim): three.js view — the rev A GLB, the live display on its active area, LEDs, clickable switches

Board coordinates come from board.js (the DS1 footprint and the pos export),
not from GLB node names; the display plane sits on the measured active area
with the panel canvas as its texture and the backlight as its brightness.
Procedural studio environment instead of an HDRI file, glow sprites instead of
a bloom pass, a slab when the GLB is missing, and a disabled enclosure checkbox
that arms itself when enclosure.glb appears."
```

---

### Task 4: CI, Pages, READMEs, licence line

**Files:**
- Create: `.github/workflows/simulator.yml`
- Create: `simulator/README.md`
- Modify: `README.md` (badge + "Try it in the browser" section), `LICENSE.md` (third-party: three.js), `.github/workflows/docs.yml` (lychee exclusion)

**Interfaces:** none new; this task publishes Tasks 1–3.

- [ ] **Step 1: The workflow**

`.github/workflows/simulator.yml`:

```yaml
# Browser simulator: board GLB from KiCad, the UI core + machine model to
# WebAssembly, Vite build, one Playwright smoke test, GitHub Pages from main.
# Green here means "it builds and starts" — the machine is simulated and the
# protocol is unverified until the M3 capture (README says so).
name: simulator

on:
  push:
    paths: ['simulator/**', 'firmware/components/**', 'hardware/vallox-rs485-controller.kicad_pcb', 'lib/**', '.github/workflows/simulator.yml']
  pull_request:
    paths: ['simulator/**', 'firmware/components/**', 'hardware/vallox-rs485-controller.kicad_pcb', 'lib/**', '.github/workflows/simulator.yml']
  workflow_dispatch:

permissions:
  contents: read

concurrency:
  group: simulator-${{ github.ref }}
  cancel-in-progress: true

jobs:
  glb:
    name: Board GLB (kicad-cli)
    runs-on: ubuntu-latest
    container:
      image: kicad/kicad:10.0
      options: --user root
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
      - name: Export GLB with tracks, zones, pads, silkscreen and mask
        run: |
          mkdir -p simulator/public
          kicad-cli pcb export glb --force --subst-models --no-dnp \
            --include-tracks --include-zones --include-pads --include-silkscreen --include-soldermask \
            --user-origin 0x0mm --output simulator/public/board-raw.glb hardware/vallox-rs485-controller.kicad_pcb
          ls -la simulator/public/board-raw.glb
      - uses: actions/upload-artifact@v4
        with:
          name: board-raw-glb
          path: simulator/public/board-raw.glb
          retention-days: 7

  build:
    name: WASM, site, smoke test
    needs: glb
    runs-on: ubuntu-latest
    env:
      VITE_BASE: /${{ github.event.repository.name }}/
    steps:
      - uses: actions/checkout@v4
      - uses: actions/download-artifact@v4
        with:
          name: board-raw-glb
          path: simulator/public
      - uses: actions/setup-node@v4
        with:
          node-version: 22
          cache: npm
          cache-dependency-path: simulator/package-lock.json
      - uses: mymindstorm/setup-emsdk@v14
        with:
          version: 6.0.8
          actions-cache-folder: emsdk-cache
      - name: Native tests of the browser host
        run: make -C simulator test-c
      - name: Optimise the GLB (meshopt, no simplification, node names kept)
        run: |
          cd simulator
          npx --yes @gltf-transform/cli@4.4.2 optimize public/board-raw.glb public/board.glb \
            --compress meshopt --simplify false --flatten false --texture-compress false
          rm public/board-raw.glb
          ls -la public/board.glb
      - name: WebAssembly + Vite build
        run: |
          cd simulator
          make wasm SIM_VERSION="$(git describe --tags --always 2>/dev/null || echo "$GITHUB_SHA" | cut -c1-7)"
          npm ci --no-audit --no-fund
          npx vite build
          ls -la dist dist/assets
      - name: Playwright smoke test
        run: |
          cd simulator
          npx playwright install --with-deps chromium
          npx playwright test
      - uses: actions/upload-artifact@v4
        if: failure()
        with:
          name: playwright-report
          path: |
            simulator/test-results/
            simulator/playwright-report/
          if-no-files-found: ignore
      - uses: actions/upload-pages-artifact@v3
        with:
          path: simulator/dist

  deploy:
    name: GitHub Pages
    if: github.event_name == 'push' && github.ref == 'refs/heads/main'
    needs: build
    runs-on: ubuntu-latest
    permissions:
      pages: write
      id-token: write
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    steps:
      - uses: actions/configure-pages@v5
      - id: deployment
        uses: actions/deploy-pages@v4
```

- [ ] **Step 2: Enable Pages for the repository (once)**

GitHub Pages must be set to build from Actions before the `deploy` job can run:

Run: `gh api -X POST repos/Miroeilola/vallox-rs485-controller/pages -f build_type=workflows`
Expected: JSON with `"build_type": "workflows"` and `"html_url": "https://miroeilola.github.io/vallox-rs485-controller/"`. If it answers `409 Conflict`, Pages already exists — `gh api repos/Miroeilola/vallox-rs485-controller/pages --jq .build_type` must print `workflows`.

- [ ] **Step 3: Documentation**

`simulator/README.md`:

````markdown
# Browser simulator

The controller's panel, running in a browser: the firmware's UI core
(`firmware/components/panel_ui`) and its machine emulator (`vallox_machine`)
compiled to WebAssembly with Emscripten, drawn on the rev A board's 3D model
(KiCad's GLB export) with three.js. One C core, four hosts — this is the fourth
(`docs/design/2026-08-23-panel-simulator-design.md`).

**Live:** https://miroeilola.github.io/vallox-rs485-controller/ — built from
`main` by `.github/workflows/simulator.yml`.

The machine is **simulated** from `docs/research/protocol.md` with a coarse
thermal model. Nothing in it has been verified against a real unit: the
protocol document's confidence classes apply, and the first capture (M3)
corrects the document, then the model, then the UI — in that order.

## What is in the page

| Part | Where | Notes |
|---|---|---|
| 3D board | `src/scene.js` | `public/board.glb` from `kicad-cli pcb export glb` (tracks, zones, pads, silkscreen, mask), meshopt-compressed with gltf-transform to ~0.9 MB (raw 9.7 MB, measured 2026-08-23). Generated, not committed. Procedural studio environment (`RoomEnvironment`), contact shadow. OrbitControls; `Front view` / `¾ view` buttons and keys `F` / `V`. |
| Live display | `src/display.js`, `c/hal_web.c` | `hal_display_flush` converts RGB565 → RGBA into a buffer in WASM memory and keeps the union of dirty rectangles; once per animation frame JS copies that rectangle to a 320 × 240 2D canvas (`putImageData`), which is both the flat view in the side panel and the texture on the display plane. Backlight PWM → texture brightness. |
| Buttons | `src/scene.js`, `src/loop.js` | Hit boxes at SW1–SW4 from `src/board.js` (board coordinates, not GLB node names); keys `← → Enter Backspace`; a press sinks 0.3 mm. The C host holds a press for at least 3 samples so a short click in a slow tab still debounces (`HAL_WEB_MIN_HOLD_SAMPLES`). |
| LEDs | `src/scene.js` | D4 PWR, D6 BUS, D5 FAULT: emissive plane + additive glow sprite. |
| Side panel | `src/panel.js`, `index.html` | Machine state, outdoor temperature −25…+30 °C, time scale 1×/10×/60×, response delay 0/10/200 ms/never, fault injection, panel language, restart; LEDs and backlight; bus log (one frame per line: time, hex, sender → receiver, register/value). |
| Settings | `src/store.js` | `hal_store_*` RAM table mirrored to `localStorage` (`vallox-panel:<key>` = hex) — the language survives a reload, as NVS does. |
| Enclosure | checkbox, disabled | Reserved. When `public/enclosure.glb` exists the checkbox arms itself and toggles a second GLB in the same frame. The STEP does not exist yet. |

Coordinates: the GLB is exported with `--user-origin 0x0mm`, so board x → scene
x, board y → scene +z, height → +y in metres, board top face at y = 1.595 mm.
The display's active area is at board x 10.64–51.44, y 10.8–41.4 mm (from the
DS1 footprint: 30.6 × 40.8 mm AA, footprint at (33.9, 26.1) rotated 90°), module
top 2.05 mm above the board. Pixel (0,0) is the top-left corner as seen from the
front; 0.1275 mm per pixel.

## Build

```
make glb      # public/board.glb from ../hardware (kicad-cli; CI does this in the kicad/kicad:10.0 container)
make wasm     # src/wasm/panel.{js,wasm} with emcc (Emscripten 6.0.8 — brew install emscripten, or emsdk)
make test-c   # native tests of c/hal_web.c and c/sim.c (no browser)
make build    # npm ci + vite build → dist/
make smoke    # Playwright smoke test against dist/ (headless Chromium on SwiftShader)
make test     # all of the above except glb
npm run dev   # Vite dev server on :5173 (needs make wasm first; board.glb optional — a slab is drawn without it)
```

Pinned: three 0.185.1, vite 7.3.6, @playwright/test 1.62.1,
@gltf-transform/cli 4.4.2, Emscripten 6.0.8, Node 22 in CI. WASM ≈ 69 kB
(30 kB gzipped); the page ≈ 680 kB of JS (177 kB gzipped), three.js being most
of it.

## Exported C API

`c/sim_api.h` is the whole surface JS uses: `sim_init/run/time_ms`, the RGBA
framebuffer and dirty rectangle, buttons, LEDs, backlight, machine controls and
readouts (`sim_machine_*`), UI hooks (`sim_ui_*`), the bus log (`sim_log_*`) and
the settings store (`sim_store_*`). Plain C ABI, no Embind. `src/sim.js` wraps
it; nothing else touches pointers.

## Tests

- `test/test_hal_web.c` (native, in CI before the WASM build): ladder levels and
  "lowest pressed wins", minimum-hold release, RGB565 → RGBA conversion and the
  dirty-rectangle union, LEDs/backlight/store, the bus log in both directions
  and its ring, and the e2e through the exported API: `+` three times → fan
  speed +3 and register 0x29, `sim_run` clamping, machine controls, fault →
  FAULT LED, response delay "never" → bus fault and recovery, language + store
  mirror.
- `e2e/smoke.spec.js` (Playwright): the page loads, WASM runs, the display has
  pixels, the bus is up and logged, the GLB loaded, a click on SW2 in the front
  view ×3 moves the fan speed and register 0x29, the keyboard works, the outdoor
  slider reaches the model, an injected fault lights the FAULT LED, no page
  errors. No pixel comparison of the 3D view (spec §4) — the goldens live in
  `firmware/test/host`.

## Known gaps

- The enclosure checkbox is disabled until a STEP exists (`mechanical/`).
- GLB materials are KiCad's defaults; no photographic textures.
- The display texture is uploaded whole when any rectangle is dirty
  (`CanvasTexture.needsUpdate`); the dirty rectangle saves the RGB conversion
  and the 2D copy, not the GPU upload. At 320 × 240 that is 307 kB per dirty
  frame and was not measurable as a cost.
- The full page reruns the core at real time only; `sim_run` clamps a sleeping
  tab's catch-up to 200 ms per frame, so after a long sleep the simulated clock
  is behind wall-clock. Intentional.
- No MQTT/Home Assistant mock, no 24 h graphs, no CO₂/RH, no week clock (spec §6).
````

Root `README.md`: add the simulator badge after the existing `firmware` badge line —

```markdown
[![simulator](https://github.com/Miroeilola/vallox-rs485-controller/actions/workflows/simulator.yml/badge.svg)](https://github.com/Miroeilola/vallox-rs485-controller/actions/workflows/simulator.yml)
```

— and insert this section **before** `## Specifications`:

```markdown
## Try it in the browser

[![simulator](https://github.com/Miroeilola/vallox-rs485-controller/actions/workflows/simulator.yml/badge.svg)](https://github.com/Miroeilola/vallox-rs485-controller/actions/workflows/simulator.yml)

**https://miroeilola.github.io/vallox-rs485-controller/** — the panel firmware's UI
core and its machine emulator compiled to WebAssembly, drawn on the rev A board's
3D model. Click the buttons, change the outdoor temperature, inject a fault,
watch the bus log. It is the same C that will run on the device
(`firmware/components/panel_ui`), not a look-alike.

The machine behind it is **simulated** from [`docs/research/protocol.md`](docs/research/protocol.md)
and nothing in it has been verified against a real unit yet; the first bus capture
(M3) corrects the document, then the emulator, then the UI. Details and the build
in [`simulator/README.md`](simulator/README.md).
```

`LICENSE.md`: append under "Third-party material", after the Inter line:

```markdown
- **three.js** (simulator/, via npm): © three.js authors, MIT — https://github.com/mrdoob/three.js/blob/dev/LICENSE. Not vendored; fetched by `npm ci`.
```

`.github/workflows/docs.yml`: in the lychee `args`, add one exclusion line right after the existing `--exclude 'github\.com/Miroeilola/vallox-rs485-controller'` line, with its comment above the step:

```yaml
            --exclude 'miroeilola\.github\.io/vallox-rs485-controller'
```

and above the `lycheeverse/lychee-action` step add the comment:

```yaml
      # The Pages URL answers only after the first deploy from main
      # (simulator.yml). Remove the miroeilola.github.io exclusion once it does.
```

- [ ] **Step 4: Check the documents the way CI does**

Run from the repo root:
```bash
grep -rnE '\{\{[A-Z_]+\}\}|\bTBD\b|\bTODO\b' --include='*.md' --include='*.yaml' --include='*.yml' simulator README.md LICENSE.md .github/workflows/simulator.yml ; echo "grep exit $?"
```
Expected: no matches, `grep exit 1`.

Run: `cd simulator && make clean && make test`
Expected: `98 checks, 0 failures`, the wasm and Vite builds, `1 passed`. (`make clean` keeps `public/board.glb`.)

- [ ] **Step 5: Commit, push, open the PR, watch CI**

```bash
git add .github/workflows/simulator.yml .github/workflows/docs.yml simulator/README.md README.md LICENSE.md
git commit -m "chore(ci): simulator workflow — GLB in the KiCad container, WASM + Vite + Playwright, GitHub Pages from main

Plus the simulator README, the demo link and badge in the root README, the
three.js licence line, and a lychee exclusion for the Pages URL until the first
deploy answers."
git push -u origin feat/s3-browser-simulator
gh pr create --title "feat(sim): S3 — browser simulator: WASM core on the rev A GLB, side panel, bus log, Pages" --body-file <pr-body.md>
```

The PR body: what it is (one paragraph), the two screenshots, the deviations table from this plan's decisions (display transport, minimum hold, procedural environment), the measured sizes (GLB 9.7 → 0.88 MB, wasm 69 kB, JS 681 kB), the honesty line (simulated machine, unverified protocol), and what is not in it (enclosure, MQTT mock, graphs — spec §6).

Run: `gh pr checks --watch`
Expected: `firmware`, `docs`, `simulator / Board GLB`, `simulator / WASM, site, smoke test` green; `simulator / GitHub Pages` skipped (not `main`). After squash-merge: the `simulator` run on `main` deploys and https://miroeilola.github.io/vallox-rs485-controller/ answers with the page; then a follow-up commit may drop the lychee exclusion.

---

## Self-review

**Spec coverage (§3.4, §4, §5 S3 row):** Emscripten build of `vallox_protocol + vallox_machine + panel_ui + panel_hal(web)` into one module, `-O2`, no exceptions/iostream, ~69 kB — Task 1 (`Makefile` `wasm`, `c/`); three.js + small vanilla code, Vite, no framework, static `dist/` — Tasks 2–3; deploy from `main` only — Task 4 (`deploy` job `if`); board from `kicad-cli pcb export glb --subst-models --include-tracks --include-zones` of rev A, in CI, not committed — Task 4 (`glb` job), Task 3 Step 1 locally; PBR materials as exported, environment, soft contact shadow, OrbitControls, default ¾ view, a front-view button, touch — Task 3 (`scene.js`); live display plane at the active area 40.8 × 30.6 mm, position from the library model, dirty-rectangle update, glass plane, backlight → brightness — Task 3 + Task 1 (`hal_display_flush`), coordinates in `board.js` (§7 closed); buttons by raycast onto hit boxes at x = 14.5/27.5/40.5/53.5, y = 52 mm, keys `← → Enter Backspace`, a pressed button sinks 0.3 mm — Task 3 + Task 2 (`loop.js`); LEDs at D4/D6/D5 with glow — Task 3; enclosure checkbox reserved — Task 3 (`loadEnclosure`, `#chk-enclosure` disabled); side panel: machine state, outdoor −25…+30 °C, time scale, response delay, language, fault injection, bus log in hex one frame per line with sender → receiver, register, value, direction — Task 2 (`panel.js`, `index.html`); collapses below on phones — `styles.css` media query; no MQTT mock — none built; §4 Playwright smoke: page loads, WASM starts, display non-empty, a click changes the view (register and display change on a click in the front view) — Task 3 `smoke.spec.js`; Emscripten in CI (cached by `setup-emsdk`), Vite build, Pages deploy, README badge — Task 4; "CI green is not a measurement" — `simulator/README.md` and the README section say the machine is simulated and unverified; S3 row: smoke test in CI, demo link in the README, enclosure checkbox present (empty) — Tasks 3–4.

**Deviations from the spec text, recorded in "Decisions made in this plan":** the GPU upload is the whole `CanvasTexture` rather than `texSubImage2D` of the dirty rectangle (the dirty rectangle is kept where it costs); the environment is `RoomEnvironment` rather than an HDRI file; the bloom is a sprite. All three are in `simulator/README.md` under "Known gaps"/"What is in the page".

**Placeholder scan:** none. The two tokens docs.yml forbids do not appear in this document or in the files it embeds (checked with the Task 4 Step 4 grep).

**Type consistency:** `sim_api.h` names ↔ `sim.js` `cwrap` names (35 exports, all present in both; the Makefile derives the export list from the header so a new export cannot be forgotten in the link step — it can only be forgotten in `sim.js`, where `cwrap` would throw at load); `hal_web_*` ↔ `sim.c`; `SIM_LOG_ENTRY_BYTES`/`HAL_WEB_LOG_ENTRY_BYTES` = 12 ↔ `sim.js` `log()` reads 12 bytes; `sim_leds()` bits ↔ `panel.js`/`scene.js` `bit` values ↔ `LEDS[].bit`; `sim_machine_flags()` bits ↔ `panel.js`; `window.__vallox` fields ↔ `smoke.spec.js`; DOM ids ↔ `panel.js`/`main.js`; `BUTTONS[].name` `SW1`… ↔ `scene.js` `getObjectByName`; `VITE_BASE` ↔ `vite.config.js`, `playwright.config.js`, `simulator.yml`.

**Validated while writing (2026-08-23, Apple clang 21 + gcc-15 + emcc 6.0.8, Node 24 locally, three 0.185.1, vite 7.3.6, Playwright 1.62.1 with Chrome Headless Shell 151 on SwiftShader):** every file in this document was built and run from the scratchpad tree it was copied from: `make test-c` 98 checks, 0 failures (both compilers); `make wasm` 68.7 kB wasm; `make glb` 9.58 MB → 880 kB; `vite build`; the interim (Task 2) and final (Task 3) `smoke.spec.js` both `1 passed` (6 s and 13 s), also with `VITE_BASE=/vallox-rs485-controller/`; `make clean && make test` from a fresh `npm install` in 15 s; ¾ and front views screenshotted and inspected (dashboard right way up, fault banner, LEDs lit, buttons below the display). Two defects were found and fixed in that pass: `-sSTRICT` silently disabled `Module.locateFile` (Vite's hashed wasm URL 404'd to `index.html`; fixed with `-sINCOMING_MODULE_JS_API=locateFile`), and a 60 ms click at ~30 fps spanned one tick and never debounced (fixed with `HAL_WEB_MIN_HOLD_SAMPLES = 3`, tested). The CI workflow itself has not run anywhere yet — its first run is on the execution branch's PR; the action versions were read from the GitHub API on the day (`setup-emsdk` v14 with emsdk tag `6.0.8` exists; `upload-pages-artifact@v3`, `deploy-pages@v4`, `configure-pages@v5`, `download-artifact@v4` paired with the repo's `upload-artifact@v4`).
