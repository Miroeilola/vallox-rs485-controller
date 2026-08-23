# panel_ui — the panel's UI core

The menu engine, renderer, fonts, texts, button decoding and bus client that
run on the device, in the host tests and in the browser simulator — one copy,
driven by `panel_ui_tick(now_ms)` at 50 Hz, seeing the world only through
`panel_hal.h`. No ESP-IDF headers, no stdio, no floats, no allocation.

## Screen

320 × 240 RGB565. Top bar (title, bus state, fault icon) 0–23, content 24–207,
bottom bar 208–239 with the four buttons' current meaning drawn above the
physical buttons (`− + OK ←`, left to right; columns at x = 40/120/200/280 for
now — the active-area origin vs the switches is read from the library model in
S3/S4 and shifts those four numbers in `theme.h`).

## Pages are data

`pages.c` holds one `page_t` per page: DASHBOARD, LIST (rows → pages), EDITOR
(register + encoding + min/max/step), INFO. A new setting is one row. Value
encodings come from `vallox_protocol`, so the editor knows nothing about Vallox.

| Page | What |
|---|---|
| Dashboard | fan speed large + 8-segment bar, four temperatures, heater/bypass and boost line, fault banner. `−`/`+` change the speed directly, `OK` opens the menu |
| Menu | Fan speed · Heating setpoint · Status · Settings |
| Fan speed | editor 1…8, saved through the client (0x29 is on the write allow-list) |
| Heating setpoint | editor 10…25 °C, **read-only until verified** — 0xA4 is not on the allow-list, so `+ −` are inert and `OK` leaves |
| Status | fault, service months, bus state, firmware version |
| Settings → Language | English / Suomi, stored under `"lang"` |

Any page: `←` held 1 s → dashboard. 60 s idle → dashboard. 5 min idle → dim to
10 % backlight and hide the button labels; any press wakes and is consumed.
`←` held from power-up for 3 s → factory reset (language), 2 s splash.

## Bus client

`vlx_client` keeps a shadow of the machine's registers with timestamps, polls
one register every 250 ms (list in `vlx_client_default_poll`), takes the
machine's broadcasts, writes with the one-byte acknowledge (3 tries, 50 ms
window), and declares a bus fault after 10 unanswered polls (protocol.md
claim 24). A write is refused before a byte leaves unless
`vlx_register_is_write_allowed()` and `vlx_value_is_valid_for()` agree. Values
older than 30 s are drawn dimmed; unknown values as `--`.

## Fonts and texts

Inter 4.1 (OFL) pre-rendered by `tools/fontgen.py` into 4-bit tables at 12/18/36
px; see `fonts/README.md`. Texts are keys in `texts.h` with `en`/`fi` tables;
a host test fails on a missing key or a glyph the fonts lack.

## Tests

`firmware/test/host/`: `test_panel_fonts_gfx.c`, `test_panel_buttons_texts.c`,
`test_vlx_client.c`, `test_panel_ui.c` (incl. the spec's e2e "press + three
times → speed 4, 0x29 reads 0x0F"), `test_panel_golden.c` with five golden
images in `golden/` (`make golden` regenerates; a look change updates them in
the same PR). `panel_host` renders any state to a PNG from the command line.

## Known gaps

- Fault names exist for the six documented codes; others show `Fault N`.
- The acknowledge byte is matched by value within the 50 ms window; a
  broadcast byte equal to it would be mistaken for the ack (re-polled right
  after, so the shadow self-corrects).
- Full-page redraw at ≤ 5 Hz: the dashboard costs 500 `hal_display_flush` calls
  and 85 709 pixels per render (measured on the host, 2026-08-23; text drawing
  was 327 calls / 77 582 px before this change started marking one dirty
  rectangle per string instead of one per glyph — merging narrow per-glyph
  rects had let the pre-existing dirty-list overflow merge coalesce them into
  the same bounding boxes as the surrounding fills, for free; the now-correct
  per-string rects are separate instead, so `gfx_flush`'s one-call-per-scanline-
  per-rectangle design pushes the row total up, not down); flush is one call
  per scanline per dirty rectangle, so the ESP-IDF host should batch rows or
  push the frame whole (S4 decides with a measured heap/SPI figure).
- No scrolling lists (every list fits the screen), no boost/bypass controls,
  no installer menu — option-A scope, the table grows when the M3 capture
  confirms registers.
