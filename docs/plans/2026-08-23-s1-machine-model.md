# S1 — Machine emulator, HAL contract and host memory bus: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `vallox_machine` (an emulator of the Vallox mainboard on the RS-485 bus with a coarse thermal model), the `panel_hal.h` contract every host implements, and a host implementation of that HAL with a memory bus — so that a "panel" can talk to a simulated machine in real six-byte frames inside a host test, with no UI yet.

**Architecture:** Three IDF-free C components under `firmware/components/`: `panel_hal` (header only, the contract), `vallox_machine` (register table + protocol behaviour + tick physics + fault injection, itself a bus device at 0x11 using the existing `vallox_protocol` parser), and a host HAL in `firmware/test/host/hal_host.c` (ring-buffer memory bus, fake clock, RAM key-value store, framebuffer/LED stubs). Tests follow the existing pattern: one `test_<component>.c` with its own `main`, the `CHECK` macro, built by the host `Makefile` and run in CI.

**Tech Stack:** C11, `cc -std=c11 -Wall -Wextra -Werror`, GNU make, existing `vallox_protocol` API, GitHub Actions (`firmware.yml`). No new dependencies.

**Spec:** `docs/design/2026-08-23-panel-simulator-design.md` — sections 3.1 (HAL), 3.3 (machine), 4 (tests), 5 (S1 row).

## Global Constraints

- Every source file starts with `// SPDX-License-Identifier: MIT` and `// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet` (julkaisu.md).
- Components under `firmware/components/` include **no ESP-IDF headers** and no `<stdio.h>` (firmware.md: the same source compiles for IDF, ESPHome, host and Emscripten). Test files may use stdio.
- All code, comments, commit messages in English; Conventional Commits; commits by Miro Eilola, no AI attribution (git.md).
- The machine model is exactly as right as `docs/research/protocol.md`; a register the documents do not describe gets **no answer**. Never invent a value.
- Frames on the memory bus are real six-byte frames built with `vlx_make_poll` / `vlx_make_write` / `vlx_frame_encode` — no shortcut structs across the bus.
- Work on branch `feat/s1-machine-model` off `main` (ef49c17 or later); one PR at the end, CI green before merge.
- Do not touch `firmware/components/vallox_protocol/` except where a task says so (none do).

---

## File structure

| Path | Responsibility |
|---|---|
| `firmware/components/panel_hal/include/panel_hal.h` | The HAL contract: display, buttons, LEDs, backlight, bus, time, store. Header only. |
| `firmware/components/panel_hal/CMakeLists.txt` | `idf_component_register(INCLUDE_DIRS "include")` so IDF and Emscripten builds see it. |
| `firmware/components/vallox_machine/include/vallox_machine.h` | Public API: `vlx_machine_t`, init/feed/tick/fault/params, register access for tests. |
| `firmware/components/vallox_machine/vallox_machine.c` | Register table, parser, poll answers, writes + acknowledge, reply delay, broadcast scheduler, fault injection. |
| `firmware/components/vallox_machine/vallox_machine_physics.c` | The thermal model: one `vlx_machine_physics_step()` called from tick. |
| `firmware/components/vallox_machine/vallox_machine_regs.h` | The register table (address, access, default, confidence) as a static array shared by the two `.c` files. |
| `firmware/components/vallox_machine/CMakeLists.txt` | `idf_component_register(SRCS ... INCLUDE_DIRS "include" REQUIRES vallox_protocol)`. |
| `firmware/components/vallox_machine/README.md` | What is modelled, what is not, where every behaviour comes from. |
| `firmware/test/host/check.h` | The `CHECK` macro and counters, shared by the new test files. |
| `firmware/test/host/hal_host.h` / `hal_host.c` | Host implementation of `panel_hal.h` + the memory bus (`membus`) the tests pump. |
| `firmware/test/host/test_hal_host.c` | Tests for the host HAL itself. |
| `firmware/test/host/test_vallox_machine.c` | Tests for the machine: polls, writes, delay, broadcasts, physics, faults. |
| `firmware/test/host/test_e2e_membus.c` | End to end: a panel stub over `hal_bus_*` talks to the machine through the memory bus. |
| `firmware/test/host/Makefile` | Builds and runs all four test binaries. |
| `.github/workflows/firmware.yml` | Runs `make test` instead of the single binary. |

---

### Task 1: `panel_hal.h` contract and the host HAL with a memory bus

**Files:**
- Create: `firmware/components/panel_hal/include/panel_hal.h`
- Create: `firmware/components/panel_hal/CMakeLists.txt`
- Create: `firmware/test/host/check.h`
- Create: `firmware/test/host/hal_host.h`
- Create: `firmware/test/host/hal_host.c`
- Create: `firmware/test/host/test_hal_host.c`
- Modify: `firmware/test/host/Makefile`

**Interfaces:**
- Produces `panel_hal.h`:
  ```c
  void     hal_display_flush(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *rgb565);
  uint16_t hal_buttons_read_mv(void);
  void     hal_leds_set(bool pwr, bool bus, bool fault);
  void     hal_backlight_set(uint8_t level);
  size_t   hal_bus_write(const uint8_t *buf, size_t len);   // returns bytes accepted
  size_t   hal_bus_read(uint8_t *buf, size_t max);          // returns bytes copied, 0 if none
  uint32_t hal_time_ms(void);
  bool     hal_store_get(const char *key, void *buf, size_t len);
  bool     hal_store_put(const char *key, const void *buf, size_t len);
  ```
- Produces `hal_host.h` (test-side controls):
  ```c
  void     hal_host_reset(void);                       // clears bus, time=0, store, framebuffer, leds
  void     hal_host_advance_ms(uint32_t ms);           // fake clock
  void     hal_host_set_buttons_mv(uint16_t mv);
  // memory bus: the panel side is hal_bus_*; the machine side is these two
  size_t   membus_machine_read(uint8_t *buf, size_t max);   // bytes the panel wrote
  size_t   membus_machine_write(const uint8_t *buf, size_t len); // bytes the panel will read
  // inspection
  const uint16_t *hal_host_framebuffer(void);          // 320*240 RGB565
  void     hal_host_leds(bool *pwr, bool *bus, bool *fault);
  uint8_t  hal_host_backlight(void);
  ```

- [ ] **Step 1: Write the HAL contract header**

`firmware/components/panel_hal/include/panel_hal.h`:

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The contract between the panel core (panel_ui, vallox_machine in the
// simulator) and whatever hosts it: ESP-IDF, ESPHome, the host test runner, the
// Emscripten build. Plain C functions resolved at link time — one implementation
// per host, none of them inside the core.
//
// The core is tick-driven and single-threaded. Every function here must be safe
// to call from the tick and must not block.

#ifndef PANEL_HAL_H
#define PANEL_HAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_DISPLAY_W 320
#define HAL_DISPLAY_H 240

// Copy a rectangle of RGB565 pixels (row-major, w*h entries) to the display.
void hal_display_flush(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       const uint16_t *rgb565);

// Button-ladder voltage in millivolts. The core maps it to a button; the
// thresholds are core code so they are tested on the host.
uint16_t hal_buttons_read_mv(void);

void hal_leds_set(bool pwr, bool bus, bool fault);
void hal_backlight_set(uint8_t level);   // 0 = off, 255 = full

// RS-485 bytes. Driver-enable timing and the idle-gap detection belong to the
// host; the core only sees bytes.
size_t hal_bus_write(const uint8_t *buf, size_t len);   // bytes accepted
size_t hal_bus_read(uint8_t *buf, size_t max);          // bytes copied, 0 if none

uint32_t hal_time_ms(void);   // monotonic, wraps after 49 days — use differences

// Settings. Keys are short ASCII; values opaque bytes. get returns false when
// the key is absent or the stored length differs from len.
bool hal_store_get(const char *key, void *buf, size_t len);
bool hal_store_put(const char *key, const void *buf, size_t len);

#ifdef __cplusplus
}
#endif
#endif
```

`firmware/components/panel_hal/CMakeLists.txt`:

```cmake
idf_component_register(INCLUDE_DIRS "include")
```

- [ ] **Step 2: Write the shared CHECK macro**

`firmware/test/host/check.h`:

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Minimal assertion helper shared by the host test binaries. Each test file
// defines its own `checks` and `failures` counters and a main() that prints
// them; see test_vallox_protocol.c for the pattern.
#ifndef CHECK_H
#define CHECK_H
#include <stdio.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        checks++;                                                              \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        long long _a = (long long)(a), _b = (long long)(b);                    \
        checks++;                                                              \
        if (_a != _b) {                                                        \
            printf("FAIL %s:%d  %s == %s  (%lld != %lld)\n", __FILE__,         \
                   __LINE__, #a, #b, _a, _b);                                  \
            failures++;                                                        \
        }                                                                      \
    } while (0)

#define REPORT() (printf("%d checks, %d failures\n", checks, failures), failures == 0 ? 0 : 1)
#endif
```

- [ ] **Step 3: Write the failing host-HAL test**

`firmware/test/host/test_hal_host.c`:

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include <string.h>
#include "check.h"
#include "panel_hal.h"
#include "hal_host.h"

static void test_bus_roundtrip_panel_to_machine(void)
{
    hal_host_reset();
    const uint8_t tx[6] = {1, 2, 3, 4, 5, 6};
    CHECK_EQ(hal_bus_write(tx, 6), 6);
    uint8_t rx[16];
    CHECK_EQ(membus_machine_read(rx, sizeof rx), 6);
    CHECK(memcmp(rx, tx, 6) == 0);
    CHECK_EQ(membus_machine_read(rx, sizeof rx), 0);   // drained
}

static void test_bus_roundtrip_machine_to_panel(void)
{
    hal_host_reset();
    const uint8_t tx[3] = {0xAA, 0xBB, 0xCC};
    CHECK_EQ(membus_machine_write(tx, 3), 3);
    uint8_t rx[2];
    CHECK_EQ(hal_bus_read(rx, 2), 2);           // partial reads keep the rest
    CHECK(rx[0] == 0xAA && rx[1] == 0xBB);
    CHECK_EQ(hal_bus_read(rx, 2), 1);
    CHECK(rx[0] == 0xCC);
    CHECK_EQ(hal_bus_read(rx, 2), 0);
}

