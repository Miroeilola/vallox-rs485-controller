# Layout notes

Chronological, measurement-based log of schematic and PCB work. Written at the end
of every `/kicad-layout` session. Its value is in the numbers and in the dead ends:
"tried X, it did not work because Y" saves the next attempt.

## Template

### YYYY-MM-DD — <subsystem>

- **Baseline.** DRC / ERC violation counts and types before the session.
- **Changes.** What was moved, routed or replaced.
- **Measured.** Values before and after: trace widths, loop areas, clearances, lengths.
- **Did not work.** What was tried and rejected, and why.
- **Result.** DRC / ERC delta against the baseline. Zero new violations, or the reason.
- **Open.** What is left for the next session.
