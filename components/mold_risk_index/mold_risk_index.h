#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::mold_risk_index {

class MoldRiskIndex : public PollingComponent, public sensor::Sensor {
 public:
  void update() override;
  void dump_config() override;
  void set_temperature_sensor(sensor::Sensor *value) { temperature_sensor_ = value; }
  void set_humidity_sensor(sensor::Sensor *value) { humidity_sensor_ = value; }
  void set_limit_level_1_sensor(sensor::Sensor *value) { limit_level_1_sensor_ = value; }
  void set_limit_level_2_sensor(sensor::Sensor *value) { limit_level_2_sensor_ = value; }
  void set_limit_level_3_sensor(sensor::Sensor *value) { limit_level_3_sensor_ = value; }

 protected:
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *limit_level_1_sensor_{nullptr};
  sensor::Sensor *limit_level_2_sensor_{nullptr};
  sensor::Sensor *limit_level_3_sensor_{nullptr};
  static int limit_level_1_(float temperature);
  static int limit_level_2_(float temperature);
  static int limit_level_3_(float temperature);
  void publish_nan_();
};

}  // namespace esphome::mold_risk_index
