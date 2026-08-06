#include "ds2438.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::ds2438 {

static const char *const TAG = "ds2438.sensor";

void DS2438Component::setup() {
  if (!this->check_address_or_index_())
    return;

  if ((this->address_ & 0xFF) != FAMILY_CODE) {
    ESP_LOGE(TAG, "Address %s is not a DS2438 (expected family 0x26)", this->get_address_name().c_str());
    this->mark_failed();
  }
}

void DS2438Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DS2438 sensor module:");
  if (this->address_ == 0) {
    ESP_LOGE(TAG, "  Unable to select an address");
    return;
  }
  LOG_ONE_WIRE_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Humidity model: %s", this->humidity_model_name_());
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Raw humidity", this->humidity_raw_sensor_);
  LOG_SENSOR("  ", "DS2438 temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "VAD", this->vad_sensor_);
  LOG_SENSOR("  ", "VDD", this->vdd_sensor_);
}

void DS2438Component::update() {
  if (this->address_ == 0 || this->measurement_in_progress_)
    return;

  this->measurement_in_progress_ = true;
  this->config_captured_ = false;
  this->status_clear_warning();

  if (!this->start_conversion_(COMMAND_CONVERT_T)) {
    this->fail_measurement_("temperature conversion bus reset failed");
    return;
  }

  this->set_timeout("temperature", CONVERSION_TIME_MS, [this] { this->read_temperature_and_start_vdd_(); });
}

bool DS2438Component::start_conversion_(uint8_t command) { return this->send_command_(command); }

bool DS2438Component::read_page_zero_(uint8_t *data) {
  if (!this->send_command_(COMMAND_READ_SCRATCHPAD))
    return false;

  this->bus_->write8(PAGE_ZERO);
  for (uint8_t i = 0; i < 9; i++)
    data[i] = this->bus_->read8();

  if (crc8(data, 8) != data[8]) {
    ESP_LOGW(TAG, "%s: page 0 CRC mismatch (expected %02X, got %02X)", this->get_address_name().c_str(),
             crc8(data, 8), data[8]);
    return false;
  }
  return true;
}

bool DS2438Component::write_config_(uint8_t config) {
  if (!this->send_command_(COMMAND_WRITE_SCRATCHPAD))
    return false;

  this->bus_->write8(PAGE_ZERO);
  this->bus_->write8(config);
  return true;
}

void DS2438Component::read_temperature_and_start_vdd_() {
  uint8_t page[9];
  if (!this->read_page_zero_(page)) {
    this->fail_measurement_("temperature read failed");
    return;
  }

  this->original_config_ = page[0];
  this->config_captured_ = true;
  const int16_t raw_temperature = static_cast<int16_t>((uint16_t(page[2]) << 8) | page[1]);
  this->temperature_ = raw_temperature / 256.0f;

  if (!this->write_config_(this->original_config_ | CONFIG_AD) || !this->start_conversion_(COMMAND_CONVERT_V)) {
    this->fail_measurement_("VDD conversion setup failed");
    return;
  }

  this->set_timeout("vdd", CONVERSION_TIME_MS, [this] { this->read_vdd_and_start_vad_(); });
}

void DS2438Component::read_vdd_and_start_vad_() {
  uint8_t page[9];
  if (!this->read_page_zero_(page)) {
    this->fail_measurement_("VDD read failed");
    return;
  }

  const uint16_t raw_voltage = (uint16_t(page[4]) << 8) | page[3];
  this->vdd_ = raw_voltage / 100.0f;

  if (!this->write_config_(this->original_config_ & ~CONFIG_AD) || !this->start_conversion_(COMMAND_CONVERT_V)) {
    this->fail_measurement_("VAD conversion setup failed");
    return;
  }

  this->set_timeout("vad", CONVERSION_TIME_MS, [this] { this->read_vad_and_publish_(); });
}

bool DS2438Component::calculate_humidity_(float vad, float *humidity_raw, float *humidity) const {
  switch (this->humidity_model_) {
    case HumidityModel::HIH4031: {
      if (this->vdd_ < 4.0f || this->vdd_ > 5.8f)
        return false;
      *humidity_raw = ((vad / this->vdd_) - 0.16f) / 0.0062f;
      const float compensation = 1.0546f - 0.00216f * this->temperature_;
      *humidity = *humidity_raw / compensation;
      return true;
    }
  }
  return false;
}

const char *DS2438Component::humidity_model_name_() const {
  switch (this->humidity_model_) {
    case HumidityModel::HIH4031:
      return "HIH4031";
  }
  return "UNKNOWN";
}

void DS2438Component::read_vad_and_publish_() {
  uint8_t page[9];
  if (!this->read_page_zero_(page)) {
    this->fail_measurement_("VAD read failed");
    return;
  }

  // Restore the previous A/D input selection in volatile scratchpad only.
  if (!this->write_config_(this->original_config_))
    ESP_LOGW(TAG, "%s: could not restore DS2438 configuration", this->get_address_name().c_str());
  this->config_captured_ = false;

  const uint16_t raw_voltage = (uint16_t(page[4]) << 8) | page[3];
  const float vad = raw_voltage / 100.0f;

  if (!std::isfinite(this->temperature_) || this->temperature_ < -40.0f || this->temperature_ > 85.0f ||
      !std::isfinite(this->vdd_) || !std::isfinite(vad) || vad < 0.0f || vad > this->vdd_) {
    this->fail_measurement_("measurement outside sensor operating range");
    return;
  }

  float humidity_raw;
  float humidity;
  if (!this->calculate_humidity_(vad, &humidity_raw, &humidity)) {
    this->fail_measurement_("measurement outside configured humidity model range");
    return;
  }

  ESP_LOGD(TAG, "%s: model=%s T=%.2f°C VDD=%.2fV VAD=%.2fV RH(raw)=%.1f%% RH=%.1f%%",
           this->get_address_name().c_str(), this->humidity_model_name_(), this->temperature_, this->vdd_, vad,
           humidity_raw, humidity);

  this->humidity_sensor_->publish_state(humidity);
  if (this->humidity_raw_sensor_ != nullptr)
    this->humidity_raw_sensor_->publish_state(humidity_raw);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(this->temperature_);
  if (this->vad_sensor_ != nullptr)
    this->vad_sensor_->publish_state(vad);
  if (this->vdd_sensor_ != nullptr)
    this->vdd_sensor_->publish_state(this->vdd_);

  this->measurement_in_progress_ = false;
}

void DS2438Component::fail_measurement_(const char *message) {
  ESP_LOGW(TAG, "%s: %s", this->get_address_name().c_str(), message);
  if (this->config_captured_) {
    if (!this->write_config_(this->original_config_))
      ESP_LOGW(TAG, "%s: could not restore DS2438 configuration after error", this->get_address_name().c_str());
    this->config_captured_ = false;
  }
  this->status_set_warning(message);
  this->publish_nan_();
  this->measurement_in_progress_ = false;
}

void DS2438Component::publish_nan_() {
  this->humidity_sensor_->publish_state(NAN);
  if (this->humidity_raw_sensor_ != nullptr)
    this->humidity_raw_sensor_->publish_state(NAN);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(NAN);
  if (this->vad_sensor_ != nullptr)
    this->vad_sensor_->publish_state(NAN);
  if (this->vdd_sensor_ != nullptr)
    this->vdd_sensor_->publish_state(NAN);
}

}  // namespace esphome::ds2438
