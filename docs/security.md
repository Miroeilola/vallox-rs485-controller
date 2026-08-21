# Security considerations

A networked device that cannot be updated is a liability. This document records
what was decided and why, so that a reader can judge the device rather than
trust it.

**Status: decided, not implemented.** No firmware beyond the protocol codec
exists yet. What follows is the position the firmware will be built to, written
before the code so it constrains the code rather than describing it afterwards.

## Threat model, stated plainly

The device sits inside or beside a ventilation machine in a private home, on the
owner's own network, wired to a five-terminal bus that carries no personal data.
It holds one secret worth taking: the Wi-Fi credentials of the house.

What it can do if compromised is change fan speed and a handful of setpoints on
a ventilation unit. That is a nuisance, not a hazard — the machine's own safety
interlocks, over-temperature thermostats and frost protection are in the
machine's firmware and this device cannot reach them.

The realistic attacker is someone on the local network, not someone with a
soldering iron in the utility room. The design follows that ranking.

## Update path

| | |
|---|---|
| OTA support | Required. A bus-powered device inside a machine is not going to be carried to a laptop. |
| Rollback on failure | Two OTA partitions, `esp_ota_mark_app_valid_cancel_rollback` only after the device has reached its broker. Firmware that boots but cannot connect rolls back by itself. |
| Signed images | Yes, image signature verification without secure boot, so a bad image is refused at update time. Details below. |
| Version reporting | From the git tag at build time, readable from the device at runtime. Never a hand-maintained constant. |

## Boot and flash

| Measure | Status | Reasoning |
|---|---|---|
| Secure boot | Not enabled in the published build | It is a one-way fuse burn, and enabling it with a Mironet key would make the board refuse firmware its owner built. The whole premise of this project is that the reader can build one and own it, so a lock that only the vendor holds the key to is the wrong trade. The build option and instructions are provided for anyone who wants it on their own key. |
| Flash encryption | Not enabled in the published build | Also irreversible in release mode, and it would surprise a builder who expects to reflash the board they assembled. Its benefit here is protecting the Wi-Fi credentials from someone holding the board, which is behind the local-network attacker in the ranking above. |
| Signed OTA images | Enabled | This is the part that carries real weight: it makes a hostile update fail without taking away the owner's ability to flash over USB. |

The residual risk is stated rather than hidden: **anyone with physical possession
of the board can read the Wi-Fi credentials out of its flash.** That is the price
of leaving it reflashable, and for a device inside the owner's own ventilation
machine it is the right price. Anyone whose threat model includes physical access
should enable flash encryption, and the documentation says how.

## Network exposure

- **Services listening:** none by design. The device opens outbound connections
  to an MQTT broker or a Home Assistant instance and listens on nothing else. A
  device with no listening socket has a much smaller attack surface than one with
  a web interface, and it does not need a web interface.
- **Transport security:** TLS to the broker when the broker offers it. When it
  does not, the device says so in its own status output rather than pretending.
- **Credentials:** provisioned at runtime into NVS, never compiled in and never
  committed. There is no default password and no recovery password.
- **Provisioning:** the credential entry path is available at first boot and
  after an explicit reset action, not permanently. A provisioning access point
  that never closes is a permanent open door.

## Dependencies

Component versions are pinned in `idf_component.yml`. No wildcard versions: a
supply-chain problem that arrives through an unpinned dependency is not
detectable afterwards from the repository.

## Bus safety, which is not the same as security

The device holds an allow-list of registers it may write, enforced in
`firmware/components/vallox_protocol` and asserted by a unit test that fails if
the list grows. Vallox states that writing an incorrect register or value can
damage the unit. Nothing in the network path can bypass that list, so a
compromised broker cannot make the device write a register that has not been
verified on hardware.

## Known limitations

- None of the above is implemented yet. There is no firmware to audit.
- No security review by anyone other than the author has taken place.
- The device makes no claim of compliance with any specific regulation. If it is
  ever sold rather than published, that is a separate assessment and this
  document is not it.
