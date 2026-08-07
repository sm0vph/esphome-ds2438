#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

#include <cmath>

namespace esphome::ds248x_unified {

class DS248CycleCompleteTrigger : public Trigger<> {};

class DS18B20Sensor : public sensor::Sensor {
 public:
  void set_address(uint64_t value) { address_ = value; }
  uint64_t address() const { return address_; }
 protected:
  uint64_t address_{0};
};

class DS2438Sensor {
 public:
  void set_address(uint64_t value) { address_ = value; }
  uint64_t address() const { return address_; }
  void set_humidity(sensor::Sensor *value) { humidity_ = value; }
  void set_raw(sensor::Sensor *value) { raw_ = value; }
  void set_temperature(sensor::Sensor *value) { temperature_ = value; }
  void set_vad(sensor::Sensor *value) { vad_ = value; }
  void set_vdd(sensor::Sensor *value) { vdd_ = value; }
  void set_invalid_readings(sensor::Sensor *value) { invalid_readings_ = value; }
  void set_humidity_model(uint8_t value) { humidity_model_ = value; }
  void setup();
  bool is_valid_reading(float temperature, float vdd, float vad) const;
  bool publish(float temperature, float vdd, float vad);
  void report_invalid_reading();
  void publish_nan();
 protected:
  uint64_t address_{0};
  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *raw_{nullptr};
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *vad_{nullptr};
  sensor::Sensor *vdd_{nullptr};
  sensor::Sensor *invalid_readings_{nullptr};
  uint32_t invalid_reading_count_{0};
  ESPPreferenceObject invalid_readings_preference_;
  // 0=HIH-4030/4031, 1=HIH-3600, 2=HIH-4000, 3=HIH-5030/5031.
  uint8_t humidity_model_{0};
};

class DS248xUnifiedComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void set_active_pullup(bool value) { active_pullup_ = value; }
  void set_strong_pullup(bool value) { strong_pullup_ = value; }
  void register_ds18b20(DS18B20Sensor *value) { ds18b20_.push_back(value); }
  void register_ds2438(DS2438Sensor *value) { ds2438_.push_back(value); }
  void register_cycle_complete_trigger(DS248CycleCompleteTrigger *value) { cycle_complete_triggers_.push_back(value); }
 protected:
  bool active_pullup_{true};
  bool strong_pullup_{true};
  std::vector<DS18B20Sensor *> ds18b20_;
  std::vector<DS2438Sensor *> ds2438_;
  std::vector<DS248CycleCompleteTrigger *> cycle_complete_triggers_;
  size_t index_{0};
  uint8_t page_[9]{};
  float temperature_{NAN};
  float vdd_{NAN};
  bool vad_retry_pending_{false};
  bool cycle_had_error_{false};

  bool wait_(uint8_t *status = nullptr);
  bool set_config_(uint8_t config);
  bool reset_wire_();
  bool write_wire_(uint8_t value);
  bool read_wire_(uint8_t *value);
  bool select_(uint64_t address);
  bool command_(uint64_t address, uint8_t command, bool strong = false);
  bool read_ds2438_page_(uint64_t address);
  bool write_ds2438_config_(uint64_t address, uint8_t config);
  bool read_ds18_(DS18B20Sensor *sensor);
  void read_next_ds18_();
  void start_next_ds2438_();
  void read_ds2438_temperature_();
  void read_ds2438_vdd_();
  void read_ds2438_vad_();
  void fail_ds2438_(const char *message);
  void finish_cycle_();
};

}  // namespace esphome::ds248x_unified
