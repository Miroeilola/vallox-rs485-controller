# Host-side unit tests: no ESP-IDF, no hardware. `make test` builds and runs all.
CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O1 -g
COMP    := ../../components
PROTO   := $(COMP)/vallox_protocol
MACH    := $(COMP)/vallox_machine
HAL     := $(COMP)/panel_hal
INC     := -I$(PROTO)/include -I$(MACH)/include -I$(HAL)/include -I.

UI      := $(COMP)/panel_ui
INC     += -I$(UI)/include -I$(UI)
UI_FONTS := $(UI)/fonts/font_inter_12.c $(UI)/fonts/font_inter_18.c $(UI)/fonts/font_inter_36.c

BINS := run_tests run_tests_hal run_tests_machine run_tests_e2e run_tests_ui_fonts_gfx

run_tests: $(PROTO)/vallox_protocol.c test_vallox_protocol.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

run_tests_hal: hal_host.c test_hal_host.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

run_tests_machine: $(PROTO)/vallox_protocol.c $(MACH)/vallox_machine.c $(MACH)/vallox_machine_physics.c test_vallox_machine.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

run_tests_e2e: $(PROTO)/vallox_protocol.c $(MACH)/vallox_machine.c $(MACH)/vallox_machine_physics.c hal_host.c test_e2e_membus.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

run_tests_ui_fonts_gfx: $(UI)/font.c $(UI_FONTS) test_panel_fonts_gfx.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

.PHONY: test clean all
all: $(BINS)
test: all
	@set -e; for b in $(BINS); do echo "== $$b"; ./$$b; done

clean:
	rm -rf $(BINS) *.dSYM
