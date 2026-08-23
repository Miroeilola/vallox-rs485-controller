# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#
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
	mkdir -p public
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
