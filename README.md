# ESPHome DS248x unified hub

`ds248x_unified` is an external ESPHome component for a DS2484 I²C-to-1-Wire
bridge. One hub owns the full 1-Wire transaction sequence and supports both
DS18B20 temperature sensors and DS2438 modules fitted with Honeywell HIH-4031
humidity sensors.

## What it publishes

- DS18B20 temperature;
- temperature-compensated relative humidity from each DS2438/HIH-4031;
- DS2438 local temperature, VAD and VDD; and
- uncompensated relative humidity as diagnostics.

The hub runs one controlled update cycle: DS18B20 conversion and reads first,
then each DS2438 temperature/VDD/VAD measurement. This avoids concurrent
commands to the same DS2484 and is suitable for mixed 1-Wire networks.

## Hardware

- DS2484 at I²C address `0x18`;
- `SLPZ` must be held high;
- a pull-up is required on the 1-Wire bus; and
- HIH-4031/DS2438 modules require a valid 4.0–5.8 V supply for the humidity
  calculation.

## Installation

```yaml
external_components:
  - source: github://sm0vph/esphome-ds2438@main
    components: [ds248x_unified]
    refresh: 0s
```

`refresh: 0s` makes ESPHome fetch the current component revision for every
compile. Pin the revision to a tag or commit once you want reproducible builds.

## Configuration

```yaml
i2c:
  sda: GPIO2
  scl: GPIO1
  frequency: 100kHz

ds248x_unified:
  - id: one_wire_hub
    address: 0x18
    active_pullup: true
    strong_pullup: true
    update_interval: 60s

    ds18b20:
      - name: "External temperature"
        address: 0x0000000000000028

    ds2438:
      - id: humidity_module
        address: 0x0000000000000026
        humidity:
          name: "Relative humidity"
        temperature:
          name: "DS2438 temperature"
        humidity_raw:
          name: "Uncompensated humidity"
          entity_category: diagnostic
        vad:
          name: "HIH-4031 output voltage"
          entity_category: diagnostic
        vdd:
          name: "DS2438 supply voltage"
          entity_category: diagnostic
```

Replace every example address with the complete 64-bit 1-Wire address. See
[`example.yaml`](example.yaml) for a complete ESPHome configuration.

## HIH-4031 calculation

```text
raw_RH  = ((VAD / VDD) - 0.16) / 0.0062
true_RH = raw_RH / (1.0546 - 0.00216 × temperature_C)
```

The DS2438 temperature is used for compensation. The hub uses DS2438 Recall
Memory before every scratchpad read, as required by the DS2438 protocol.

## Validation

Hardware-tested with one DS2484, six DS18B20 sensors and three DS2438/HIH-4031
modules on the same 1-Wire network using ESPHome 2026.7.4.
