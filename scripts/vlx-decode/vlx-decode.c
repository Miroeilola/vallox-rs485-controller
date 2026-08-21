// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// vlx-decode — turn a raw capture of a Vallox DIGIT RS-485 bus into annotated
// telegrams and a census of what was on it.
//
// This is the analysis tool for measurement M3 in
// docs/research/measurement-plan.md, and it is deliberately built from the same
// codec the firmware runs. A decoder that disagrees with the firmware is worse
// than no decoder, because it makes the firmware look wrong.
//
//   vlx-decode capture.bin          binary, as written by a serial capture
//   cat capture.bin | vlx-decode -  the same, from a pipe
//   vlx-decode -x capture.txt       hex text: 01 or 0x01, any separator
//   vlx-decode -q capture.bin       census only, no per-frame lines
//   vlx-decode --demo               synthetic traffic, to show the output format
//
// What this cannot tell you: anything about timing. A byte dump has no
// timestamps, so the inter-frame gaps that M4 needs have to come from the
// oscilloscope or from a capture format that carries time.

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vallox_protocol.h"

typedef struct {
    unsigned reg_count[256];
    unsigned sender_count[256];
    unsigned receiver_count[256];
    vlx_bus_survey_t survey;
    bool quiet;
    size_t frame_index;
} census_t;

// ---------------------------------------------------------------------------

static void print_value(const vlx_frame_t *f)
{
    if (vlx_frame_is_poll(f)) {
        printf("asks for %s (0x%02X)", vlx_register_name(f->value), f->value);
        return;
    }

    switch (f->reg) {
    case VLX_REG_TEMP_OUTDOOR:
    case VLX_REG_TEMP_EXHAUST:
    case VLX_REG_TEMP_EXTRACT:
    case VLX_REG_TEMP_SUPPLY:
    case VLX_REG_TEMP_OUTDOOR_LEGACY:
    case VLX_REG_TEMP_EXTRACT_LEGACY:
    case VLX_REG_TEMP_SUPPLY_LEGACY:
    case VLX_REG_TEMP_EXHAUST_LEGACY:
    case VLX_REG_POST_HEAT_TARGET:
    case VLX_REG_HEAT_SETPOINT:
    case VLX_REG_PREHEAT_SETPOINT:
    case VLX_REG_SUPPLY_FAN_STOP:
    case VLX_REG_BYPASS_SETPOINT:
        printf("%d C%s", vlx_temp_table(f->value),
               vlx_temp_is_saturated(f->value) ? "  [sensor at its rail]" : "");
        break;

    case VLX_REG_FAN_SPEED:
    case VLX_REG_FAN_SPEED_MAX:
    case VLX_REG_FAN_SPEED_DEFAULT: {
        const int speed = vlx_fan_speed_from_raw(f->value);
        if (speed == VLX_FAN_SPEED_INVALID) {
            printf("not a valid speed code");
        } else {
            printf("speed %d of 8", speed);
        }
        break;
    }

    case VLX_REG_RH_HIGHEST:
    case VLX_REG_RH_SENSOR_1:
    case VLX_REG_RH_SENSOR_2:
    case VLX_REG_RH_BASIC_LEVEL: {
        const int rh = vlx_rh_from_raw(f->value);
        if (rh == VLX_RH_INVALID) {
            printf("below the defined range");
        } else {
            printf("%d %%RH", rh);
        }
        break;
    }

    case VLX_REG_FAULT:
        printf("%s", vlx_fault_name(f->value));
        break;

    case VLX_REG_STATUS:
        printf("%s%s%s%s%s%s%s%s",
               (f->value & VLX_STATUS_POWER)       ? "power "   : "",
               (f->value & VLX_STATUS_CO2_CONTROL) ? "co2 "     : "",
               (f->value & VLX_STATUS_RH_CONTROL)  ? "rh "      : "",
               (f->value & VLX_STATUS_WINTER_MODE) ? "winter "  : "summer ",
               (f->value & VLX_STATUS_FILTER)      ? "filter! " : "",
               (f->value & VLX_STATUS_HEATING)     ? "heating " : "",
               (f->value & VLX_STATUS_FAULT)       ? "FAULT "   : "",
               (f->value & VLX_STATUS_SERVICE)     ? "service " : "");
        break;

    case VLX_REG_DC_FAN_SUPPLY:
    case VLX_REG_DC_FAN_EXHAUST:
    case VLX_REG_SERVICE_INTERVAL:
    case VLX_REG_SERVICE_MONTHS_LEFT:
    case VLX_REG_BOOST_MINUTES:
        printf("%u", (unsigned)f->value);
        break;

    default:
        printf("0x%02X", f->value);
        break;
    }
}

