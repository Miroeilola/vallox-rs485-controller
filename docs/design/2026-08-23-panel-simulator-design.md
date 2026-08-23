# Panel UI core and browser simulator — design

Date 2026-08-23 · Status: approved in discussion, awaiting review of this text ·
Applies to firmware rev 0.x on hardware rev A

## 1. Purpose

The device replaces the original Vallox DIGIT SE panel, so it carries the panel's
user interface: a 320 × 240 display, four buttons, three indicator LEDs, and
every setting the original panel could reach. This design covers three things
that are built together because they share one core:

1. **`panel_ui`** — the menu engine, state machine, renderer and texts that run
   on the device.
2. **`vallox_machine`** — an emulator of the machine side of the RS-485 bus,
   register map plus a coarse thermal model, used by host tests and by the
   simulator.
3. **The browser simulator** — the same `panel_ui` and `vallox_machine` compiled
   to WebAssembly, drawn onto the board's 3D model (three.js), with a side panel
   to drive the machine. Published on GitHub Pages, linked from the README, so a
   reader can use the device before ordering a board.

The simulator is not a demo written beside the firmware; it is the firmware's
UI core in another host. There is one copy of the UI logic.

## 2. Decisions taken

| Question | Decision | Why |
|---|---|---|
| Scope of the menu | **Option C:** basic functions now (fan speed, four temperatures, heating setpoint, boost, faults, service reminder); the menu engine is table-driven so every further setting is one table row | No rewrite when the installer menu arrives after the M3 capture confirms registers |
| Drawing layer | **Own narrow renderer** + fonts pre-rendered at build time from a TTF (Inter, SIL OFL) into 4-bit antialiased bitmaps, three sizes | Typography and disciplined layout make the look; LVGL would cost a large dependency and a recognisable style; a runtime font engine is not needed at 320 × 240 |
| Browser realism | **3D from the start, bare PCB** from KiCad's GLB export; the enclosure becomes a show/hide toggle when its STEP exists | The board model with display, buttons and LEDs already exists; the enclosure does not |
| Machine model | **Coarse physics** (option B): temperatures follow fan speed, outdoor temperature, heat-recovery efficiency and the heater setpoint with a time constant; faults injectable | The demo convinces when the machine reacts; it is also the test bench for UI behaviour under changing values |
| Display language | **English and Finnish**, selectable in settings; all texts through one key table from day one | Cheap now, painful later; shows localisation was considered |
| Host boundary | One C core, thin HAL as plain C functions linked per host; four hosts: ESP-IDF, ESPHome, host test runner, Emscripten | Deterministic tick-based core behaves identically everywhere; no runtime function pointers needed on the MCU |
| Where the machine model lives | In C, inside the same WASM module as the UI | The host tests need it; two models (C + JS) would drift |

Rejected: a JavaScript re-implementation of the UI ("look-alike demo") — the
duplicate logic `firmware.md` forbids; LVGL; putting the machine model in JS.

## 3. Architecture

```
firmware/components/
├── vallox_protocol/   exists   frames, parser, value encodings, bus survey
├── vallox_machine/    new      machine emulator: register map + tick physics + fault injection
├── panel_ui/          new      menu engine, state machine, renderer, fonts, texts (en/fi), vlx_client
└── panel_hal/         new      header only: the contract every host implements

hosts
├── firmware/main/             ESP-IDF: UART + DE, ST7789 SPI, ADC buttons, NVS, Wi-Fi later
├── esphome/components/...     ESPHome wrapper (existing, grows)
├── firmware/test/host/        Unity tests, PNG writer, memory bus between UI and machine
└── simulator/                 Emscripten build of ui + machine, three.js view, side panel, Pages deploy
```

The core never touches a UART, a clock, a display driver or storage. It is
driven by `panel_ui_tick()` at ~50 Hz from whichever host owns time. No threads,
no callbacks inside the core.

### 3.1 `panel_hal.h`

Plain C functions, one implementation per host, resolved at link time:

| Function | Contract |
|---|---|
| `void hal_display_flush(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *rgb565)` | Copy a rectangle to the panel (ST7789 window write / canvas texture sub-update) |
| `uint16_t hal_buttons_read_mv(void)` | Button-ladder voltage in millivolts. The core maps it to a button; the thresholds are core code and are tested |
| `void hal_leds_set(bool pwr, bool bus, bool fault)` | Three indicator LEDs |
| `void hal_backlight_set(uint8_t level)` | 0–255 PWM duty |
| `size_t hal_bus_write(const uint8_t *buf, size_t len)` / `size_t hal_bus_read(uint8_t *buf, size_t max)` | RS-485 bytes. Driver-enable timing belongs to the host |
| `uint32_t hal_time_ms(void)` | Monotonic milliseconds |
| `bool hal_store_get(const char *key, void *buf, size_t len)` / `bool hal_store_put(...)` | Settings: NVS on the device, `localStorage` in the browser, a RAM table in tests |

