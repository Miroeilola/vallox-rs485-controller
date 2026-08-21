# Research

Everything learned before the design started, with sources. This folder exists so
that a reader can check the reasoning, and so that a wrong assumption can be traced
back to where it entered.

| File | Contents |
|---|---|
| `target.md` | The device being replaced or interfaced with: model, connectors, measured voltages |
| `sources.md` | Every source with URL, date and a reliability rating |
| `protocol.md` | Claims about the interface, each tagged with its confidence and how it was verified |
| `measurement-plan.md` | What must be measured before the design can be trusted, and safety notes |
| `risks.md` | What could stop the project, and the way around it |
| `component-candidates.md` | Parts considered before the schematic exists, with live stock and the reasoning |
| `user-interface.md` | Display or LEDs: the options costed against live stock, with the power arithmetic and the recommendation |

## Reliability ratings

| Rating | Meaning |
|---|---|
| `manufacturer` | The manufacturer's own document |
| `reverse-engineered` | Someone else's analysis, unofficial |
| `anecdotal` | Forum post, single report, unverified |

Anything not rated `manufacturer` is an open question until it has been measured
on the actual hardware.
