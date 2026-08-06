#include "mold_risk_index.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome::mold_risk_index {
static const char *const TAG = "mold_risk_index";

int MoldRiskIndex::limit_level_1_(float temperature) {
  if (temperature < 0.0f || temperature > 50.0f) return 100;
  return std::clamp(static_cast<int>(std::lround(20.0f * std::exp(-temperature * 0.15f) + 73.0f)), 72, 100);
}
int MoldRiskIndex::limit_level_2_(float temperature) {
  if (temperature < 0.0f || temperature > 50.0f) return 100;
  return std::clamp(static_cast<int>(std::lround(17.0f * std::exp(-temperature * 0.11f) + 80.0f)), 79, 100);
}
int MoldRiskIndex::limit_level_3_(float temperature) {
  if (temperature < 0.0f || temperature > 50.0f) return 100;
  return std::clamp(static_cast<int>(std::lround(15.0f * std::exp(-temperature * 0.10f) + 85.0f)), 84, 100);
}

void MoldRiskIndex::update() {
  if (this->temperature_sensor_ == nullptr || this->humidity_sensor_ == nullptr || !this->temperature_sensor_->has_state() ||
      !this->humidity_sensor_->has_state() || !std::isfinite(this->temperature_sensor_->state) ||
      !std::isfinite(this->humidity_sensor_->state)) {
    this->status_set_warning("input sensor unavailable");
    this->publish_nan_();
    return;
  }
  const float temperature = this->temperature_sensor_->state;
  const float humidity = std::clamp(this->humidity_sensor_->state, 0.0f, 100.0f);
  const int limit_1 = limit_level_1_(temperature);
  const int limit_2 = limit_level_2_(temperature);
  const int limit_3 = limit_level_3_(temperature);
  const int risk = humidity > limit_3 ? 3 : humidity > limit_2 ? 2 : humidity > limit_1 ? 1 : 0;
  this->status_clear_warning();
  this->publish_state(risk);
  if (this->limit_level_1_sensor_ != nullptr) this->limit_level_1_sensor_->publish_state(limit_1);
  if (this->limit_level_2_sensor_ != nullptr) this->limit_level_2_sensor_->publish_state(limit_2);
  if (this->limit_level_3_sensor_ != nullptr) this->limit_level_3_sensor_->publish_state(limit_3);
  ESP_LOGD(TAG, "T=%.2f RH=%.1f limits=%d/%d/%d risk=%d", temperature, humidity, limit_1, limit_2, limit_3, risk);
}

void MoldRiskIndex::publish_nan_() {
  this->publish_state(NAN);
  if (this->limit_level_1_sensor_ != nullptr) this->limit_level_1_sensor_->publish_state(NAN);
  if (this->limit_level_2_sensor_ != nullptr) this->limit_level_2_sensor_->publish_state(NAN);
  if (this->limit_level_3_sensor_ != nullptr) this->limit_level_3_sensor_->publish_state(NAN);
}

void MoldRiskIndex::dump_config() {
  ESP_LOGCONFIG(TAG, "Mold risk index:");
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

}  // namespace esphome::mold_risk_index
