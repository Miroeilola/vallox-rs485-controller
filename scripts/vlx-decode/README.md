# vlx-decode

Turns a raw capture of a Vallox DIGIT RS-485 bus into annotated telegrams and a
census of what was on the bus. This is the analysis tool for measurement M3 in
[`docs/research/measurement-plan.md`](../../docs/research/measurement-plan.md).

It is built from `firmware/components/vallox_protocol`, the same codec the
firmware runs. A decoder that disagrees with the firmware is worse than no
decoder, because it makes the firmware look wrong.

```bash
make
./vlx-decode capture.bin           # binary, as written by a serial capture
cat capture.bin | ./vlx-decode -   # the same, from a pipe
./vlx-decode -x capture.txt        # hex text: 01 or 0x01, any separator
./vlx-decode -q capture.bin        # census only
./vlx-decode --demo                # synthetic traffic, to show the output format
```

## What it tells you

- Every accepted telegram, decoded: who spoke to whom, which register, and the
  value in engineering units rather than hex.
- How many bytes it had to discard to stay in sync. The telegram has no start
  delimiter, so this number is the honest measure of how clean the capture is.
- Which addresses were on the bus and in which role, which registers appeared and
  how often, and therefore which of the two temperature register sets this
  particular machine uses — a per-machine fact, not a protocol constant.
- Who is on the panel side of the bus. This device replaces the original panel
  rather than joining it — two controllers on this bus override each other — so
  the useful question is not "which address is free" but "is anything else still
  connected".

## What it cannot tell you

Anything about time. A byte dump carries no timestamps, so the inter-frame gaps
that measurement M4 needs come from the oscilloscope, not from here.

It also cannot tell you that a telegram is real. A six-byte frame with no start
delimiter means a checksum passes at the wrong byte alignment roughly once in
65 000 tries, which on a long capture happens. Lines flagged
`implausible addresses` are the decoder saying so.

## The `--demo` output is not data

`--demo` generates traffic. It exists so the output format can be shown before
any hardware does, and it prints a banner saying so. Nothing it produces belongs
in a measurement report.