static void test_bus_ring_wraps(void)
{
    hal_host_reset();
    uint8_t b = 0;
    for (int i = 0; i < 1000; i++) {          // more than the ring size
        b = (uint8_t)i;
        CHECK_EQ(hal_bus_write(&b, 1), 1);
        uint8_t r = 0;
        CHECK_EQ(membus_machine_read(&r, 1), 1);
        CHECK_EQ(r, b);
    }
}

static void test_time_is_fake_and_monotonic(void)
{
    hal_host_reset();
    CHECK_EQ(hal_time_ms(), 0);
    hal_host_advance_ms(10);
    hal_host_advance_ms(5);
    CHECK_EQ(hal_time_ms(), 15);
}

static void test_store_roundtrip_and_length_check(void)
{
    hal_host_reset();
    uint8_t v = 7, out = 0;
    CHECK(!hal_store_get("lang", &out, 1));
    CHECK(hal_store_put("lang", &v, 1));
    CHECK(hal_store_get("lang", &out, 1));
    CHECK_EQ(out, 7);
    uint16_t wrong;
    CHECK(!hal_store_get("lang", &wrong, 2));   // length mismatch is a miss
}

static void test_display_leds_backlight_are_observable(void)
{
    hal_host_reset();
    uint16_t px[4] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
    hal_display_flush(10, 20, 2, 2, px);
    const uint16_t *fb = hal_host_framebuffer();
    CHECK_EQ(fb[20 * HAL_DISPLAY_W + 10], 0xF800);
    CHECK_EQ(fb[20 * HAL_DISPLAY_W + 11], 0x07E0);
    CHECK_EQ(fb[21 * HAL_DISPLAY_W + 10], 0x001F);
    CHECK_EQ(fb[21 * HAL_DISPLAY_W + 11], 0xFFFF);
    hal_leds_set(true, false, true);
    bool p, b, f;
    hal_host_leds(&p, &b, &f);
    CHECK(p && !b && f);
    hal_backlight_set(200);
    CHECK_EQ(hal_host_backlight(), 200);
    hal_host_set_buttons_mv(430);
    CHECK_EQ(hal_buttons_read_mv(), 430);
}

int main(void)
{
    test_bus_roundtrip_panel_to_machine();
    test_bus_roundtrip_machine_to_panel();
    test_bus_ring_wraps();
    test_time_is_fake_and_monotonic();
    test_store_roundtrip_and_length_check();
    test_display_leds_backlight_are_observable();
    return REPORT();
}
```

- [ ] **Step 4: Extend the Makefile to build it and run it to see the failure**

Replace `firmware/test/host/Makefile` with:

```make
# Host-side unit tests: no ESP-IDF, no hardware. `make test` builds and runs all.
CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O1 -g
COMP    := ../../components
PROTO   := $(COMP)/vallox_protocol
MACH    := $(COMP)/vallox_machine
HAL     := $(COMP)/panel_hal
INC     := -I$(PROTO)/include -I$(MACH)/include -I$(HAL)/include -I.

BINS := run_tests run_tests_hal run_tests_machine run_tests_e2e

run_tests: $(PROTO)/vallox_protocol.c test_vallox_protocol.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

run_tests_hal: hal_host.c test_hal_host.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

run_tests_machine: $(PROTO)/vallox_protocol.c $(MACH)/vallox_machine.c $(MACH)/vallox_machine_physics.c test_vallox_machine.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

run_tests_e2e: $(PROTO)/vallox_protocol.c $(MACH)/vallox_machine.c $(MACH)/vallox_machine_physics.c hal_host.c test_e2e_membus.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

.PHONY: test clean all
all: $(BINS)
test: all
	@set -e; for b in $(BINS); do echo "== $$b"; ./$$b; done

clean:
	rm -rf $(BINS) *.dSYM
```

Run: `cd firmware/test/host && make run_tests_hal`
Expected: FAIL to compile — `hal_host.h: No such file`.

- [ ] **Step 5: Implement the host HAL**

`firmware/test/host/hal_host.h`:

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Host implementation of panel_hal.h for tests (and a template for the
// Emscripten host): a memory bus instead of a UART, a fake clock, a RAM store.
#ifndef HAL_HOST_H
#define HAL_HOST_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void hal_host_reset(void);
void hal_host_advance_ms(uint32_t ms);
void hal_host_set_buttons_mv(uint16_t mv);

// The machine side of the memory bus. The panel side is hal_bus_write/read.
size_t membus_machine_read(uint8_t *buf, size_t max);
size_t membus_machine_write(const uint8_t *buf, size_t len);

const uint16_t *hal_host_framebuffer(void);   // HAL_DISPLAY_W * HAL_DISPLAY_H
void hal_host_leds(bool *pwr, bool *bus, bool *fault);
uint8_t hal_host_backlight(void);
#endif
```

`firmware/test/host/hal_host.c`:

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include "hal_host.h"
#include <string.h>
#include "panel_hal.h"

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

static ring_t s_to_machine, s_to_panel;
static uint32_t s_now_ms;
static uint16_t s_buttons_mv = 3300;
static uint16_t s_fb[HAL_DISPLAY_W * HAL_DISPLAY_H];
static bool s_led_pwr, s_led_bus, s_led_fault;
static uint8_t s_backlight;

#define STORE_MAX 16
typedef struct { char key[16]; uint8_t val[32]; size_t len; bool used; } slot_t;
static slot_t s_store[STORE_MAX];

void hal_host_reset(void)
{
    memset(&s_to_machine, 0, sizeof s_to_machine);
    memset(&s_to_panel, 0, sizeof s_to_panel);
    s_now_ms = 0;
    s_buttons_mv = 3300;
    memset(s_fb, 0, sizeof s_fb);
    s_led_pwr = s_led_bus = s_led_fault = false;
    s_backlight = 0;
    memset(s_store, 0, sizeof s_store);
}

void hal_host_advance_ms(uint32_t ms) { s_now_ms += ms; }
void hal_host_set_buttons_mv(uint16_t mv) { s_buttons_mv = mv; }

size_t membus_machine_read(uint8_t *buf, size_t max) { return ring_get(&s_to_machine, buf, max); }
size_t membus_machine_write(const uint8_t *buf, size_t len) { return ring_put(&s_to_panel, buf, len); }

const uint16_t *hal_host_framebuffer(void) { return s_fb; }
void hal_host_leds(bool *pwr, bool *bus, bool *fault) { *pwr = s_led_pwr; *bus = s_led_bus; *fault = s_led_fault; }
uint8_t hal_host_backlight(void) { return s_backlight; }

// ---- panel_hal.h --------------------------------------------------------

void hal_display_flush(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *rgb565)
{
    for (uint16_t r = 0; r < h; r++) {
        if (y + r >= HAL_DISPLAY_H) break;
        for (uint16_t c = 0; c < w; c++) {
            if (x + c >= HAL_DISPLAY_W) break;
            s_fb[(y + r) * HAL_DISPLAY_W + (x + c)] = rgb565[r * w + c];
        }
    }
}

uint16_t hal_buttons_read_mv(void) { return s_buttons_mv; }
void hal_leds_set(bool pwr, bool bus, bool fault) { s_led_pwr = pwr; s_led_bus = bus; s_led_fault = fault; }
void hal_backlight_set(uint8_t level) { s_backlight = level; }
size_t hal_bus_write(const uint8_t *buf, size_t len) { return ring_put(&s_to_machine, buf, len); }
size_t hal_bus_read(uint8_t *buf, size_t max) { return ring_get(&s_to_panel, buf, max); }
uint32_t hal_time_ms(void) { return s_now_ms; }

static slot_t *find(const char *key)
{
    for (int i = 0; i < STORE_MAX; i++)
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
        for (int i = 0; i < STORE_MAX && !s; i++)
            if (!s_store[i].used) s = &s_store[i];
        if (!s) return false;
        s->used = true;
        strcpy(s->key, key);
    }
    memcpy(s->val, buf, len);
    s->len = len;
    return true;
}
```

- [ ] **Step 6: Run the HAL test**

Run: `cd firmware/test/host && make run_tests_hal && ./run_tests_hal`
Expected: `... checks, 0 failures`, exit 0. (`make test` will still fail because the machine sources do not exist yet — that is expected until Task 2.)

- [ ] **Step 7: Commit**

```bash
git add firmware/components/panel_hal firmware/test/host/check.h firmware/test/host/hal_host.[ch] firmware/test/host/test_hal_host.c firmware/test/host/Makefile
git commit -m "feat(hal): panel HAL contract and a host implementation with a memory bus

