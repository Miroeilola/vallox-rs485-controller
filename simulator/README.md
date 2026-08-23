# Browser simulator

The controller's panel, running in a browser: the firmware's UI core
(`firmware/components/panel_ui`) and its machine emulator (`vallox_machine`)
compiled to WebAssembly with Emscripten, drawn on the rev A board's 3D model
(KiCad's GLB export) with three.js. One C core, four hosts in the design
(ESP-IDF, ESPHome, host tests, browser) — this is the second one that exists,
after the host tests (`docs/design/2026-08-23-panel-simulator-design.md`).

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
git submodule update --init   # lib/mironet-hw-lib holds the DS1 display model the GLB export needs
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
