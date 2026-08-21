# ESPHome external component

Home Assistant users can run this device through ESPHome instead of the ESP-IDF
firmware. The component is a thin wrapper: the protocol codec lives in
`firmware/components/vallox_protocol` and is shared, so the two builds cannot
drift apart.

Sharing is done with two symbolic links inside this component directory pointing
back at the codec. ESPHome copies the component directory into its build, and a
relative link inside the repository survives both a `type: local` path and a
`type: git` clone. It does not survive a zip download on Windows, which is a
known limitation and the reason this is written down rather than assumed.

**Status: not built yet.** The wrapper compiles against the codec API and does
nothing but listen. It stays that way until the bus has been captured and the
electrical work is done — see `docs/research/measurement-plan.md`. The example
below has not been run through `esphome compile` on real hardware, and the pin
numbers in it are placeholders from the template, not a pinout.

## Use it

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/Miroeilola/vallox-rs485-controller
      ref: main
    components: [ vallox_rs485_controller ]
```

A complete, working configuration is in [`example.yaml`](example.yaml).

## Test before publishing

```bash
esphome config example.yaml     # schema check
esphome compile example.yaml    # full build
```

Both must pass before a release. An example that does not compile is worse than
no example at all.
