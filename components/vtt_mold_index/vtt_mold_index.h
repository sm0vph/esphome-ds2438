#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome::vtt_mold_index {

class VTTMoldIndex : public PollingComponent, public sensor::Sensor {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_temperature_sensor(sensor::Sensor *value) { temperature_sensor_ = value; }
  void set_humidity_sensor(sensor::Sensor *value) { humidity_sensor_ = value; }
  void set_material(uint8_t value) { material_ = value; }

 protected:
  struct StoredState {
    float mold_index;
    float unfavorable_hours;
  };

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  uint8_t material_{0};
  StoredState state_{0.0f, 0.0f};
  ESPPreferenceObject preference_;
  uint8_t updates_since_save_{0};

  float critical_rh_(float temperature) const;
  float maximum_index_(float critical_rh, float humidity) const;
  void save_state_();
};

}  // namespace esphome::vtt_mold_index