static void on_frame(const vlx_frame_t *f, void *ctx)
{
    census_t *c = (census_t *)ctx;
    c->frame_index++;
    c->reg_count[f->reg]++;
    c->sender_count[f->sender]++;
    c->receiver_count[f->receiver]++;
    vlx_bus_survey_observe(&c->survey, f);

    if (c->quiet) {
        return;
    }

    printf("%6zu  %02X %02X %02X %02X %02X %02X  %02X -> %02X  %-20s ",
           c->frame_index, f->domain, f->sender, f->receiver, f->reg, f->value,
           f->checksum, f->sender, f->receiver,
           vlx_frame_is_poll(f) ? "poll" : vlx_register_name(f->reg));
    print_value(f);
    if (!vlx_frame_is_plausible(f)) {
        printf("   <- implausible addresses, probably a false alignment");
    }
    printf("\n");
}

// ---------------------------------------------------------------------------

static void report(const census_t *c, const vlx_parser_t *p)
{
    printf("\n--- bus census ---\n");
    printf("frames accepted   %u\n", p->stats.frames_ok);
    printf("bytes discarded   %u   (resynchronising after a lost byte)\n",
           p->stats.bytes_discarded);
    printf("  wrong domain    %u\n", p->stats.domain_rejects);
    printf("  bad checksum    %u\n", p->stats.checksum_rejects);

    printf("\naddresses seen\n");
    for (unsigned a = 0; a < 256u; a++) {
        if (c->sender_count[a] == 0 && c->receiver_count[a] == 0) {
            continue;
        }
        const char *role = "unknown";
        if (a == VLX_ADDR_MAINBOARDS)      role = "all mainboards";
        else if (a == VLX_ADDR_PANELS)     role = "all panels";
        else if (a == VLX_ADDR_LON)        role = "LON gateway";
        else if (vlx_addr_is_mainboard(a)) role = "mainboard";
        else if (vlx_addr_is_panel(a))     role = "panel";
        printf("  0x%02X  sent %6u  addressed %6u   %s\n", a, c->sender_count[a],
               c->receiver_count[a], role);
    }

    printf("\nregisters seen\n");
    for (unsigned r = 0; r < 256u; r++) {
        if (c->reg_count[r] == 0) {
            continue;
        }
        printf("  0x%02X  %6u  %s\n", r, c->reg_count[r],
               vlx_register_name((uint8_t)r));
    }

    // Which temperature register set this machine uses is a per-machine fact,
    // not a protocol constant, and it is one of the open questions in
    // docs/research/protocol.md.
    const unsigned modern = c->reg_count[VLX_REG_TEMP_OUTDOOR] +
                            c->reg_count[VLX_REG_TEMP_EXHAUST] +
                            c->reg_count[VLX_REG_TEMP_EXTRACT] +
                            c->reg_count[VLX_REG_TEMP_SUPPLY];
    const unsigned legacy = c->reg_count[VLX_REG_TEMP_OUTDOOR_LEGACY] +
                            c->reg_count[VLX_REG_TEMP_EXTRACT_LEGACY] +
                            c->reg_count[VLX_REG_TEMP_SUPPLY_LEGACY] +
                            c->reg_count[VLX_REG_TEMP_EXHAUST_LEGACY];
    printf("\ntemperature register set\n");
    if (modern == 0 && legacy == 0) {
        printf("  neither set appeared - the capture may be too short\n");
    } else {
        printf("  0x32..0x35  %u frames\n  0x58..0x5C  %u frames\n", modern, legacy);
    }

    const uint8_t pick = vlx_bus_survey_pick_address(&c->survey);
    printf("\nfree panel address\n");
    if (pick == 0) {
        printf("  none - every panel address was in use in this capture\n");
    } else {
        printf("  0x%02X   (highest address not heard; longer captures are safer)\n",
               pick);
    }
    printf("\nNo timing information: a byte dump carries no timestamps. The\n"
           "inter-frame gaps that measurement M4 needs come from the scope.\n");
}

// ---------------------------------------------------------------------------

static int feed_hex(FILE *in, vlx_parser_t *p)
{
    int c;
    int nibble = -1;
    unsigned value = 0;
    while ((c = fgetc(in)) != EOF) {
        // Logic analyser exports write bytes as 01, 0x01 or 0X01, separated by
        // whitespace or commas. An 'x' directly after a lone 0 is a prefix, not
        // data.
        if ((c == 'x' || c == 'X') && nibble == 1 && value == 0) {
            nibble = -1;
            continue;
        }
        if (isxdigit(c)) {
            const unsigned digit = (unsigned)(isdigit(c) ? c - '0' : (tolower(c) - 'a' + 10));
            if (nibble < 0) {
                value = digit;
                nibble = 1;
            } else {
                value = (value << 4) | digit;
                vlx_parser_feed(p, (uint8_t)value);
                nibble = -1;
            }
        } else if (nibble >= 0) {
            // A single hex digit standing alone is a malformed capture, not a
            // byte. Refusing it is better than inventing a value.
            fprintf(stderr, "vlx-decode: odd hex digit in input\n");
            return 1;
        }
    }
    return nibble < 0 ? 0 : 1;
}