The core never touches a UART, a clock or storage; this header is the
contract every host implements. The host version gives tests a ring-buffer
bus, a fake clock and a RAM store so a panel and a simulated machine can
talk in real frames without hardware."
```

---

### Task 2: Machine skeleton — register table, parser, poll answers

**Files:**
- Create: `firmware/components/vallox_machine/include/vallox_machine.h`
- Create: `firmware/components/vallox_machine/vallox_machine_regs.h`
- Create: `firmware/components/vallox_machine/vallox_machine.c`
- Create: `firmware/components/vallox_machine/vallox_machine_physics.c` (stub for now, filled in Task 5)
- Create: `firmware/components/vallox_machine/CMakeLists.txt`
- Create: `firmware/test/host/test_vallox_machine.c`

**Interfaces:**
- Consumes from `vallox_protocol.h`: `vlx_parser_*`, `vlx_frame_t`, `vlx_frame_is_poll`, `vlx_make_write`, `vlx_frame_encode`, `VLX_ADDR_*`, `VLX_REG_*`, `vlx_temp_to_raw`, `vlx_fan_speed_to_raw`.
- Produces:
  ```c
  typedef enum { VLX_CONF_MANUFACTURER, VLX_CONF_IMPLEMENTATIONS, VLX_CONF_ASSUMED } vlx_conf_t;
  typedef struct { uint8_t reg; bool writable; uint8_t def; vlx_conf_t conf; } vlx_machine_reg_t;
  typedef struct vlx_machine vlx_machine_t;   // opaque-ish: fields documented in the header
  void   vlx_machine_init(vlx_machine_t *m);
  void   vlx_machine_feed(vlx_machine_t *m, const uint8_t *bytes, size_t n);
  size_t vlx_machine_tick(vlx_machine_t *m, uint32_t now_ms, uint8_t *out, size_t max);
  bool   vlx_machine_reg_known(const vlx_machine_t *m, uint8_t reg);
  uint8_t vlx_machine_reg_get(const vlx_machine_t *m, uint8_t reg);
  void   vlx_machine_reg_set(vlx_machine_t *m, uint8_t reg, uint8_t value);  // test/side-panel use
  ```
  Fields of `vlx_machine_t` used by later tasks: `reply_delay_ms` (uint16_t; `VLX_MACHINE_NEVER = 0xFFFF`), `broadcast_period_ms` (uint32_t, default 12000), `regs[256]`, `known[256]`.

- [ ] **Step 1: Write the failing tests (polls)**

`firmware/test/host/test_vallox_machine.c`:

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The machine model is exactly as right as docs/research/protocol.md. These
// tests pin the model to that document, not to a real machine.
#include <string.h>
#include "check.h"
#include "vallox_machine.h"
#include "vallox_protocol.h"

#define PANEL VLX_ADDR_PANEL_DEFAULT
#define MACHINE VLX_ADDR_MAINBOARD_1

// Helpers: send one frame, tick once at now_ms, decode whatever came back.
static size_t send_and_tick(vlx_machine_t *m, const uint8_t *frame, uint32_t now_ms,
                            uint8_t *out, size_t max)
{
    vlx_machine_feed(m, frame, VLX_FRAME_LEN);
    return vlx_machine_tick(m, now_ms, out, max);
}

static void test_poll_known_register_is_answered_with_its_value(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    vlx_machine_reg_set(&m, VLX_REG_FAN_SPEED, vlx_fan_speed_to_raw(3));
    uint8_t poll[VLX_FRAME_LEN], out[32];
    vlx_make_poll(PANEL, MACHINE, VLX_REG_FAN_SPEED, poll);
    size_t n = send_and_tick(&m, poll, 0, out, sizeof out);
    CHECK_EQ(n, VLX_FRAME_LEN);
    vlx_frame_t f;
    CHECK(vlx_frame_decode(out, &f));
    CHECK_EQ(f.sender, MACHINE);
    CHECK_EQ(f.receiver, PANEL);
    CHECK_EQ(f.reg, VLX_REG_FAN_SPEED);
    CHECK_EQ(f.value, vlx_fan_speed_to_raw(3));
    CHECK(!vlx_frame_is_poll(&f));
}

static void test_poll_unknown_register_gets_no_answer(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    CHECK(!vlx_machine_reg_known(&m, 0xC1));     // not in the documents
    uint8_t poll[VLX_FRAME_LEN], out[32];
    vlx_make_poll(PANEL, MACHINE, 0xC1, poll);
    CHECK_EQ(send_and_tick(&m, poll, 0, out, sizeof out), 0);
}

static void test_poll_for_another_address_is_ignored(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t poll[VLX_FRAME_LEN], out[32];
    vlx_make_poll(PANEL, 0x22 /* another panel */, VLX_REG_FAN_SPEED, poll);
    CHECK_EQ(send_and_tick(&m, poll, 0, out, sizeof out), 0);
}

static void test_poll_to_mainboard_broadcast_is_answered(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t poll[VLX_FRAME_LEN], out[32];
    vlx_make_poll(PANEL, VLX_ADDR_MAINBOARDS, VLX_REG_STATUS, poll);
    CHECK_EQ(send_and_tick(&m, poll, 0, out, sizeof out), VLX_FRAME_LEN);
}

static void test_feed_survives_garbage_and_chunking(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t poll[VLX_FRAME_LEN], out[32];
    vlx_make_poll(PANEL, MACHINE, VLX_REG_STATUS, poll);
    const uint8_t junk[3] = {0x55, 0xAA, 0x01};
    vlx_machine_feed(&m, junk, 3);
    vlx_machine_feed(&m, poll, 2);
    vlx_machine_feed(&m, poll + 2, 4);
    CHECK_EQ(vlx_machine_tick(&m, 0, out, sizeof out), VLX_FRAME_LEN);
}

static void test_defaults_are_a_plausible_machine(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    CHECK(vlx_machine_reg_known(&m, VLX_REG_FAN_SPEED));
    CHECK_EQ(vlx_fan_speed_from_raw(vlx_machine_reg_get(&m, VLX_REG_FAN_SPEED)), 3);
    CHECK_EQ(vlx_temp_table(vlx_machine_reg_get(&m, VLX_REG_TEMP_OUTDOOR)), 5);
    CHECK_EQ(vlx_temp_table(vlx_machine_reg_get(&m, VLX_REG_HEAT_SETPOINT)), 18);
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_FAULT), 0);
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_CO2_SENSORS_FITTED), 0);
}

int main(void)
{
    test_poll_known_register_is_answered_with_its_value();
    test_poll_unknown_register_gets_no_answer();
    test_poll_for_another_address_is_ignored();
    test_poll_to_mainboard_broadcast_is_answered();
    test_feed_survives_garbage_and_chunking();
    test_defaults_are_a_plausible_machine();
    return REPORT();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cd firmware/test/host && make run_tests_machine`
Expected: FAIL — `vallox_machine.h: No such file`.

- [ ] **Step 3: Write the register table**

`firmware/components/vallox_machine/vallox_machine_regs.h`:

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The register map the emulator answers for. Source: docs/research/protocol.md
// and vallox_protocol.h. `conf` is the confidence class from the claim table
// there: manufacturer = in Vallox's own document, implementations = agreed by
// the four independent implementations, assumed = reported by one source or
// inferred. A register absent from this table is unknown and gets NO answer.
// Defaults describe a plausible idle machine in a Finnish autumn, not a real one.
#ifndef VALLOX_MACHINE_REGS_H
#define VALLOX_MACHINE_REGS_H
#include "vallox_machine.h"
#include "vallox_protocol.h"

// Raw NTC values for the defaults come from vlx_temp_to_raw() at init time;
// here only the degrees are listed, so this table stays readable.
typedef struct { uint8_t reg; bool writable; int16_t def_c; vlx_conf_t conf; } temp_reg_def_t;

