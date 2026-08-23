# Host-side unit tests: no ESP-IDF, no hardware. `make test` builds and runs all.
CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O1 -g
COMP    := ../../components
PROTO   := $(COMP)/vallox_protocol
MACH    := $(COMP)/vallox_machine
HAL     := $(COMP)/panel_hal
UI      := $(COMP)/panel_ui
INC     := -I$(PROTO)/include -I$(MACH)/include -I$(HAL)/include -I$(UI)/include -I$(UI) -I.

PROTO_SRCS := $(PROTO)/vallox_protocol.c
MACH_SRCS  := $(MACH)/vallox_machine.c $(MACH)/vallox_machine_physics.c
UI_FONTS   := $(UI)/fonts/font_inter_12.c $(UI)/fonts/font_inter_18.c $(UI)/fonts/font_inter_36.c
UI_SRCS    := $(UI)/font.c $(UI_FONTS) $(UI)/gfx.c $(UI)/icons.c $(UI)/texts.c $(UI)/texts_en.c $(UI)/texts_fi.c \
              $(UI)/buttons.c $(UI)/vlx_client.c $(UI)/pages.c $(UI)/panel_ui.c

BINS := run_tests run_tests_hal run_tests_machine run_tests_e2e \
        run_tests_ui_fonts_gfx run_tests_ui_buttons_texts run_tests_client run_tests_ui run_tests_golden

run_tests: $(PROTO_SRCS) test_vallox_protocol.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

run_tests_hal: hal_host.c test_hal_host.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

run_tests_machine: $(PROTO_SRCS) $(MACH_SRCS) test_vallox_machine.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

run_tests_e2e: $(PROTO_SRCS) $(MACH_SRCS) hal_host.c test_e2e_membus.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

run_tests_ui_fonts_gfx: $(UI)/font.c $(UI_FONTS) $(UI)/gfx.c $(UI)/icons.c hal_host.c test_panel_fonts_gfx.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

run_tests_ui_buttons_texts: $(UI)/font.c $(UI_FONTS) $(UI)/texts.c $(UI)/texts_en.c $(UI)/texts_fi.c $(UI)/buttons.c hal_host.c test_panel_buttons_texts.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

run_tests_client: $(PROTO_SRCS) $(MACH_SRCS) $(UI)/vlx_client.c hal_host.c test_vlx_client.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

run_tests_ui: $(PROTO_SRCS) $(MACH_SRCS) $(UI_SRCS) hal_host.c test_panel_ui.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

run_tests_golden: $(PROTO_SRCS) $(MACH_SRCS) $(UI_SRCS) hal_host.c png.c test_panel_golden.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

panel_host: $(PROTO_SRCS) $(MACH_SRCS) $(UI_SRCS) hal_host.c png.c panel_host.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

.PHONY: test clean all golden
all: $(BINS) panel_host
test: all
	@set -e; for b in $(BINS); do echo "== $$b"; ./$$b; done
	./panel_host --ticks 100 --press ok,+,ok --png /dev/null >/dev/null
golden: run_tests_golden
	UPDATE_GOLDEN=1 ./run_tests_golden

clean:
	rm -rf $(BINS) panel_host *.dSYM golden/*.actual.png
