#include "vtt_mold_index.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome::vtt_mold_index {
namespace {
static const char *const TAG = "vtt_mold_index";
constexpr float HOURS_PER_UPDATE = 1.0f / 60.0f;

struct MaterialParameters {
  float k1_below_1;
  float k1_at_least_1;
  float a;
  float b;
  float c;
  float rh_min;
};

// Parameters from the improved Finnish/VTT mould-growth model. The default
// class is deliberately conservative for a crawl space with organic material.
constexpr MaterialParameters MATERIALS[] = {
    {1.000f, 2.000f, 1.0f, 7.0f, 2.0f, 80.0f},  // very_sensitive
    {0.578f, 0.386f, 0.3f, 6.0f, 1.0f, 80.0f},  // sensitive
    {0.072f, 0.097f, 0.0f, 5.0f, 1.5f, 85.0f},  // medium_resistant
    {0.033f, 0.014f, 0.0f, 3.0f, 1.0f, 85.0f},  // resistant
};
}  // namespace

void VTTMoldIndex::setup() {
  this->preference_ = global_preferences->make_preference<StoredState>(this->get_object_id_hash());
  if (!this->preference_.load(&this->state_) || !std::isfinite(this->state_.mold_index) ||
      !std::isfinite(this->state_.unfavorable_hours)) {
    this->state_ = {0.0f, 0.0f};
  }
  this->state_.mold_index = std::clamp(this->state_.mold_index, 0.0f, 6.0f);
  this->state_.unfavorable_hours = std::clamp(this->state_.unfavorable_hours, 0.0f, 48.0f);
  this->publish_state(this->state_.mold_index);
}

float VTTMoldIndex::critical_rh_(float temperature) const {
  const auto &p = MATERIALS[std::min<uint8_t>(this->material_, 3)];
  const float curve = temperature <= 20.0f
                          ? -0.00267f * temperature * temperature * temperature + 0.160f * temperature * temperature -
                                3.13f * temperature + 100.1f
                          : p.rh_min;
  return std::max(curve, p.rh_min);
}

float VTTMoldIndex::maximum_index_(float critical_rh, float humidity) const {
  const auto &p = MATERIALS[std::min<uint8_t>(this->material_, 3)];
  const float ratio = (critical_rh - humidity) / (critical_rh - 100.0f);
  return std::clamp(p.a + p.b * ratio - p.c * ratio * ratio, 0.0f, 6.0f);
}

void VTTMoldIndex::update() {
  if (this->temperature_sensor_ == nullptr || this->humidity_sensor_ == nullptr || !this->temperature_sensor_->has_state() ||
      !this->humidity_sensor_->has_state() || !std::isfinite(this->temperature_sensor_->state) ||
      !std::isfinite(this->humidity_sensor_->state)) {
    this->status_set_warning("input sensor unavailable");
    this->publish_state(NAN);
    return;
  }

  const float temperature = this->temperature_sensor_->state;
  const float humidity = std::clamp(this->humidity_sensor_->state, 0.0f, 100.0f);
  const auto &p = MATERIALS[std::min<uint8_t>(this->material_, 3)];
  const float critical_rh = this->critical_rh_(temperature);
  const bool favorable = temperature > 0.0f && temperature < 50.0f && humidity > critical_rh;

  if (favorable) {
    this->state_.unfavorable_hours = 0.0f;
    const float maximum = this->maximum_index_(critical_rh, humidity);
    const float k1 = this->state_.mold_index < 1.0f ? p.k1_below_1 : p.k1_at_least_1;
    const float k2 = std::max(1.0f - std::exp(2.3f * (this->state_.mold_index - maximum)), 0.0f);
    const float denominator = 168.0f * std::exp(-0.68f * std::log(temperature) - 13.9f * std::log(humidity) + 66.02f);
    if (std::isfinite(denominator) && denominator > 0.0f) {
      this->state_.mold_index += (k1 * k2 / denominator) * HOURS_PER_UPDATE;
    }
  } else {
    this->state_.unfavorable_hours += HOURS_PER_UPDATE;
    // VTT decline: initial decline up to 6 h, pause through 24 h, then a
    // slower decline. k3=0.1 is the recommended conservative coefficient.
    float decline_per_hour = 0.0f;
    if (this->state_.unfavorable_hours <= 6.0f) {
      decline_per_hour = -0.00133f * 0.1f;
    } else if (this->state_.unfavorable_hours > 24.0f) {
      decline_per_hour = -0.000667f * 0.1f;
    }
    this->state_.mold_index += decline_per_hour * HOURS_PER_UPDATE;
  }

  this->state_.mold_index = std::clamp(this->state_.mold_index, 0.0f, 6.0f);
  this->state_.unfavorable_hours = std::clamp(this->state_.unfavorable_hours, 0.0f, 48.0f);
  this->status_clear_warning();
  this->publish_state(this->state_.mold_index);
  if (++this->updates_since_save_ >= 15) {
    this->save_state_();
    this->updates_since_save_ = 0;
  }
  ESP_LOGD(TAG, "T=%.2f RH=%.1f RHcrit=%.1f M=%.3f", temperature, humidity, critical_rh, this->state_.mold_index);
}

void VTTMoldIndex::save_state_() { this->preference_.save(&this->state_); }

void VTTMoldIndex::dump_config() {
  ESP_LOGCONFIG(TAG, "VTT mould index:");
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  ESP_LOGCONFIG(TAG, "  Material sensitivity: %u", this->material_);
}

}  // namespace esphome::vtt_mold_index