static const vlx_machine_reg_t k_regs[] = {
    // measurements, read only
    { VLX_REG_IO_FAN_RELAYS,      false, 0x04, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_IO_MULTI_1,         false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_IO_MULTI_2,         false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_FAN_SPEED,          false, 0x07, VLX_CONF_MANUFACTURER },   // speed 3
    { VLX_REG_RH_HIGHEST,         false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_CO2_HIGH,           false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_CO2_LOW,            false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_CO2_SENSORS_FITTED, false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_CURRENT_MA,         false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_RH_SENSOR_1,        false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_RH_SENSOR_2,        false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_FAULT,              false, 0x00, VLX_CONF_MANUFACTURER },
    { VLX_REG_POST_HEAT_ON_CNT,   false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_POST_HEAT_OFF_CNT,  false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_POST_HEAT_TARGET,   false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    // flags
    { VLX_REG_FLAGS_1, false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_FLAGS_2, true,  0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_FLAGS_3, false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_FLAGS_4, false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_FLAGS_5, false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_FLAGS_6, true,  0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_BOOST_MINUTES, true, 0x00, VLX_CONF_IMPLEMENTATIONS },
    // settings, writable
    { VLX_REG_STATUS,              true, 0x03, VLX_CONF_MANUFACTURER },   // power on, CO2 off... bits per header
    { VLX_REG_FAN_SPEED_MAX,       true, 0xFF, VLX_CONF_IMPLEMENTATIONS }, // 8
    { VLX_REG_SERVICE_INTERVAL,    true, 0x04, VLX_CONF_IMPLEMENTATIONS }, // months
    { VLX_REG_SUPPLY_FAN_STOP,     true, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_FAN_SPEED_DEFAULT,   true, 0x07, VLX_CONF_IMPLEMENTATIONS }, // 3
    { VLX_REG_PROGRAM,             true, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_SERVICE_MONTHS_LEFT, true, 0x03, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_RH_BASIC_LEVEL,      true, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_DC_FAN_SUPPLY,       true, 0x64, VLX_CONF_IMPLEMENTATIONS }, // 100 %
    { VLX_REG_DC_FAN_EXHAUST,      true, 0x64, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_CO2_SETPOINT_HIGH,   true, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_CO2_SETPOINT_LOW,    true, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_PROGRAM_2,           true, 0x00, VLX_CONF_IMPLEMENTATIONS },
};

// Temperature registers: the default is in degrees and converted at init.
static const temp_reg_def_t k_temp_regs[] = {
    { VLX_REG_TEMP_OUTDOOR,       false,  5, VLX_CONF_MANUFACTURER },
    { VLX_REG_TEMP_EXHAUST,       false,  9, VLX_CONF_MANUFACTURER },
    { VLX_REG_TEMP_EXTRACT,       false, 21, VLX_CONF_MANUFACTURER },
    { VLX_REG_TEMP_SUPPLY,        false, 17, VLX_CONF_MANUFACTURER },
    { VLX_REG_HEAT_SETPOINT,      true,  18, VLX_CONF_MANUFACTURER },
    { VLX_REG_PREHEAT_SETPOINT,   true,  -6, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_BYPASS_SETPOINT,    true,  10, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_DEFROST_HYSTERESIS, true,   3, VLX_CONF_IMPLEMENTATIONS },
};
// Deliberately absent: the legacy temperature set 0x58-0x5C (assumed; which set
// a machine uses is discovered from traffic), 0xC0 (undocumented), anything
// not in vallox_protocol.h. Unknown registers get no answer.
#endif
```

- [ ] **Step 4: Write the public header**

`firmware/components/vallox_machine/include/vallox_machine.h`:

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Emulator of the Vallox DIGIT mainboard as seen from the RS-485 bus: a
// register map, the poll/write/acknowledge behaviour and the periodic
// broadcasts from docs/research/protocol.md, plus a coarse thermal model so the
// readings move when the settings do. It is itself a bus device at 0x11 and
// speaks in real six-byte frames through whatever transport the host gives it.
//
// It is exactly as right as the protocol document. Nothing here has been
// verified against a machine; every behaviour names its confidence class.
//
// Free of ESP-IDF headers: compiled by the host tests and the browser
// simulator (Emscripten). Not used on the device.

#ifndef VALLOX_MACHINE_H
#define VALLOX_MACHINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "vallox_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VLX_CONF_MANUFACTURER,     // in Vallox's own protocol document
    VLX_CONF_IMPLEMENTATIONS,  // agreed by the four independent implementations
    VLX_CONF_ASSUMED,          // one source, or inferred
} vlx_conf_t;

typedef struct {
    uint8_t    reg;
    bool       writable;
    uint8_t    def;
    vlx_conf_t conf;
} vlx_machine_reg_t;

#define VLX_MACHINE_NEVER 0xFFFFu      // reply_delay_ms: never answer a poll

// Thermal model parameters. Temperatures in degrees C (float is fine here: this
// code does not run on the MCU).
typedef struct {
    float t_outdoor;        // set from the side panel / test; default 5
    float t_indoor;         // what the rooms sit at; default 21
    float efficiency;       // heat-recovery efficiency 0..1; default 0.6
    float tau_s;            // time constant at fan speed 1, seconds; default 600
    float time_scale;       // 1 = real time, 10, 60 for demos; default 1
} vlx_machine_params_t;

typedef struct {
    // registers
    uint8_t regs[256];
    bool    known[256];
    bool    writable[256];
    vlx_conf_t conf[256];
    // protocol behaviour
    uint16_t reply_delay_ms;        // 0 default; 10 / 200 / VLX_MACHINE_NEVER for tests
    uint32_t broadcast_period_ms;   // 12000
    uint32_t broadcast_spacing_ms;  // 130 between the frames of one broadcast round
    // internal state
    vlx_parser_t parser;
    uint8_t  pending[64];           // bytes queued to go out (answers, acks)
    size_t   pending_len;
    uint32_t pending_due_ms;        // not before this time
    bool     pending_armed;
    uint32_t last_broadcast_ms;
    uint8_t  broadcast_idx;         // index into the broadcast set during a round
    uint32_t next_broadcast_frame_ms;
    uint32_t last_tick_ms;
    bool     have_tick;
    // physics
    vlx_machine_params_t p;
    float t_extract, t_supply, t_exhaust;   // continuous state; regs hold the NTC-rounded copy
    uint32_t service_elapsed_ms;
} vlx_machine_t;

void   vlx_machine_init(vlx_machine_t *m);

// Bytes heard on the bus (anything: our own echo is filtered by address).
void   vlx_machine_feed(vlx_machine_t *m, const uint8_t *bytes, size_t n);

// Advance time, run the physics, and write whatever the machine sends now
// (poll answers, acknowledges, broadcasts) into out. Returns bytes written.
size_t vlx_machine_tick(vlx_machine_t *m, uint32_t now_ms, uint8_t *out, size_t max);

// Register access for tests and the simulator side panel.
bool    vlx_machine_reg_known(const vlx_machine_t *m, uint8_t reg);
uint8_t vlx_machine_reg_get(const vlx_machine_t *m, uint8_t reg);
void    vlx_machine_reg_set(vlx_machine_t *m, uint8_t reg, uint8_t value);

// Fault injection: sets VLX_REG_FAULT and the fault bit; cleared by
// vlx_machine_fault_clear() or by the panel writing 0 to VLX_REG_FAULT
// (the clearing write is ASSUMED — no document describes how the panel
// acknowledges a fault).
void vlx_machine_fault(vlx_machine_t *m, vlx_fault_t code);
void vlx_machine_fault_clear(vlx_machine_t *m);

// Physics step, called from tick; exposed for the physics tests.
void vlx_machine_physics_step(vlx_machine_t *m, float dt_s);

#ifdef __cplusplus
}
#endif
#endif
```

- [ ] **Step 5: Write the skeleton implementation (polls only; writes, delay, broadcasts and physics come in later tasks)**

`firmware/components/vallox_machine/vallox_machine.c`:

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include "vallox_machine.h"
#include <string.h>
#include "vallox_machine_regs.h"

#define ME VLX_ADDR_MAINBOARD_1

static void queue_bytes(vlx_machine_t *m, const uint8_t *b, size_t n, uint32_t due_ms)
{
    if (m->pending_len + n > sizeof m->pending) return;   // drop: the real bus has no queue either
    memcpy(m->pending + m->pending_len, b, n);
    m->pending_len += n;
    if (!m->pending_armed || due_ms > m->pending_due_ms) m->pending_due_ms = due_ms;
    m->pending_armed = true;
}

static void answer_poll(vlx_machine_t *m, const vlx_frame_t *f)
{
    uint8_t reg = f->value;                 // a poll carries the wanted register in value
    if (!m->known[reg]) return;             // unknown: silence, like the documents say nothing
    if (m->reply_delay_ms == VLX_MACHINE_NEVER) return;
    uint8_t out[VLX_FRAME_LEN];
    vlx_make_write(ME, f->sender, reg, m->regs[reg], out);   // an answer has the same shape as a write
    queue_bytes(m, out, VLX_FRAME_LEN, m->last_tick_ms + m->reply_delay_ms);
}

static void on_frame(const vlx_frame_t *f, void *ctx)
{
    vlx_machine_t *m = (vlx_machine_t *)ctx;
    if (f->sender == ME) return;                                   // our own echo
    if (f->receiver != ME && f->receiver != VLX_ADDR_MAINBOARDS) return;
    if (vlx_frame_is_poll(f)) { answer_poll(m, f); return; }
    // writes: Task 3
}

void vlx_machine_init(vlx_machine_t *m)
{
    memset(m, 0, sizeof *m);
    for (size_t i = 0; i < sizeof k_regs / sizeof k_regs[0]; i++) {
        const vlx_machine_reg_t *r = &k_regs[i];
        m->known[r->reg] = true; m->writable[r->reg] = r->writable;
        m->regs[r->reg] = r->def; m->conf[r->reg] = r->conf;
    }
    for (size_t i = 0; i < sizeof k_temp_regs / sizeof k_temp_regs[0]; i++) {
        const temp_reg_def_t *r = &k_temp_regs[i];
        m->known[r->reg] = true; m->writable[r->reg] = r->writable;
        m->regs[r->reg] = vlx_temp_to_raw(r->def_c); m->conf[r->reg] = r->conf;
    }
    m->reply_delay_ms = 0;
    m->broadcast_period_ms = 12000;
    m->broadcast_spacing_ms = 130;
    m->p.t_outdoor = 5.0f; m->p.t_indoor = 21.0f; m->p.efficiency = 0.6f;
    m->p.tau_s = 600.0f; m->p.time_scale = 1.0f;
    m->t_extract = 21.0f; m->t_supply = 17.0f; m->t_exhaust = 9.0f;
    vlx_parser_init(&m->parser, on_frame, m);
}

void vlx_machine_feed(vlx_machine_t *m, const uint8_t *bytes, size_t n)
{
    vlx_parser_feed_buffer(&m->parser, bytes, n);
}

size_t vlx_machine_tick(vlx_machine_t *m, uint32_t now_ms, uint8_t *out, size_t max)
{
    m->last_tick_ms = now_ms;
    m->have_tick = true;
    size_t n = 0;
    if (m->pending_armed && (int32_t)(now_ms - m->pending_due_ms) >= 0 && m->pending_len <= max) {
        memcpy(out, m->pending, m->pending_len);
        n = m->pending_len;
        m->pending_len = 0;
        m->pending_armed = false;
    }
    return n;
}

bool    vlx_machine_reg_known(const vlx_machine_t *m, uint8_t reg) { return m->known[reg]; }
uint8_t vlx_machine_reg_get(const vlx_machine_t *m, uint8_t reg)   { return m->regs[reg]; }
void    vlx_machine_reg_set(vlx_machine_t *m, uint8_t reg, uint8_t value)
{
    m->regs[reg] = value;
    m->known[reg] = true;
}

void vlx_machine_fault(vlx_machine_t *m, vlx_fault_t code) { (void)m; (void)code; }   // Task 6
void vlx_machine_fault_clear(vlx_machine_t *m) { (void)m; }                           // Task 6
```

`firmware/components/vallox_machine/vallox_machine_physics.c` (stub until Task 5):

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include "vallox_machine.h"

void vlx_machine_physics_step(vlx_machine_t *m, float dt_s) { (void)m; (void)dt_s; }
```

`firmware/components/vallox_machine/CMakeLists.txt`:

```cmake
idf_component_register(SRCS "vallox_machine.c" "vallox_machine_physics.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "."
                       REQUIRES vallox_protocol)
```

Note on `answer_poll`: `last_tick_ms` is the time of the most recent tick; the test helper feeds then ticks at the same `now`, so a 0 ms delay answers in that tick. That ordering (feed, then tick) is the contract for every host.

- [ ] **Step 6: Run the machine tests**

Run: `cd firmware/test/host && make run_tests_machine && ./run_tests_machine`
Expected: `... checks, 0 failures`.

If `test_defaults_are_a_plausible_machine` fails on the temperatures, the NTC table does not have an exact raw for 5/17/18 °C — `vlx_temp_to_raw` returns the lowest raw that *reaches* the degree, so `vlx_temp_table(raw)` equals the degree; if it does not, fix the test expectation to `vlx_temp_table(vlx_temp_to_raw(5))` rather than hard-coding.

- [ ] **Step 7: Commit**

```bash
git add firmware/components/vallox_machine firmware/test/host/test_vallox_machine.c
git commit -m "feat(machine): emulator skeleton — register table from protocol.md, answers polls

A bus device at 0x11 with the same parser as the panel. Every register
carries the confidence class of the claim it comes from, and a register
the documents do not describe is simply not answered: the UI will see a
timeout, never an invented value."
```

---

### Task 3: Writes, the acknowledge byte, and what is not writable

**Files:**
- Modify: `firmware/components/vallox_machine/vallox_machine.c` (`on_frame`, new `handle_write`)
- Modify: `firmware/test/host/test_vallox_machine.c`

**Interfaces:** unchanged public API. Behaviour (protocol.md claim 25, `manufacturer`): a write addressed to the machine updates the register and the machine answers with **one byte: the checksum of the frame it received**. A write to a register that is not writable, or unknown, gets no acknowledge and changes nothing. Writes to the mainboard broadcast address 0x10 are applied and acknowledged the same way (the real machine's behaviour on broadcast writes is `assumed`; note it in the README).

- [ ] **Step 1: Add failing tests**

Append to `test_vallox_machine.c` (before `main`) and call them from `main`:

```c
static void test_write_updates_register_and_is_acknowledged_with_checksum(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t w[VLX_FRAME_LEN], out[32];
    vlx_make_write(PANEL, MACHINE, VLX_REG_HEAT_SETPOINT, vlx_temp_to_raw(20), w);
    size_t n = send_and_tick(&m, w, 0, out, sizeof out);
    CHECK_EQ(n, 1);
    CHECK_EQ(out[0], w[5]);                    // the acknowledge is the received checksum byte
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_HEAT_SETPOINT), vlx_temp_to_raw(20));
}

static void test_write_to_read_only_register_is_ignored_silently(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t before = vlx_machine_reg_get(&m, VLX_REG_TEMP_OUTDOOR);
    uint8_t w[VLX_FRAME_LEN], out[32];
    vlx_make_write(PANEL, MACHINE, VLX_REG_TEMP_OUTDOOR, 0x80, w);
    CHECK_EQ(send_and_tick(&m, w, 0, out, sizeof out), 0);
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_TEMP_OUTDOOR), before);
}

static void test_write_to_unknown_register_is_ignored_silently(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t w[VLX_FRAME_LEN], out[32];
    vlx_make_write(PANEL, MACHINE, 0xC1, 0x01, w);
    CHECK_EQ(send_and_tick(&m, w, 0, out, sizeof out), 0);
    CHECK(!vlx_machine_reg_known(&m, 0xC1));
}

static void test_write_then_poll_reads_back(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t w[VLX_FRAME_LEN], p[VLX_FRAME_LEN], out[32];
    vlx_make_write(PANEL, MACHINE, VLX_REG_FAN_SPEED_DEFAULT, vlx_fan_speed_to_raw(5), w);
    send_and_tick(&m, w, 0, out, sizeof out);
    vlx_make_poll(PANEL, MACHINE, VLX_REG_FAN_SPEED_DEFAULT, p);
    CHECK_EQ(send_and_tick(&m, p, 1, out, sizeof out), VLX_FRAME_LEN);
    vlx_frame_t f;
    CHECK(vlx_frame_decode(out, &f));
    CHECK_EQ(vlx_fan_speed_from_raw(f.value), 5);
}
```

- [ ] **Step 2: Run to verify they fail**

Run: `make run_tests_machine && ./run_tests_machine`
Expected: the four new tests report FAIL lines (ack count 0, etc.).

- [ ] **Step 3: Implement writes**

In `vallox_machine.c`, add above `on_frame`:

```c
static void handle_write(vlx_machine_t *m, const vlx_frame_t *f)
{
    if (!m->known[f->reg] || !m->writable[f->reg]) return;   // silently: nothing documents a NAK
    m->regs[f->reg] = f->value;
    // Acknowledge = the checksum byte of the frame we received (protocol.md claim 25).
    uint8_t raw[VLX_FRAME_LEN];
    vlx_frame_t copy = *f;
    vlx_frame_encode(&copy, raw);
    queue_bytes(m, &raw[5], 1, m->last_tick_ms + m->reply_delay_ms);
}
```

and replace the `// writes: Task 3` line in `on_frame` with `handle_write(m, f);`.

- [ ] **Step 4: Run the tests**

Run: `make run_tests_machine && ./run_tests_machine`
Expected: 0 failures.

- [ ] **Step 5: Commit**

```bash
git add firmware/components/vallox_machine/vallox_machine.c firmware/test/host/test_vallox_machine.c
git commit -m "feat(machine): writes update the register and return the checksum byte as acknowledge

Read-only and unknown registers are ignored without a reply, because no
document describes a negative acknowledge and inventing one would teach
the panel something false."
```

---

### Task 4: Reply delay and the 12 s broadcast round

**Files:**
- Modify: `firmware/components/vallox_machine/vallox_machine.c` (`vlx_machine_tick`, new `run_broadcasts`)
- Modify: `firmware/test/host/test_vallox_machine.c`

**Interfaces:** unchanged. Behaviour (protocol.md claim 23): every `broadcast_period_ms` (12 000) the machine sends registers `0x2B 0x2C 0x35 0x34 0x32 0x33` to `VLX_ADDR_PANELS` (0x20), one frame every `broadcast_spacing_ms` (130), then `0x2A`. `reply_delay_ms` delays poll answers and acknowledges; `VLX_MACHINE_NEVER` suppresses both. First broadcast round starts one period after the first tick (the start-up burst is not documented for the mainboard and is not modelled).

- [ ] **Step 1: Add failing tests**

```c
static int count_frames_to_panels(const uint8_t *buf, size_t n, uint8_t *regs_out, int max)
{
    // the output of a round is back-to-back frames; decode each six bytes
    int k = 0;
    for (size_t i = 0; i + VLX_FRAME_LEN <= n; i += VLX_FRAME_LEN) {
        vlx_frame_t f;
        if (vlx_frame_decode(buf + i, &f) && f.receiver == VLX_ADDR_PANELS && k < max) regs_out[k++] = f.reg;
    }
    return k;
}

static void test_reply_delay_holds_the_answer_until_due(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    m.reply_delay_ms = 10;
    uint8_t poll[VLX_FRAME_LEN], out[32];
    vlx_make_poll(PANEL, MACHINE, VLX_REG_STATUS, poll);
    vlx_machine_feed(&m, poll, VLX_FRAME_LEN);
    CHECK_EQ(vlx_machine_tick(&m, 100, out, sizeof out), 0);    // queued at 100, due 110
    CHECK_EQ(vlx_machine_tick(&m, 105, out, sizeof out), 0);
    CHECK_EQ(vlx_machine_tick(&m, 110, out, sizeof out), VLX_FRAME_LEN);
}

static void test_reply_never_means_silence(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    m.reply_delay_ms = VLX_MACHINE_NEVER;
    uint8_t poll[VLX_FRAME_LEN], out[32];
    vlx_make_poll(PANEL, MACHINE, VLX_REG_STATUS, poll);
    vlx_machine_feed(&m, poll, VLX_FRAME_LEN);
    CHECK_EQ(vlx_machine_tick(&m, 0, out, sizeof out), 0);
    CHECK_EQ(vlx_machine_tick(&m, 5000, out, sizeof out), 0);
}

static void test_broadcast_round_every_12_s_in_documented_order(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t out[64], regs[16];
    size_t total = 0; uint8_t all[256];
    // walk 13 s in 10 ms ticks, collect everything sent
    for (uint32_t t = 0; t <= 13000; t += 10) {
        size_t n = vlx_machine_tick(&m, t, out, sizeof out);
        if (n && total + n <= sizeof all) { memcpy(all + total, out, n); total += n; }
    }
    int k = count_frames_to_panels(all, total, regs, 16);
    CHECK_EQ(k, 7);
    const uint8_t expect[7] = {0x2B, 0x2C, 0x35, 0x34, 0x32, 0x33, 0x2A};
    CHECK(k == 7 && memcmp(regs, expect, 7) == 0);
}

static void test_broadcast_frames_are_spaced_130_ms(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    uint8_t out[64];
    uint32_t first = 0, second = 0;
    for (uint32_t t = 0; t <= 13000; t += 10) {
        if (vlx_machine_tick(&m, t, out, sizeof out)) {
            if (!first) first = t; else if (!second) second = t;
        }
    }
    CHECK_EQ(first, 12000);
    CHECK_EQ(second - first, 130);
}
```

- [ ] **Step 2: Run to verify they fail** — `make run_tests_machine && ./run_tests_machine`: the broadcast tests report 0 frames.

- [ ] **Step 3: Implement the broadcast scheduler**

In `vallox_machine.c` add:

```c
static const uint8_t k_broadcast_set[] = {0x2B, 0x2C, 0x35, 0x34, 0x32, 0x33, 0x2A};
#define BROADCAST_N (sizeof k_broadcast_set / sizeof k_broadcast_set[0])

// One broadcast round: BROADCAST_N frames to 0x20, broadcast_spacing_ms apart,
// starting broadcast_period_ms after the previous round started.
static size_t run_broadcasts(vlx_machine_t *m, uint32_t now_ms, uint8_t *out, size_t max)
{
    if (m->broadcast_idx == 0) {
        if (m->last_broadcast_ms == 0 && !m->have_tick) m->last_broadcast_ms = now_ms;  // first tick anchors the period
        if ((int32_t)(now_ms - (m->last_broadcast_ms + m->broadcast_period_ms)) < 0) return 0;
        m->last_broadcast_ms = now_ms;
        m->next_broadcast_frame_ms = now_ms;
    }
    if ((int32_t)(now_ms - m->next_broadcast_frame_ms) < 0 || max < VLX_FRAME_LEN) return 0;
    uint8_t reg = k_broadcast_set[m->broadcast_idx];
    vlx_make_write(ME, VLX_ADDR_PANELS, reg, m->known[reg] ? m->regs[reg] : 0, out);
    m->broadcast_idx++;
    m->next_broadcast_frame_ms = now_ms + m->broadcast_spacing_ms;
    if (m->broadcast_idx >= BROADCAST_N) m->broadcast_idx = 0;
    return VLX_FRAME_LEN;
}
```

and change `vlx_machine_tick` to:

```c
size_t vlx_machine_tick(vlx_machine_t *m, uint32_t now_ms, uint8_t *out, size_t max)
{
    size_t n = 0;
    // first tick: anchor the broadcast clock before have_tick flips
    n += run_broadcasts(m, now_ms, out + n, max - n);
    m->last_tick_ms = now_ms;
    m->have_tick = true;
    if (m->pending_armed && (int32_t)(now_ms - m->pending_due_ms) >= 0 && m->pending_len <= max - n) {
        memcpy(out + n, m->pending, m->pending_len);
        n += m->pending_len;
        m->pending_len = 0;
        m->pending_armed = false;
    }
    return n;
}
```

(The anchoring line in `run_broadcasts` relies on `have_tick` being false on the very first call, which is why `run_broadcasts` runs before `have_tick = true`.)

- [ ] **Step 4: Run the tests** — expected 0 failures. If `test_broadcast_round_every_12_s_in_documented_order` sees 14 frames, the 10 ms loop reached the second round; it does not (13 000 < 24 000), so 14 means the period anchoring is wrong — check that the first round starts at 12 000, not 0.

- [ ] **Step 5: Commit**

```bash
git add firmware/components/vallox_machine/vallox_machine.c firmware/test/host/test_vallox_machine.c
git commit -m "feat(machine): poll-answer delay and the 12 s broadcast round

The delay is a test knob for the panel's timeout path (0 / 10 / 200 ms /
never). The broadcast set, period and spacing are the ones in
protocol.md claim 23; the mainboard's start-up burst is undocumented and
not modelled."
```

---

### Task 5: Coarse thermal model

**Files:**
- Modify: `firmware/components/vallox_machine/vallox_machine_physics.c` (replace the stub)
- Modify: `firmware/components/vallox_machine/vallox_machine.c` (`vlx_machine_tick` calls the step)
- Modify: `firmware/test/host/test_vallox_machine.c`

**Interfaces:** `void vlx_machine_physics_step(vlx_machine_t *m, float dt_s)` — advances `t_extract/t_supply/t_exhaust` and writes the NTC-rounded copies into `regs[0x34/0x35/0x33]`, `regs[0x32]` from `p.t_outdoor`; sets/clears the heater bit in `VLX_REG_STATUS` (bit `VLX_STATUS_HEATING`... use the bit names in `vallox_protocol.h`; if the header names the heater indicator differently, use that name — read the header's "Bits of VLX_REG_STATUS" block) and the supply-fan-stop register on frost; advances the service month counter.

Model (spec §3.3):
- `speed = vlx_fan_speed_from_raw(regs[0x29])`, treat invalid as 1.
- `tau = p.tau_s / speed`; `alpha = 1 - exp(-dt*time_scale/tau)` (use a first-order Euler `alpha = min(1, dt*time_scale/tau)` to avoid `<math.h>`).
- `t_extract += alpha * (p.t_indoor - t_extract)`.
- `t_recovered = p.t_outdoor + p.efficiency * (t_extract - p.t_outdoor)`.
- heater setpoint `sp = vlx_temp_table(regs[VLX_REG_HEAT_SETPOINT])`; `heater_on = t_recovered < sp`; `target_supply = heater_on ? sp : t_recovered`; `t_supply += alpha * (target_supply - t_supply)`.
- `t_exhaust_target = t_extract - p.efficiency * (t_extract - p.t_outdoor)`; `t_exhaust += alpha * (t_exhaust_target - t_exhaust)`.
- frost: if `t_exhaust < -2.0f` set `regs[VLX_REG_SUPPLY_FAN_STOP] = 1` and keep it until `t_exhaust > -2.0f + hysteresis` where `hysteresis = vlx_temp_table(regs[VLX_REG_DEFROST_HYSTERESIS])` interpreted as degrees (the encoding of 0xB2 is `implementations`; note in README).
- service: `service_elapsed_ms += dt*time_scale*1000`; every 30 days (2 592 000 000 ms) decrement `regs[VLX_REG_SERVICE_MONTHS_LEFT]` to a floor of 0.

- [ ] **Step 1: Add failing tests**

```c
static void run_physics(vlx_machine_t *m, float seconds, float dt)
{
    for (float t = 0; t < seconds; t += dt) vlx_machine_physics_step(m, dt);
}

static void test_cold_outdoor_turns_heater_on_and_supply_reaches_setpoint(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    m.p.t_outdoor = -20.0f;
    run_physics(&m, 3600.0f, 1.0f);
    int16_t supply = vlx_temp_table(vlx_machine_reg_get(&m, VLX_REG_TEMP_SUPPLY));
    int16_t sp = vlx_temp_table(vlx_machine_reg_get(&m, VLX_REG_HEAT_SETPOINT));
    CHECK(supply >= sp - 1 && supply <= sp + 1);
    CHECK(vlx_machine_reg_get(&m, VLX_REG_STATUS) & VLX_STATUS_HEATING);   // name from vallox_protocol.h
}

static void test_mild_outdoor_keeps_heater_off_and_supply_below_setpoint(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    m.p.t_outdoor = 15.0f;
    run_physics(&m, 3600.0f, 1.0f);
    CHECK(!(vlx_machine_reg_get(&m, VLX_REG_STATUS) & VLX_STATUS_HEATING));
    int16_t supply = vlx_temp_table(vlx_machine_reg_get(&m, VLX_REG_TEMP_SUPPLY));
    // recovered = 15 + 0.6*(21-15) = 18.6 ≈ setpoint 18 → heater off, supply ~18-19
    CHECK(supply >= 18 && supply <= 19);
}

static void test_higher_fan_speed_settles_faster(void)
{
    vlx_machine_t a, b;
    vlx_machine_init(&a); vlx_machine_init(&b);
    a.p.t_outdoor = b.p.t_outdoor = -20.0f;
    vlx_machine_reg_set(&a, VLX_REG_FAN_SPEED, vlx_fan_speed_to_raw(1));
    vlx_machine_reg_set(&b, VLX_REG_FAN_SPEED, vlx_fan_speed_to_raw(8));
    run_physics(&a, 300.0f, 1.0f);
    run_physics(&b, 300.0f, 1.0f);
    // b moved further toward its target in the same time
    CHECK(b.t_exhaust < a.t_exhaust);
}

static void test_frost_protection_stops_supply_fan_and_releases_with_hysteresis(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    m.p.t_outdoor = -30.0f;
    run_physics(&m, 7200.0f, 1.0f);
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_SUPPLY_FAN_STOP), 1);
    m.p.t_outdoor = 10.0f;
    run_physics(&m, 7200.0f, 1.0f);
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_SUPPLY_FAN_STOP), 0);
}

static void test_time_scale_speeds_everything_up(void)
{
    vlx_machine_t a, b;
    vlx_machine_init(&a); vlx_machine_init(&b);
    a.p.t_outdoor = b.p.t_outdoor = -20.0f;
    b.p.time_scale = 60.0f;
    run_physics(&a, 60.0f, 1.0f);
    run_physics(&b, 60.0f, 1.0f);
    CHECK(b.t_exhaust < a.t_exhaust);
}

static void test_tick_runs_physics_and_updates_registers(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    m.p.t_outdoor = -20.0f;
    m.p.time_scale = 60.0f;
    uint8_t out[64];
    uint8_t before = vlx_machine_reg_get(&m, VLX_REG_TEMP_EXHAUST);
    for (uint32_t t = 0; t <= 60000; t += 20) vlx_machine_tick(&m, t, out, sizeof out);
    CHECK(vlx_machine_reg_get(&m, VLX_REG_TEMP_EXHAUST) != before);
    CHECK_EQ(vlx_temp_table(vlx_machine_reg_get(&m, VLX_REG_TEMP_OUTDOOR)), -20);
}
```

- [ ] **Step 2: Run to verify they fail** — `make run_tests_machine && ./run_tests_machine`.

- [ ] **Step 3: Implement the physics**

`firmware/components/vallox_machine/vallox_machine_physics.c`:

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Coarse thermal model: enough for the readings to move the right way when
// the fan speed, the outdoor temperature or the heater setpoint change. First
// order lags, no heat capacity of the house, no defrost cycle — see README.
#include "vallox_machine.h"

#define FROST_LIMIT_C (-2.0f)
#define MONTH_MS (30u * 24u * 3600u * 1000u)

static float lag(float x, float target, float alpha) { return x + alpha * (target - x); }

void vlx_machine_physics_step(vlx_machine_t *m, float dt_s)
{
    int speed = vlx_fan_speed_from_raw(m->regs[VLX_REG_FAN_SPEED]);
    if (speed == VLX_FAN_SPEED_INVALID) speed = 1;
    float tau = m->p.tau_s / (float)speed;
    float alpha = dt_s * m->p.time_scale / tau;
    if (alpha > 1.0f) alpha = 1.0f;

    m->t_extract = lag(m->t_extract, m->p.t_indoor, alpha);
    float recovered = m->p.t_outdoor + m->p.efficiency * (m->t_extract - m->p.t_outdoor);
    float sp = (float)vlx_temp_table(m->regs[VLX_REG_HEAT_SETPOINT]);
    bool heater_on = recovered < sp;
    m->t_supply = lag(m->t_supply, heater_on ? sp : recovered, alpha);
    float exhaust_target = m->t_extract - m->p.efficiency * (m->t_extract - m->p.t_outdoor);
    m->t_exhaust = lag(m->t_exhaust, exhaust_target, alpha);

    // frost protection with hysteresis (0xB2 is in degrees: implementations-class claim)
    float hyst = (float)vlx_temp_table(m->regs[VLX_REG_DEFROST_HYSTERESIS]);
    if (hyst < 1.0f) hyst = 1.0f;
    if (m->t_exhaust < FROST_LIMIT_C) m->regs[VLX_REG_SUPPLY_FAN_STOP] = 1;
    else if (m->t_exhaust > FROST_LIMIT_C + hyst) m->regs[VLX_REG_SUPPLY_FAN_STOP] = 0;

    // service counter
    m->service_elapsed_ms += (uint32_t)(dt_s * m->p.time_scale * 1000.0f);
    while (m->service_elapsed_ms >= MONTH_MS) {
        m->service_elapsed_ms -= MONTH_MS;
        if (m->regs[VLX_REG_SERVICE_MONTHS_LEFT] > 0) m->regs[VLX_REG_SERVICE_MONTHS_LEFT]--;
    }

    // publish through the NTC table so the panel decodes real raw bytes
    m->regs[VLX_REG_TEMP_OUTDOOR] = vlx_temp_to_raw((int16_t)(m->p.t_outdoor + (m->p.t_outdoor >= 0 ? 0.5f : -0.5f)));
    m->regs[VLX_REG_TEMP_EXTRACT] = vlx_temp_to_raw((int16_t)(m->t_extract + 0.5f));
    m->regs[VLX_REG_TEMP_SUPPLY]  = vlx_temp_to_raw((int16_t)(m->t_supply  + (m->t_supply  >= 0 ? 0.5f : -0.5f)));
    m->regs[VLX_REG_TEMP_EXHAUST] = vlx_temp_to_raw((int16_t)(m->t_exhaust + (m->t_exhaust >= 0 ? 0.5f : -0.5f)));
    if (heater_on) m->regs[VLX_REG_STATUS] |= VLX_STATUS_HEATING;
    else           m->regs[VLX_REG_STATUS] &= (uint8_t)~VLX_STATUS_HEATING;
}
```

`VLX_STATUS_HEATING` must be the header's name for the heating indicator bit of 0xA3 (the block "Bits of VLX_REG_STATUS (0xA3). Bits 0..3 are settable, 4..7 are indicators" in `vallox_protocol.h`). Open the header, find the indicator bit for heating, and use its exact macro; if it is named e.g. `VLX_STATUS_HEATING_ON`, use that name in both the test and the code.

In `vlx_machine_tick`, after `m->last_tick_ms = now_ms;` insert:

```c
    if (m->have_tick) {
        uint32_t dt_ms = now_ms - prev_ms;
        if (dt_ms > 0 && dt_ms < 10000) vlx_machine_physics_step(m, (float)dt_ms / 1000.0f);
    }
```

where `prev_ms` is the value of `m->last_tick_ms` saved at the top of the function before it is overwritten (add `uint32_t prev_ms = m->last_tick_ms;` as the first line of `vlx_machine_tick`). Ticks more than 10 s apart are skipped (a paused browser tab must not fast-forward the house).

- [ ] **Step 4: Run the tests** — expected 0 failures.

- [ ] **Step 5: Commit**

```bash
git add firmware/components/vallox_machine/vallox_machine_physics.c firmware/components/vallox_machine/vallox_machine.c firmware/test/host/test_vallox_machine.c
git commit -m "feat(machine): coarse thermal model — readings follow fan speed, outdoor temperature and the heater setpoint

First-order lags, heat-recovery efficiency as a parameter, frost
protection with hysteresis, a service month counter. Results go through
the NTC table so the panel decodes real raw bytes. The numbers are
parameters, not claims about any machine."
```

---

### Task 6: Fault injection

**Files:**
- Modify: `firmware/components/vallox_machine/vallox_machine.c` (`vlx_machine_fault`, `vlx_machine_fault_clear`, `handle_write` clearing rule)
- Modify: `firmware/test/host/test_vallox_machine.c`

**Interfaces:** `vlx_machine_fault(m, code)` sets `regs[VLX_REG_FAULT] = code` and the fault indicator bit of `VLX_REG_STATUS` (header name, same rule as Task 5 — the block lists an indicator bit for fault; use its exact macro, called `VLX_STATUS_FAULT` below). `vlx_machine_fault_clear` zeroes both. A panel write of 0 to `VLX_REG_FAULT` also clears (ASSUMED; `VLX_REG_FAULT` stays `writable = false` in the table — this is the one exception handled explicitly in `handle_write` and it is acknowledged).

- [ ] **Step 1: Add failing tests**

```c
static void test_fault_sets_register_and_status_bit(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    vlx_machine_fault(&m, VLX_FAULT_SUPPLY_SENSOR);   // pick any member of vlx_fault_t from the header
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_FAULT), (uint8_t)VLX_FAULT_SUPPLY_SENSOR);
    CHECK(vlx_machine_reg_get(&m, VLX_REG_STATUS) & VLX_STATUS_FAULT);
    vlx_machine_fault_clear(&m);
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_FAULT), 0);
    CHECK(!(vlx_machine_reg_get(&m, VLX_REG_STATUS) & VLX_STATUS_FAULT));
}

