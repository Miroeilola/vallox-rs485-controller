# ESPHome external component

Home Assistant users can run this device through ESPHome instead of the ESP-IDF
firmware. The component is a thin wrapper: protocol logic lives in
`firmware/components/device_core` and is shared, so the two builds cannot drift
apart.

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