Expected ladder levels from the rev A schematic, simulated 2026-08-23
(`docs/measurements/2026-08-23-spice-rev-a.md`): none 3300, SW1 0, SW2 430, SW3
819, SW4 1336 mV. Thresholds sit at the midpoints; the bring-up measurement of
the real levels replaces the table, not the code.

### 3.2 `panel_ui`

**Screen.** Three zones: a top bar (page title, bus state, fault icon), content,
and a bottom bar showing the four buttons' current meaning (`− + OK ←` change by
page) drawn directly above the physical buttons (13 mm pitch under the glass).

**Pages are data:**

```c
typedef struct {
    const char  *title_key;            // text-table key
    page_kind_t  kind;                 // DASHBOARD | LIST | EDITOR | INFO
    const item_t *items; uint8_t n;    // LIST rows -> sub-page or editor
    uint8_t      reg; value_enc_t enc; // EDITOR: register + encoding (fan 1–8, °C, %, min, bit)
    int16_t      min, max, step;
} page_t;
```

A new setting is one `page_t`/`item_t` row. Value encodings come from
`vallox_protocol` (NTC table, speed bits), so the editor knows nothing about
Vallox. The dashboard is the only hand-drawn view: fan speed large, four
temperatures, heater/bypass state, boost minutes left.

**State machine.** A page stack (depth ≤ 6); button events `PRESS / REPEAT /
LONG` with debounce and auto-repeat in the core; dashboard timeout 60 s; dim
after 5 min to 10 % backlight, wake on any button; `←` held at power-up restores
factory settings (decided earlier, `decisions.md`).

**Data flow.** The UI does not write registers directly. `vlx_client` (part of
`panel_ui`, on top of `vallox_protocol`) owns the poll sequence, the write
acknowledge byte, the 10 ms response window and a **shadow register map with
timestamps**. Views draw from the shadow; a value older than 30 s is drawn
dimmed — the UI is honest when the bus is quiet.

**Renderer.** RGB565 framebuffer, 150 kB (ESP32-C3 has 400 kB; a band buffer is
the fallback if RAM gets tight), dirty-rectangle list, primitives: fill, line,
rounded box, text, icon. Fonts from `tools/fontgen.py`: TTF → 4-bit antialiased
glyphs at 12/18/36 px, ASCII + `äöåÄÖÅ` + `°`, emitted as `.c` tables. One theme
struct (colours, spacing); dark by default because the panel hangs under the
machine in a plant room. Texts `texts_en.c` / `texts_fi.c` over one key enum; a
build-time check fails if the Finnish table lacks a key.

### 3.3 `vallox_machine`

**Interface.** `vlx_machine_init(&m, profile)`, `vlx_machine_feed(&m, bytes, n)`,
`vlx_machine_tick(&m, now_ms, out, &out_len)`, `vlx_machine_fault(&m, code)`.
It is itself a bus device at `0x11` using the same parser as the UI: all traffic
is genuine six-byte frames through a memory buffer, in tests and in the browser.