static void test_panel_can_clear_fault_by_writing_zero_assumed(void)
{
    vlx_machine_t m;
    vlx_machine_init(&m);
    vlx_machine_fault(&m, VLX_FAULT_SUPPLY_SENSOR);
    uint8_t w[VLX_FRAME_LEN], out[32];
    vlx_make_write(PANEL, MACHINE, VLX_REG_FAULT, 0, w);
    CHECK_EQ(send_and_tick(&m, w, 0, out, sizeof out), 1);     // acknowledged
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_FAULT), 0);
    // but a non-zero write to the fault register is still refused
    vlx_make_write(PANEL, MACHINE, VLX_REG_FAULT, 5, w);
    CHECK_EQ(send_and_tick(&m, w, 1, out, sizeof out), 0);
    CHECK_EQ(vlx_machine_reg_get(&m, VLX_REG_FAULT), 0);
}
```

Replace `VLX_FAULT_SUPPLY_SENSOR` with an actual enumerator from the `vlx_fault_t` enum in `vallox_protocol.h` (open the header; any non-zero member is fine — use the same one in both tests).

- [ ] **Step 2: Run to verify they fail.**

- [ ] **Step 3: Implement**

```c
void vlx_machine_fault(vlx_machine_t *m, vlx_fault_t code)
{
    m->regs[VLX_REG_FAULT] = (uint8_t)code;
    m->regs[VLX_REG_STATUS] |= VLX_STATUS_FAULT;
}

