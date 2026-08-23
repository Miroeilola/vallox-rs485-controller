#!/bin/bash
# Runs every netlist in this folder with ngspice in batch mode and keeps the
# measured lines in out/<name>.txt. One question per netlist; the question is the
# comment block at the top of each file. Findings live in docs/measurements/.
set -uo pipefail
cd "$(dirname "$0")"; mkdir -p out
for f in [0-9]*.cir; do
  n="${f%.cir}"
  ngspice -b "$f" > "out/$n.log" 2>&1
  grep -E "^(RSRC=|EN start|IL=|VRAIL=|SM712 model|RPTC=|VPK=|LED corner|BTN )" "out/$n.log" > "out/$n.txt"
  echo "== $n"; cat "out/$n.txt"
  grep -i "^Error" "out/$n.log" | head -3
done
