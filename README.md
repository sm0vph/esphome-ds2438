# ESPHome DS2438

An external ESPHome component for Maxim DS2438-based sensor modules. It
communicates through ESPHome's shared 1-Wire API and therefore works on a
DS2484 bus together with standard devices such as DS18B20 temperature sensors.

Humidity conversion is selected explicitly in YAML. The first supported model
is the Honeywell HIH-4031; additional sensor profiles can be added without
changing the component platform or repository name.

The component publishes:

- temperature-compensated relative humidity,
- the DS2438's local temperature,
- uncompensated relative humidity for diagnostics,
- the HIH-4031 output voltage (`VAD`),
- the DS2438 supply voltage (`VDD`).

## Hardware assumptions

The implementation assumes a Honeywell HIH-4031 powered within its specified
4.0 V to 5.8 V range, with its analog output connected to the DS2438 `VAD`
input. The DS2438 family code is `0x26`.

The driver does not write the A/D selection to EEPROM during measurements. It
changes the volatile scratchpad configuration, performs the conversions and
then restores the previous configuration. Scratchpad reads are protected by
CRC checks.

DS18B20 and DS2438 devices may share the same bus master. Each device is
selected by its unique 64-bit 1-Wire address, and conversions are performed
using short asynchronous steps so ESPHome's main loop is not blocked.

## Installation

Place the repository's `components` directory beside the ESPHome YAML file and
use a local source:

```yaml
external_components:
  - source:
      type: local
      path: components
```

After this repository has been published, it can instead be loaded directly
from a tagged Git revision:

```yaml
external_components:
  - source: github://OWNER/REPOSITORY@VERSION
    components:
      - ds2438
    refresh: never
```

## Configuration

```yaml
sensor:
  - platform: ds2438
    address: 0x0000000000000026  # Replace with the DS2438 address.
    one_wire_id: one_wire_bus
    update_interval: 30s

    humidity:
      name: "Relative humidity"
      model: HIH4031

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

`humidity` and its `model` are required. Currently, `HIH4031` is the only
accepted model; ESPHome rejects unsupported values during validation. The
other output sensors are optional. See
[`example.yaml`](example.yaml) for a complete example
using a DS2484 and a DS18B20 on the same bus.

## Supported humidity models

### HIH4031

The HIH-4031 is part of Honeywell's HIH-4000 analog humidity sensor family.
The component calculates sensor-relative humidity from the ratiometric voltage
and then applies temperature compensation:

```text
sensor_RH = ((VAD / VDD) - 0.16) / 0.0062
true_RH   = sensor_RH / (1.0546 - 0.00216 × temperature_C)
```

The DS2438's own temperature is used for this compensation because it is
located on the same module as the humidity sensor.

## Status

The component builds with ESPHome 2026.7.3 using ESP-IDF for ESP32-S3. Hardware
validation of the `HIH4031` profile against OWFS `HIH4000/humidity` is still
pending.