static void feed_binary(FILE *in, vlx_parser_t *p)
{
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        vlx_parser_feed_buffer(p, buf, n);
    }
}

static void feed_demo(vlx_parser_t *p)
{
    // Synthetic traffic. This is not a capture and must never be presented as
    // one; it exists so the output format can be shown in documentation before
    // any real hardware is available.
    printf("*** SYNTHETIC TRAFFIC - generated, not captured from a machine ***\n\n");

    uint8_t f[VLX_FRAME_LEN];
    vlx_make_write(0x11, VLX_ADDR_PANELS, VLX_REG_TEMP_OUTDOOR, vlx_temp_to_raw(-8), f);
    vlx_parser_feed_buffer(p, f, sizeof(f));
    vlx_make_write(0x11, VLX_ADDR_PANELS, VLX_REG_TEMP_SUPPLY, vlx_temp_to_raw(19), f);
    vlx_parser_feed_buffer(p, f, sizeof(f));
    vlx_make_write(0x11, VLX_ADDR_PANELS, VLX_REG_TEMP_EXTRACT, vlx_temp_to_raw(21), f);
    vlx_parser_feed_buffer(p, f, sizeof(f));
    vlx_make_write(0x11, VLX_ADDR_PANELS, VLX_REG_TEMP_EXHAUST, vlx_temp_to_raw(4), f);
    vlx_parser_feed_buffer(p, f, sizeof(f));

    vlx_make_poll(0x21, 0x11, VLX_REG_STATUS, f);
    vlx_parser_feed_buffer(p, f, sizeof(f));
    vlx_make_write(0x11, 0x21, VLX_REG_STATUS,
                   VLX_STATUS_POWER | VLX_STATUS_WINTER_MODE | VLX_STATUS_HEATING, f);
    vlx_parser_feed_buffer(p, f, sizeof(f));

    vlx_make_poll(0x21, 0x11, VLX_REG_FAN_SPEED, f);
    vlx_parser_feed_buffer(p, f, sizeof(f));
    vlx_make_write(0x11, 0x21, VLX_REG_FAN_SPEED, vlx_fan_speed_to_raw(3), f);
    vlx_parser_feed_buffer(p, f, sizeof(f));

    // A dropped byte in the middle of a frame, which is what a real capture
    // does when the receiver misses one. The parser has to walk back into
    // alignment on the frames that follow.
    vlx_make_write(0x11, 0x21, VLX_REG_RH_SENSOR_1, 0x92, f);
    vlx_parser_feed_buffer(p, f, sizeof(f) - 2);
    vlx_make_write(0x11, 0x21, VLX_REG_FAULT, VLX_FAULT_NONE, f);
    vlx_parser_feed_buffer(p, f, sizeof(f));
}

static void usage(void)
{
    fprintf(stderr,
            "usage: vlx-decode [-q] [-x] [--demo] [file|-]\n"
            "  -x      input is hex text rather than binary\n"
            "  -q      census only, no per-frame lines\n"
            "  --demo  synthetic traffic, to show the output format\n");
}

int main(int argc, char **argv)
{
    bool hex = false;
    bool demo = false;
    const char *path = NULL;
    static census_t census;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-x") == 0) {
            hex = true;
        } else if (strcmp(argv[i], "-q") == 0) {
            census.quiet = true;
        } else if (strcmp(argv[i], "--demo") == 0) {
            demo = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            usage();
            return 2;
        } else {
            path = argv[i];
        }
    }

    vlx_parser_t parser;
    vlx_parser_init(&parser, on_frame, &census);
    vlx_bus_survey_init(&census.survey);

    int rc = 0;
    if (demo) {
        feed_demo(&parser);
    } else {
        FILE *in = stdin;
        if (path != NULL && strcmp(path, "-") != 0) {
            in = fopen(path, "rb");
            if (in == NULL) {
                fprintf(stderr, "vlx-decode: cannot open %s\n", path);
                return 1;
            }
        }
        if (hex) {
            rc = feed_hex(in, &parser);
        } else {
            feed_binary(in, &parser);
        }
        if (in != stdin) {
            fclose(in);
        }
    }

    report(&census, &parser);
    return rc;
}
