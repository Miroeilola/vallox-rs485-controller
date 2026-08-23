#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
"""Fetch the KiCad-library 3D models a board references, for the GLB export in CI.

The kicad/kicad container has kicad-cli but not the packages3D library, so
`kicad-cli pcb export glb` there produces a bare board: the footprints' bodies
(switches, module, terminal block, ...) are silently left out. This reads every
`${KICAD10_3DMODEL_DIR}/<lib>.3dshapes/<model>.step` the .kicad_pcb references
and downloads those files — and only those — from the kicad-packages3D
repository at a pinned tag into a directory that is then passed to kicad-cli as
KICAD10_3DMODEL_DIR. Models the library does not have (a 404) are reported and
skipped, exactly as a desktop KiCad without that model would behave.

    fetch-3d-models.py <board.kicad_pcb> <out-dir> [--tag 10.0.5]

Exit status 0 when at least one model was fetched (or already present), 1 when
none could be, 2 on a usage error. Stdlib only: the container has python3.
"""
import argparse
import os
import re
import sys
import urllib.error
import urllib.request

RAW = "https://gitlab.com/kicad/libraries/kicad-packages3D/-/raw/{tag}/{path}"
PATTERN = re.compile(r'\(model "\$\{KICAD10_3DMODEL_DIR\}/([^"]+)"')


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("pcb")
    ap.add_argument("out")
    ap.add_argument("--tag", default="10.0.5", help="kicad-packages3D git tag")
    a = ap.parse_args()
    try:
        text = open(a.pcb, encoding="utf-8").read()
    except OSError as e:
        print(f"cannot read {a.pcb}: {e}", file=sys.stderr)
        return 2
    paths = sorted(set(PATTERN.findall(text)))
    if not paths:
        print("no ${KICAD10_3DMODEL_DIR} models referenced; nothing to fetch")
        return 0
    fetched = present = missing = 0
    for rel in paths:
        dst = os.path.join(a.out, rel)
        if os.path.exists(dst) and os.path.getsize(dst) > 0:
            present += 1
            continue
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        url = RAW.format(tag=a.tag, path=rel)
        try:
            with urllib.request.urlopen(url, timeout=60) as r, open(dst, "wb") as f:
                f.write(r.read())
            fetched += 1
            print(f"fetched  {rel}")
        except urllib.error.HTTPError as e:
            missing += 1
            print(f"missing  {rel} (HTTP {e.code} — not in kicad-packages3D {a.tag}; exported without a body)")
            if os.path.exists(dst):
                os.remove(dst)
        except (urllib.error.URLError, OSError) as e:
            missing += 1
            print(f"failed   {rel} ({e})")
            if os.path.exists(dst):
                os.remove(dst)
    print(f"{len(paths)} referenced: {fetched} fetched, {present} already present, {missing} missing")
    return 0 if fetched + present > 0 else 1


if __name__ == "__main__":
    sys.exit(main())
