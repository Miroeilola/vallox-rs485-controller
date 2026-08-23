#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
"""Cross-check the emulator's register table against docs/research/protocol.md.

The machine model is only as right as the protocol document. This guard reads
both and fails when they disagree on what can be checked mechanically:

  * writability — a register listed under "Control — read and write" must be
    writable in the table, one under "Measurements — read only" must not, and a
    bit register is writable when any of its bits is R/W;
  * encoding — a register whose protocol encoding is the NTC table must live
    in k_temp_regs (so its default is written in degrees and converted), and a
    register in k_temp_regs must be NTC-encoded;
  * presence — a register the protocol document does not describe is reported
    as a warning; the model must not answer for it without a recorded decision.

Exit code 0 = clean (warnings allowed), 1 = at least one error, 2 = parse failure.
Run from the repository root:  python3 scripts/check-machine-regs.py
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROTO_MD = ROOT / "docs/research/protocol.md"
PROTO_H = ROOT / "firmware/components/vallox_protocol/include/vallox_protocol.h"
REGS_H = ROOT / "firmware/components/vallox_machine/vallox_machine_regs.h"

ROW = re.compile(r"^\|\s*0x([0-9A-Fa-f]{2})\s*\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|")
BITREG = re.compile(r"^\*\*0x([0-9A-Fa-f]{2}) — ([^.]+)\.\*\*")
BITROW = re.compile(r"^\|\s*\d\s*\|[^|]+\|\s*(R/W|R)\b")
DEFINE = re.compile(r"^#define\s+(VLX_REG_\w+)\s+0x([0-9A-Fa-f]{2})u")
TABLE_ROW = re.compile(r"^\s*\{\s*(VLX_REG_\w+)\s*,\s*(true|false)\s*,")


def parse_protocol():
    """Return {addr: {"access": "rw"|"ro"|None, "encoding": str, "section": str}}."""
    regs = {}
    section = None
    bitreg = None
    for line in PROTO_MD.read_text(encoding="utf-8").splitlines():
        if line.startswith("### "):
            section = line[4:].strip()
            bitreg = None
            continue
        if line.startswith("## ") and not line.startswith("## Register map"):
            section = None
            bitreg = None
        if section in ("Measurements — read only", "Control — read and write"):
            m = ROW.match(line)
            if m:
                addr = int(m.group(1), 16)
                regs[addr] = {
                    "access": "ro" if section.startswith("Measurements") else "rw",
                    "encoding": m.group(3).strip(),
                    "section": section,
                }
        elif section == "Bit registers":
            m = BITREG.match(line)
            if m:
                bitreg = int(m.group(1), 16)
                regs.setdefault(bitreg, {"access": None, "encoding": "bits", "section": section})
                if "read only" in line.lower():
                    regs[bitreg]["access"] = "ro"
                continue
            m = BITROW.match(line)
            if m and bitreg is not None:
                if m.group(1) == "R/W":
                    regs[bitreg]["access"] = "rw"
                elif regs[bitreg]["access"] is None:
                    regs[bitreg]["access"] = "ro"
    return regs


def parse_defines():
    names = {}
    for line in PROTO_H.read_text(encoding="utf-8").splitlines():
        m = DEFINE.match(line)
        if m:
            names[m.group(1)] = int(m.group(2), 16)
    return names


def parse_table():
    """Return [(name, writable, in_temp_table)] in file order."""
    rows = []
    in_temp = False
    for line in REGS_H.read_text(encoding="utf-8").splitlines():
        if "k_regs[]" in line:
            in_temp = False
        elif "k_temp_regs[]" in line:
            in_temp = True
        m = TABLE_ROW.match(line)
        if m:
            rows.append((m.group(1), m.group(2) == "true", in_temp))
    return rows


def main():
    for p in (PROTO_MD, PROTO_H, REGS_H):
        if not p.exists():
            print(f"missing: {p}")
            return 2
    proto = parse_protocol()
    names = parse_defines()
    rows = parse_table()
    if not proto or not names or not rows:
        print("parse failure: protocol rows=%d defines=%d table rows=%d" % (len(proto), len(names), len(rows)))
        return 2

    errors, warnings = [], []
    seen = set()
    for name, writable, in_temp in rows:
        if name not in names:
            errors.append(f"{name}: not defined in vallox_protocol.h")
            continue
        addr = names[name]
        if addr in seen:
            errors.append(f"{name} (0x{addr:02X}): listed twice in the table")
        seen.add(addr)
        doc = proto.get(addr)
        if doc is None:
            warnings.append(f"{name} (0x{addr:02X}): not described in protocol.md — answered with an undocumented value")
            continue
        access = doc["access"]
        if access == "rw" and not writable:
            errors.append(f"{name} (0x{addr:02X}): protocol.md says read/write ({doc['section']}), table says read-only")
        if access == "ro" and writable:
            errors.append(f"{name} (0x{addr:02X}): protocol.md says read-only ({doc['section']}), table says writable")
        is_ntc = doc["encoding"].startswith("NTC table")
        if is_ntc and not in_temp:
            errors.append(f"{name} (0x{addr:02X}): NTC-encoded in protocol.md but not in k_temp_regs (default would be a raw byte, not degrees)")
        if in_temp and not is_ntc:
            errors.append(f"{name} (0x{addr:02X}): in k_temp_regs but protocol.md encoding is '{doc['encoding']}'")

    for w in warnings:
        print("WARN ", w)
    for e in errors:
        print("ERROR", e)
    print(f"{len(rows)} table rows checked against {len(proto)} protocol.md registers: {len(errors)} errors, {len(warnings)} warnings")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
