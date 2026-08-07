# ESPHome humidity monitor

`ds248x_unified` is an external ESPHome component for a DS2484 I²C-to-1-Wire
bridge. One hub owns the full 1-Wire transaction sequence and supports both
DS18B20 temperature sensors and DS2438 modules fitted with Honeywell HIH-4031
humidity sensors. The optional `mold_risk_index` sensor calculates local
crawl-space mold risk from a chosen temperature and humidity sensor. The
optional `vtt_mold_index` sensor adds the time-dependent Finnish/VTT index.

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
  - source: github://sm0vph/esphome-humidity-monitor@main
    components: [ds248x_unified, mold_risk_index, vtt_mold_index]
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

## Local mold risk index

`mold_risk_index` implements the three RH thresholds used by the
Strixx76 Mold Risk Index project for unheated crawl spaces. It publishes risk
level `0` to `3` and can optionally publish the three temperature-dependent RH
limits. Use an external DS18B20 at the measurement location for `temperature`
and the matching HIH-4031 value for `humidity`.

```yaml
sensor:
  - platform: mold_risk_index
    name: "Crawl-space north mold risk"
    temperature: crawlspace_north_external_temperature
    humidity: crawlspace_north_humidity
    update_interval: 60s
    limit_level_1:
      name: "Crawl-space north RH limit level 1"
      entity_category: diagnostic
    limit_level_2:
      name: "Crawl-space north RH limit level 2"
      entity_category: diagnostic
    limit_level_3:
      name: "Crawl-space north RH limit level 3"
      entity_category: diagnostic
```

Risk `1` corresponds to possible growth after eight weeks or more, `2` to four
to eight weeks, and `3` to less than four weeks. This is a current-condition
risk classification; it does not persist a biological growth history across
reboots.

## VTT mould index

`vtt_mold_index` implements the time-dependent Finnish/VTT model, which
integrates temperature and RH into a mould index from `0` to `6`. The value is
saved by the ESP32 every 15 minutes so normal reboots retain the accumulated
history. It is diagnostic only: adding this sensor does not control any switch.

```yaml
sensor:
  - platform: vtt_mold_index
    name: "Crawl-space north VTT mould index"
    temperature: crawlspace_north_external_temperature
    humidity: crawlspace_north_humidity
    material: very_sensitive
    update_interval: 60s
```

Available material classes are `very_sensitive`, `sensitive`,
`medium_resistant`, and `resistant`; use `very_sensitive` as the conservative
choice for wood or organic material when the actual material is unknown. The
model uses the sensor temperature as a surface-temperature approximation. A
separate surface-temperature sensor is preferable if the monitored surface is
materially colder than the air.

## Validation

Hardware-tested with one DS2484, six DS18B20 sensors and three DS2438/HIH-4031
modules on the same 1-Wire network using ESPHome 2026.7.4.
