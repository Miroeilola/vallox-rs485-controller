# Security considerations

A networked device that cannot be updated is a liability. This document records what
was decided and why, so that a reader can judge the device rather than trust it.

## Update path

| | |
|---|---|
| OTA support | — |
| Rollback on failure | — |
| Signed images | — |

## Boot and flash

| Measure | Status | Reasoning |
|---|---|---|
| Secure boot | — | — |
| Flash encryption | — | — |

If either is disabled, the reason is stated here. "Not needed for this device" is an
acceptable reason; silence is not.

## Network exposure

- Services listening: —
- Transport security: —
- Credentials: provisioned at runtime, never compiled in

## Known limitations

—
