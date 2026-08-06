#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include <cmath>

namespace esphome::ds2438_ds248x {

enum class HumidityModel : uint8_t { HIH4031 };

class DS2438DS248xComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void set_ds2438_address(uint64_t value) { this->ds2438_address_ = value; }
  void set_humidity_model(HumidityModel value) { this->humidity_model_ = value; }
  void set_humidity_sensor(sensor::Sensor *value) { this->humidity_sensor_ = value; }
  void set_humidity_raw_sensor(sensor::Sensor *value) { this->humidity_raw_sensor_ = value; }
  void set_temperature_sensor(sensor::Sensor *value) { this->temperature_sensor_ = value; }
  void set_vad_sensor(sensor::Sensor *value) { this->vad_sensor_ = value; }
  void set_vdd_sensor(sensor::Sensor *value) { this->vdd_sensor_ = value; }

 protected:
  static constexpr uint8_t FAMILY_CODE = 0x26;
  static constexpr uint8_t CONFIG_AD = 0x08;
  static constexpr uint32_t CONVERSION_TIME_MS = 15;
  uint64_t ds2438_address_{0};
  HumidityModel humidity_model_{HumidityModel::HIH4031};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *humidity_raw_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *vad_sensor_{nullptr};
  sensor::Sensor *vdd_sensor_{nullptr};
  bool busy_{false};
  uint8_t original_config_{0};
  float temperature_{NAN};
  float vdd_{NAN};

  bool wait_(uint8_t *status = nullptr);
  bool set_bridge_config_(uint8_t config);
  bool configure_bridge_();
  bool reset_wire_();
  bool write_wire_(uint8_t value);
  bool read_wire_(uint8_t *value);
  bool select_();
  bool command_(uint8_t value, bool strong_pullup = false);
  bool read_page_(uint8_t *page);
  bool write_ds2438_config_(uint8_t value);
  void read_temperature_();
  void read_vdd_();
  void read_vad_();
  void fail_(const char *message);
  void publish_nan_();
};

}  // namespace esphome::ds2438_ds248x