void vlx_machine_fault_clear(vlx_machine_t *m)
{
    m->regs[VLX_REG_FAULT] = 0;
    m->regs[VLX_REG_STATUS] &= (uint8_t)~VLX_STATUS_FAULT;
}
```

and at the top of `handle_write`, before the writable check:

```c
    if (f->reg == VLX_REG_FAULT) {
        if (f->value != 0) return;            // ASSUMED: only clearing is meaningful
        vlx_machine_fault_clear(m);
        uint8_t raw[VLX_FRAME_LEN]; vlx_frame_t copy = *f; vlx_frame_encode(&copy, raw);
        queue_bytes(m, &raw[5], 1, m->last_tick_ms + m->reply_delay_ms);
        return;
    }
```

- [ ] **Step 4: Run the tests** — 0 failures.

- [ ] **Step 5: Commit**

```bash
git add firmware/components/vallox_machine/vallox_machine.c firmware/test/host/test_vallox_machine.c
git commit -m "feat(machine): fault injection, cleared by the host or by the panel writing zero (assumed)"
```

---

### Task 7: End-to-end over the memory bus, README, CI

**Files:**
- Create: `firmware/test/host/test_e2e_membus.c`
- Create: `firmware/components/vallox_machine/README.md`
- Modify: `.github/workflows/firmware.yml:18-21`
- Modify: `firmware/test/host/Makefile` (already lists `run_tests_e2e`; nothing to change unless a source path differs)

**Interfaces:** consumes `panel_hal.h` (`hal_bus_write/read`, `hal_time_ms`), `hal_host.h` (`membus_*`, `hal_host_advance_ms`), `vallox_machine.h`, `vallox_protocol.h`. The "panel stub" in this test is the shape `vlx_client` will take in S2: poll → wait for the answer with a deadline → decode.

- [ ] **Step 1: Write the e2e test**

`firmware/test/host/test_e2e_membus.c`:

```c
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// A panel stub and the machine emulator, talking in real frames through the
// host memory bus. This is the shape of every later host test: the panel side
// only sees panel_hal.h, the machine side only sees the membus.
#include <string.h>
#include "check.h"
#include "panel_hal.h"
#include "hal_host.h"
#include "vallox_machine.h"
#include "vallox_protocol.h"

