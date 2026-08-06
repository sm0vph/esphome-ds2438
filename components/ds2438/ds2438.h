#pragma once

#include "esphome/components/one_wire/one_wire.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::ds2438 {

enum class HumidityModel : uint8_t {
  HIH4031,
};

class DS2438Component : public PollingComponent, public one_wire::OneWireDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_humidity_model(HumidityModel model) { this->humidity_model_ = model; }
  void set_humidity_sensor(sensor::Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_humidity_raw_sensor(sensor::Sensor *sensor) { this->humidity_raw_sensor_ = sensor; }
  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }
  void set_vad_sensor(sensor::Sensor *sensor) { this->vad_sensor_ = sensor; }
  void set_vdd_sensor(sensor::Sensor *sensor) { this->vdd_sensor_ = sensor; }

 protected:
  static constexpr uint8_t FAMILY_CODE = 0x26;
  static constexpr uint8_t PAGE_ZERO = 0x00;
  static constexpr uint8_t COMMAND_CONVERT_T = 0x44;
  static constexpr uint8_t COMMAND_CONVERT_V = 0xB4;
  static constexpr uint8_t COMMAND_READ_SCRATCHPAD = 0xBE;
  static constexpr uint8_t COMMAND_WRITE_SCRATCHPAD = 0x4E;
  static constexpr uint8_t CONFIG_AD = 0x08;
  static constexpr uint32_t CONVERSION_TIME_MS = 12;

  HumidityModel humidity_model_{HumidityModel::HIH4031};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *humidity_raw_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *vad_sensor_{nullptr};
  sensor::Sensor *vdd_sensor_{nullptr};

  bool measurement_in_progress_{false};
  bool config_captured_{false};
  uint8_t original_config_{0};
  float temperature_{0.0f};
  float vdd_{0.0f};

  bool read_page_zero_(uint8_t *data);
  bool write_config_(uint8_t config);
  bool start_conversion_(uint8_t command);
  bool calculate_humidity_(float vad, float *humidity_raw, float *humidity) const;
  const char *humidity_model_name_() const;
  void read_temperature_and_start_vdd_();
  void read_vdd_and_start_vad_();
  void read_vad_and_publish_();
  void fail_measurement_(const char *message);
  void publish_nan_();
};

}  // namespace esphome::ds2438