**Registers.** The whole map from `vallox_protocol.h` as a table: address,
read/write, default, encoding, and the confidence class from
`docs/research/protocol.md` (`manufacturer` / `implementations` / `assumed`). A
register the documents do not describe gets **no answer** — the UI sees a
timeout, not an invented value. Writes update the register and return the
acknowledge byte; polls are answered within a configurable delay (0 / 10 / 200
ms / never) so the UI's timeout path is testable. Broadcast set `2B 2C 35 34 32
33` every 12 s, the start-up burst, `SUSPEND/RESUME` around the CO₂ exchange.

**Physics.** Parameters: `t_outdoor` (from the side panel), `t_indoor` = 21 °C,
heat-recovery efficiency η = 0.6, heater setpoint and fan speed from registers.
Per tick: `t_extract → t_indoor` with a time constant; `t_supply = t_outdoor +
η·(t_extract − t_outdoor)`, raised to the setpoint when the heater bit is on;
`t_exhaust` likewise; time constant ∝ 1/fan_speed and scaled by `time_scale`
(1×/10×/60×) for the demo. Results go through the NTC table back to raw bytes
so the UI's decoding is exercised. Frost protection: `t_exhaust` under the
register threshold → supply-fan-stop bit and a fault; service reminder from the
month counter. Fault injection sets register 0x36 and the status bit until the
UI acknowledges.

**Not modelled:** CO₂/RH sensors (`fitted = 0`), the LON module, programme 2's
week clock — listed in the README as gaps, not guessed.

**Source of truth.** The model is exactly as right as `protocol.md`. A
discrepancy found in the M3 capture is corrected in the protocol document
first, then in the model and the UI in the same PR.

### 3.4 Browser simulator (`simulator/`)

**Build.** Emscripten compiles `vallox_protocol + vallox_machine + panel_ui +
panel_hal(web)` into one `panel.wasm` (`-O2`, no exceptions/iostream, ~100–200
kB). JS: three.js plus small vanilla code, bundled with Vite; no UI framework.
Output is a static `dist/`, deployed to GitHub Pages by `simulator.yml` from
`main` only. Licences: three.js MIT, Inter OFL, own code MIT — within the
existing `LICENSE.md` split.

**Scene.**
1. *Board.* `kicad-cli pcb export glb --subst-models --include-tracks
   --include-zones` of rev A, generated in CI, not committed. PBR materials as
   exported, one small HDRI environment, soft contact shadow. OrbitControls,
   default ¾ view, a "front view" button; touch works.
2. *Live display.* A plane at the display's active area (40.8 × 30.6 mm, position
   known from the library model) carrying a `DataTexture`; `hal_display_flush`
   writes RGB565 into shared memory and JS updates only the dirty rectangle
   (`texSubImage2D`). A thin translucent glass plane above it for the reflection.
   Backlight level → texture brightness.
3. *Buttons and LEDs.* Raycast onto hit boxes placed from the board coordinates
   (SW1–4 at x = 14.5/27.5/40.5/53.5, y = 52 mm — GLB node names are not relied
   on); keys `← → Enter Backspace` map to the same; a pressed button sinks 0.3 mm.
   LEDs are small emissive planes at D4/D6/D5 (yellow-green / yellow / red), light
   bloom. **Enclosure checkbox:** reserved; a second GLB in the same scene once
   the STEP exists; the core does not change.

**Side panel** (right, collapses below on phones): machine state (fan speed, four
temperatures, heater/bypass/frost bits), controls (outdoor temperature −25…+30
°C, time scale, response delay, language), fault injection, and a **bus log** in
hex, one frame per line (sender → receiver, register, value, direction). No
MQTT/Home Assistant mock in the first release.

**Realism, honestly.** GLB materials are KiCad's defaults, which are right for
rev A (green mask, HASL, white silk); the photographic quality comes from
lighting and the glass reflection, not from swapping the model.

## 4. Testing and CI

**Host tests (Unity, `firmware/test/host/`):** ladder thresholds incl. ±50 mV
edges; debounce/repeat/long as tick sequences; page-stack navigation; editor
limits and steps; timeouts; text-table key parity (build-time). **Golden images:**
scenarios (dashboard, list, editor, fault, dimmed) run as tick sequences →
RGB565 → PNG in `test/host/golden/`, compared pixel-exact; a deliberate look
change updates the golden in the same PR and the diff is visible in review.
`vallox_machine`: poll answers and acknowledges as real frames, broadcast timing,
physics direction and settling (not exact numbers — those are parameters),
fault injection, `assumed` registers behaving as documented. **End to end on the
host:** UI + machine over the memory bus — "press + three times → speed 4 on the
dashboard and register 0x29 reads 0x0F".

**Browser:** Emscripten toolchain in CI (cached), Vite build, one Playwright smoke
test (page loads, WASM starts, display texture non-empty, a click changes the
view). No visual 3D regression test — rendering varies by GPU; golden images
live in the deterministic host test.

**Workflows:** `firmware.yml` gains the new host tests; new `simulator.yml`: GLB
export with kicad-cli (same container as `hardware.yml`), Emscripten + Vite,
Playwright smoke, Pages deploy from `main`. README badge. CI green is not a
measurement: the README says the simulator runs a *simulated* machine and the
protocol is unverified until M3.

## 5. Staging

| # | Milestone | Done when |
|---|---|---|
| **S1** | `panel_hal.h` + `vallox_machine` + host runner with the memory bus | Machine answers polls and broadcasts in tests; physics moves the right way; e2e "UI stub reads the speed" passes. No UI yet |
| **S2** | `panel_ui`: renderer + fontgen + texts + dashboard + list/editor engine at option-A scope | Golden images for five views; tick-based button handling tested; e2e "press + → register changes" passes; host runner writes a PNG on demand |
| **S3** | `simulator/`: Emscripten build, three.js board from GLB, live display, buttons, LEDs, side panel, bus log, Pages deploy | Smoke test in CI, demo link in the README, enclosure checkbox present (empty) |
| **S4** | ESP-IDF host: ST7789 SPI, ADC buttons, UART + DE, NVS — **after bring-up only** | The same core runs on the board; golden image vs a photograph of the display. ESPHome wrapper grows with it |

S1–S3 are host-only and may proceed before boards exist. S4 waits for bring-up:
that is the `projektit.md` gate and it is a condition here, not a hope.

## 6. Out of scope for the first release

24 h graphs, MQTT/Home Assistant mock in the side panel, enclosure 3D (toggle
reserved), CO₂/RH sensors in the model, programme 2's week clock, an OTA menu
(arrives with the firmware stage), sounds. All listed in the README's "not yet"
section.

## 7. Open items carried into the plan

- Exact active-area origin of the display plane from the library model
  (`mironet-hw-lib`, `LCD_2.0in_HSD_HS20HS072RX`): read from the STEP, not
  assumed.
- Whether a 150 kB full framebuffer fits beside Wi-Fi on the ESP32-C3 is decided
  in S4 with a measured heap figure; the renderer keeps the band-buffer path
  possible (dirty rectangles only).
- Inter licence file and the fontgen tool's pinned TTF version go into
  `firmware/components/panel_ui/fonts/`.