#define PANEL VLX_ADDR_PANEL_DEFAULT
#define MACHINE VLX_ADDR_MAINBOARD_1

static vlx_machine_t s_m;

// One scheduler step: move panel->machine bytes, tick the machine, move machine->panel bytes.
static void pump(uint32_t step_ms)
{
    uint8_t buf[64];
    size_t n = membus_machine_read(buf, sizeof buf);
    if (n) vlx_machine_feed(&s_m, buf, n);
    hal_host_advance_ms(step_ms);
    n = vlx_machine_tick(&s_m, hal_time_ms(), buf, sizeof buf);
    if (n) membus_machine_write(buf, n);
}

// Panel stub: poll a register, wait up to deadline_ms for the answer frame.
static bool panel_read(uint8_t reg, uint8_t *value, uint32_t deadline_ms)
{
    uint8_t f[VLX_FRAME_LEN];
    vlx_make_poll(PANEL, MACHINE, reg, f);
    hal_bus_write(f, VLX_FRAME_LEN);
    vlx_parser_t p; vlx_frame_t got; bool have = false;
    // capture callback through a small static: the parser API takes a callback
    struct cap { vlx_frame_t fr; bool ok; } c = {{0}, false};
    void on(const vlx_frame_t *fr, void *ctx) { struct cap *cc = ctx; cc->fr = *fr; cc->ok = true; }
    vlx_parser_init(&p, on, &c);
    uint32_t t0 = hal_time_ms();
    while (hal_time_ms() - t0 < deadline_ms) {
        pump(1);
        uint8_t b[16]; size_t n = hal_bus_read(b, sizeof b);
        if (n) vlx_parser_feed_buffer(&p, b, n);
        if (c.ok && c.fr.sender == MACHINE && c.fr.receiver == PANEL && c.fr.reg == reg) { have = true; break; }
    }
    (void)got;
    if (have) *value = c.fr.value;
    return have;
}

// Panel stub: write, wait for the one-byte acknowledge.
static bool panel_write(uint8_t reg, uint8_t value, uint32_t deadline_ms)
{
    uint8_t f[VLX_FRAME_LEN];
    vlx_make_write(PANEL, MACHINE, reg, value, f);
    hal_bus_write(f, VLX_FRAME_LEN);
    uint32_t t0 = hal_time_ms();
    while (hal_time_ms() - t0 < deadline_ms) {
        pump(1);
        uint8_t b; if (hal_bus_read(&b, 1) == 1) return b == f[5];
    }
    return false;
}

static void test_panel_reads_fan_speed_through_the_bus(void)
{
    hal_host_reset(); vlx_machine_init(&s_m);
    uint8_t v = 0;
    CHECK(panel_read(VLX_REG_FAN_SPEED, &v, 50));
    CHECK_EQ(vlx_fan_speed_from_raw(v), 3);
}

static void test_panel_sets_speed_and_reads_it_back(void)
{
    hal_host_reset(); vlx_machine_init(&s_m);
    // 0x29 is read-only on the real machine (speed is set through 0xA9 default / status); the
    // machine table marks 0x29 read-only, so set it through the writable default-speed register
    CHECK(panel_write(VLX_REG_FAN_SPEED_DEFAULT, vlx_fan_speed_to_raw(5), 50));
    uint8_t v = 0;
    CHECK(panel_read(VLX_REG_FAN_SPEED_DEFAULT, &v, 50));
    CHECK_EQ(vlx_fan_speed_from_raw(v), 5);
}

static void test_panel_times_out_when_machine_never_answers(void)
{
    hal_host_reset(); vlx_machine_init(&s_m);
    s_m.reply_delay_ms = VLX_MACHINE_NEVER;
    uint8_t v = 0;
    CHECK(!panel_read(VLX_REG_FAN_SPEED, &v, 10));
}

static void test_panel_sees_broadcasts_without_asking(void)
{
    hal_host_reset(); vlx_machine_init(&s_m);
    vlx_parser_t p; struct { int n; } c = {0};
    void on(const vlx_frame_t *fr, void *ctx) { if (fr->receiver == VLX_ADDR_PANELS) ((__typeof__(c) *)ctx)->n++; }
    vlx_parser_init(&p, on, &c);
    for (int i = 0; i < 13000; i++) {
        pump(1);
        uint8_t b[16]; size_t n = hal_bus_read(b, sizeof b);
        if (n) vlx_parser_feed_buffer(&p, b, n);
    }
    CHECK_EQ(c.n, 7);
}

int main(void)
{
    test_panel_reads_fan_speed_through_the_bus();
    test_panel_sets_speed_and_reads_it_back();
    test_panel_times_out_when_machine_never_answers();
    test_panel_sees_broadcasts_without_asking();
    return REPORT();
}
```

Note: the nested functions (`void on(...)` inside a function) are a GCC/Clang extension that `-std=c11 -Werror` rejects on some compilers. If the build fails on them, hoist each callback to file scope with a `static` capture struct — behaviour is identical; the plan keeps them inline only for readability.

- [ ] **Step 2: Build and run** — `make run_tests_e2e && ./run_tests_e2e`: 0 failures. If nested functions are rejected, apply the note above first.

- [ ] **Step 3: Write the component README**

`firmware/components/vallox_machine/README.md`:

```markdown
# vallox_machine — a simulated Vallox mainboard

An emulator of the machine side of the RS-485 bus, used by the host tests and
by the browser simulator. It is itself a bus device at 0x11 and speaks only in
the six-byte frames of `vallox_protocol`. **It is exactly as right as
`docs/research/protocol.md`** — nothing here has been checked against a
machine. When the M3 capture disagrees, the protocol document is corrected
first, then this model and the panel in the same PR.

## What it does

| Behaviour | Source | Confidence |
|---|---|---|
| Answers a poll for a known register with its value, to the poller | protocol.md claims 11, 24 | manufacturer |
| Answers a write with one byte, the received checksum | claim 25 | manufacturer |
| Ignores writes to read-only or unknown registers, without a reply | no NAK is documented | assumed (silence) |
| Broadcasts `2B 2C 35 34 32 33 2A` to 0x20 every 12 s, 130 ms apart | claim 23 | manufacturer (period) / implementations (set) |
| Register map and defaults | `vallox_machine_regs.h`, each row with its class | per row |
| Thermal model: lags toward indoor / recovered / setpoint temperatures | own model, parameters | n/a — a model, not a claim |
| Frost protection: supply fan stop below −2 °C exhaust, hysteresis from 0xB2 | threshold own; 0xB2 encoding | implementations |
| Fault injection; cleared by host or by the panel writing 0 to 0x36 | clearing write | **assumed** |
| Poll-answer delay 0 / 10 / 200 ms / never | test knob | n/a |

## What it does not do (yet)

- No CO₂ / RH sensors (`0x2D` reports none fitted), no SUSPEND/RESUME exchange.
- No LON module, no programme-2 week clock, no start-up burst from the mainboard
  (undocumented).
- No legacy temperature registers 0x58–0x5C (which set a machine uses is discovered
  from traffic, not assumed).
- Broadcast writes to 0x10 are applied like unicast writes — assumed.
- The model does not poll the panel. Whether the real mainboard does is an M3 question.

## Using it

```c
vlx_machine_t m; vlx_machine_init(&m);
vlx_machine_feed(&m, rx, n);                    // bytes heard on the bus
n = vlx_machine_tick(&m, now_ms, tx, sizeof tx); // send these
m.p.t_outdoor = -15.0f; m.p.time_scale = 60.0f; // side panel knobs
vlx_machine_fault(&m, VLX_FAULT_...);
```

Tests: `firmware/test/host/test_vallox_machine.c` and `test_e2e_membus.c`.
```

- [ ] **Step 4: Update CI to run every test binary**

In `.github/workflows/firmware.yml`, replace

```yaml
      - name: Build and run host tests
        run: |
          cd firmware/test/host
          make
          ./run_tests
```

with

```yaml
      - name: Build and run host tests
        run: |
          cd firmware/test/host
          make test
```

- [ ] **Step 5: Run everything locally, including the IDF build check of the new components**

Run: `cd firmware/test/host && make clean && make test`
Expected: four `== run_tests*` blocks, each ending `... 0 failures`.

Run (if ESP-IDF is installed locally; otherwise rely on CI): `cd firmware && idf.py set-target esp32s3 && idf.py build`
Expected: builds; the new components compile even though `main` does not require them (IDF builds every component in `components/`). If `vallox_machine` fails under IDF because of float-to-int warnings treated as errors, add `-Wno-float-conversion` is **not** the fix — cast explicitly as the physics code already does.

- [ ] **Step 6: Commit, push, open the PR**

```bash
git add firmware/test/host/test_e2e_membus.c firmware/components/vallox_machine/README.md .github/workflows/firmware.yml firmware/test/host/Makefile
git commit -m "test(host): panel stub talks to the simulated machine over the memory bus; CI runs every host binary

The shape every later host test takes: the panel side sees only
panel_hal.h, the machine side only the memory bus, and the bytes in
between are real frames."
git push -u origin feat/s1-machine-model
gh pr create --title "feat(sim): S1 — machine emulator, HAL contract and host memory bus" --body "First milestone of docs/design/2026-08-23-panel-simulator-design.md. panel_hal.h contract; vallox_machine (register table with confidence classes, polls, writes + checksum acknowledge, 12 s broadcast round, reply-delay knob, coarse thermal model, fault injection); host HAL with a memory bus; four host test binaries run by CI. No UI yet (S2)."
```

---

## Self-review

**Spec coverage (S1 row + §3.1, §3.3, §4):** HAL contract — Task 1. Machine: register table with confidence classes — Task 2; polls answered, unknown → no answer — Task 2; writes + ack byte — Task 3; reply delay 0/10/200/never — Task 4; broadcast set/period/spacing — Task 4; physics (lags, efficiency, setpoint, frost, service, time scale, NTC round-trip) — Task 5; fault injection — Task 6; e2e "UI stub reads the speed" and the memory bus — Task 7; CI — Task 7. Start-up burst and SUSPEND/RESUME: spec lists them; protocol.md documents neither for the mainboard side without CO₂ sensors, so they are recorded as not modelled in the README (Task 7) rather than invented — consistent with the spec's "no answer rather than an invented value" rule.

**Placeholder scan:** none; the two header-name lookups (`VLX_STATUS_HEATING`, `VLX_STATUS_FAULT`, a `vlx_fault_t` member) are explicit instructions to read the existing header, with the rule to use its exact macro.

**Type consistency:** `vlx_machine_t` fields used in Tasks 3–7 (`reply_delay_ms`, `pending*`, `broadcast_*`, `p.*`, `t_*`, `service_elapsed_ms`, `have_tick`, `last_tick_ms`) are all declared in Task 2's header. `membus_machine_read/write`, `hal_host_*` names match between Task 1 and Task 7. `send_and_tick` helper defined in Task 2 is used unchanged in Tasks 3 and 6.
