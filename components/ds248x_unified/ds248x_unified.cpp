#include "ds248x_unified.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::ds248x_unified {
static const char *const TAG = "ds248x_unified";

void DS2438Sensor::setup() {
  const uint32_t preference_key = static_cast<uint32_t>(this->address_ ^ (this->address_ >> 32) ^ 0xD52438A1U);
  this->invalid_readings_preference_ = global_preferences->make_preference<uint32_t>(preference_key);
  if (!this->invalid_readings_preference_.load(&this->invalid_reading_count_)) this->invalid_reading_count_ = 0;
  if (this->invalid_readings_ != nullptr) this->invalid_readings_->publish_state(this->invalid_reading_count_);
}

bool DS2438Sensor::is_valid_reading(float temperature, float vdd, float vad) const {
  // A VAD value near VDD is not physically possible for the supported HIH
  // sensors. It indicates that DS2438 returned the preceding VDD conversion.
  if (!std::isfinite(temperature) || !std::isfinite(vdd) || !std::isfinite(vad) || vad <= 0.0f || vad >= vdd * 0.90f)
    return false;
  const float ratio = vad / vdd;
  float raw = NAN;
  float compensation = NAN;
  float minimum_vdd = 4.0f;
  float maximum_vdd = 5.8f;
  switch (this->humidity_model_) {
    case 0:  // Honeywell HIH-4030/4031
    case 1:  // Honeywell HIH-3600 series
      raw = (ratio - 0.16f) / 0.0062f;
      compensation = 1.0546f - 0.00216f * temperature;
      break;
    case 2:  // Honeywell HIH-4000 series
      raw = (ratio - 0.16f) / 0.0062f;
      compensation = 1.0305f + 0.000044f * temperature - 0.0000011f * temperature * temperature;
      break;
    case 3:  // Honeywell HIH-5030/5031 series
      raw = (ratio - 0.1515f) / 0.00636f;
      compensation = 1.0546f - 0.00216f * temperature;
      minimum_vdd = 2.7f;
      maximum_vdd = 5.5f;
      break;
    default:
      return false;
  }
  const float humidity = raw / compensation;
  return std::isfinite(humidity) && std::isfinite(raw) && vdd >= minimum_vdd && vdd <= maximum_vdd && humidity >= 0.0f && humidity <= 100.0f;
}

bool DS2438Sensor::publish(float temperature, float vdd, float vad) {
  if (!this->is_valid_reading(temperature, vdd, vad)) return false;
  const float ratio = vad / vdd;
  float raw = NAN;
  float compensation = NAN;
  switch (this->humidity_model_) {
    case 0:
    case 1:
      raw = (ratio - 0.16f) / 0.0062f;
      compensation = 1.0546f - 0.00216f * temperature;
      break;
    case 2:
      raw = (ratio - 0.16f) / 0.0062f;
      compensation = 1.0305f + 0.000044f * temperature - 0.0000011f * temperature * temperature;
      break;
    case 3:
      raw = (ratio - 0.1515f) / 0.00636f;
      compensation = 1.0546f - 0.00216f * temperature;
      break;
    default:
      return false;
  }
  const float humidity = raw / compensation;
  this->humidity_->publish_state(humidity);
  if (this->raw_ != nullptr) this->raw_->publish_state(raw);
  if (this->temperature_ != nullptr) this->temperature_->publish_state(temperature);
  if (this->vad_ != nullptr) this->vad_->publish_state(vad);
  if (this->vdd_ != nullptr) this->vdd_->publish_state(vdd);
  return true;
}
void DS2438Sensor::report_invalid_reading() {
  this->invalid_reading_count_++;
  this->invalid_readings_preference_.save(&this->invalid_reading_count_);
  if (this->invalid_readings_ != nullptr) this->invalid_readings_->publish_state(this->invalid_reading_count_);
}
void DS2438Sensor::publish_nan() {
  this->humidity_->publish_state(NAN);
  if (this->raw_ != nullptr) this->raw_->publish_state(NAN);
  if (this->temperature_ != nullptr) this->temperature_->publish_state(NAN);
  if (this->vad_ != nullptr) this->vad_->publish_state(NAN);
  if (this->vdd_ != nullptr) this->vdd_->publish_state(NAN);
}

void DS248xUnifiedComponent::setup() {
  for (auto *sensor : this->ds2438_) sensor->setup();
  this->set_config_(this->active_pullup_ ? 0x01 : 0x00);
}
void DS248xUnifiedComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DS248x unified hub:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  DS18B20: %u, DS2438: %u", this->ds18b20_.size(), this->ds2438_.size());
}
bool DS248xUnifiedComponent::wait_(uint8_t *out) {
  const uint8_t pointer[2] = {0xE1, 0xF0};
  uint8_t status = 1;
  if (this->write(pointer, 2) != i2c::ERROR_OK) return false;
  for (uint16_t i = 0; i < 1000; i++) {
    if (this->read(&status, 1) != i2c::ERROR_OK) return false;
    if ((status & 0x01) == 0) { if (out != nullptr) *out = status; return (status & 0x04) == 0; }
  }
  return false;
}
bool DS248xUnifiedComponent::set_config_(uint8_t config) {
  const uint8_t data[2] = {0xD2, static_cast<uint8_t>(config | ((~config & 0x0F) << 4))};
  return this->write(data, 2) == i2c::ERROR_OK && this->wait_();
}
bool DS248xUnifiedComponent::reset_wire_() {
  const uint8_t command = 0xB4; uint8_t status = 0;
  return this->write(&command, 1) == i2c::ERROR_OK && this->wait_(&status) && (status & 0x02) != 0;
}
bool DS248xUnifiedComponent::write_wire_(uint8_t value) {
  const uint8_t data[2] = {0xA5, value}; return this->write(data, 2) == i2c::ERROR_OK && this->wait_();
}
bool DS248xUnifiedComponent::read_wire_(uint8_t *value) {
  const uint8_t command = 0x96, pointer[2] = {0xE1, 0xE1};
  return this->write(&command, 1) == i2c::ERROR_OK && this->wait_() && this->write(pointer, 2) == i2c::ERROR_OK && this->read(value, 1) == i2c::ERROR_OK;
}
bool DS248xUnifiedComponent::select_(uint64_t address) {
  if (!this->reset_wire_() || !this->write_wire_(0x55)) return false;
  for (uint8_t i = 0; i < 8; i++) if (!this->write_wire_((address >> (8 * i)) & 0xFF)) return false;
  return true;
}
bool DS248xUnifiedComponent::command_(uint64_t address, uint8_t command, bool strong) {
  if (!this->select_(address)) return false;
  if (strong && !this->set_config_(0x05)) return false;
  return this->write_wire_(command);
}
bool DS248xUnifiedComponent::read_ds2438_page_(uint64_t address) {
  if (!this->command_(address, 0xB8) || !this->write_wire_(0x00)) return false;
  delay(1);
  if (!this->command_(address, 0xBE) || !this->write_wire_(0x00)) return false;
  for (uint8_t i = 0; i < 9; i++) if (!this->read_wire_(&this->page_[i])) return false;
  return crc8(this->page_, 8) == this->page_[8];
}
bool DS248xUnifiedComponent::write_ds2438_config_(uint64_t address, uint8_t config) {
  return this->command_(address, 0x4E) && this->write_wire_(0x00) && this->write_wire_(config);
}
bool DS248xUnifiedComponent::read_ds18_(DS18B20Sensor *sensor) {
  uint8_t scratch[9];
  if (!this->command_(sensor->address(), 0xBE)) return false;
  for (auto &value : scratch) if (!this->read_wire_(&value)) return false;
  if (crc8(scratch, 8) != scratch[8]) return false;
  const int16_t raw = (int16_t(scratch[1]) << 8) | scratch[0];
  sensor->publish_state(raw / 16.0f); return true;
}
void DS248xUnifiedComponent::update() {
  if (!this->set_config_(this->active_pullup_ ? 0x01 : 0x00) || !this->reset_wire_()) { this->status_set_warning(); return; }
  if (!this->write_wire_(0xCC)) return;
  if (this->strong_pullup_ && !this->set_config_(0x05)) return;
  if (!this->write_wire_(0x44)) return;
  this->index_ = 0;
  this->set_timeout("ds18_convert", 750, [this] { this->read_next_ds18_(); });
}
void DS248xUnifiedComponent::read_next_ds18_() {
  if (this->index_ >= this->ds18b20_.size()) { this->index_ = 0; this->start_next_ds2438_(); return; }
  auto *sensor = this->ds18b20_[this->index_++];
  if (!this->read_ds18_(sensor)) sensor->publish_state(NAN);
  this->set_timeout("next_ds18", 50, [this] { this->read_next_ds18_(); });
}
void DS248xUnifiedComponent::start_next_ds2438_() {
  if (this->index_ >= this->ds2438_.size()) { this->status_clear_warning(); return; }
  this->vad_retry_pending_ = false;
  auto *sensor = this->ds2438_[this->index_];
  if (!this->command_(sensor->address(), 0x44, true)) { this->fail_ds2438_("temperature conversion failed"); return; }
  this->set_timeout("ds2438_temperature", 15, [this] { this->read_ds2438_temperature_(); });
}
void DS248xUnifiedComponent::read_ds2438_temperature_() {
  auto *sensor = this->ds2438_[this->index_];
  if (!this->read_ds2438_page_(sensor->address())) { this->fail_ds2438_("temperature read failed"); return; }
  this->temperature_ = static_cast<int16_t>((uint16_t(this->page_[2]) << 8) | this->page_[1]) / 256.0f;
  if (!this->write_ds2438_config_(sensor->address(), this->page_[0] | 0x08) || !this->command_(sensor->address(), 0xB4, true)) { this->fail_ds2438_("VDD conversion failed"); return; }
  this->set_timeout("ds2438_vdd", 15, [this] { this->read_ds2438_vdd_(); });
}
void DS248xUnifiedComponent::read_ds2438_vdd_() {
  auto *sensor = this->ds2438_[this->index_];
  if (!this->read_ds2438_page_(sensor->address())) { this->fail_ds2438_("VDD read failed"); return; }
  this->vdd_ = ((uint16_t(this->page_[4]) << 8) | this->page_[3]) / 100.0f;
  if (!this->write_ds2438_config_(sensor->address(), this->page_[0] & ~0x08) || !this->command_(sensor->address(), 0xB4, true)) { this->fail_ds2438_("VAD conversion failed"); return; }
  this->set_timeout("ds2438_vad", 15, [this] { this->read_ds2438_vad_(); });
}
void DS248xUnifiedComponent::read_ds2438_vad_() {
  auto *sensor = this->ds2438_[this->index_];
  if (!this->read_ds2438_page_(sensor->address())) { this->fail_ds2438_("VAD read failed"); return; }
  const float vad = ((uint16_t(this->page_[4]) << 8) | this->page_[3]) / 100.0f;
  if (!sensor->is_valid_reading(this->temperature_, this->vdd_, vad)) {
    if (!this->vad_retry_pending_) {
      this->vad_retry_pending_ = true;
      sensor->report_invalid_reading();
      ESP_LOGW(TAG, "0x%016llX: implausible VAD=%.2f V (VDD=%.2f V), retrying conversion", sensor->address(), vad, this->vdd_);
      if (!this->command_(sensor->address(), 0xB4, true)) { this->fail_ds2438_("VAD retry conversion failed"); return; }
      this->set_timeout("ds2438_vad_retry", 30, [this] { this->read_ds2438_vad_(); });
      return;
    }
    ESP_LOGW(TAG, "0x%016llX: implausible VAD remains after retry; keeping last valid values", sensor->address());
    this->vad_retry_pending_ = false;
    this->index_++;
    this->set_timeout("next_ds2438", 20, [this] { this->start_next_ds2438_(); });
    return;
  }
  this->vad_retry_pending_ = false;
  sensor->publish(this->temperature_, this->vdd_, vad);
  ESP_LOGD(TAG, "0x%016llX: T=%.2f VDD=%.2f VAD=%.2f", sensor->address(), this->temperature_, this->vdd_, vad);
  this->index_++; this->set_timeout("next_ds2438", 20, [this] { this->start_next_ds2438_(); });
}
void DS248xUnifiedComponent::fail_ds2438_(const char *message) {
  ESP_LOGW(TAG, "%s", message); this->ds2438_[this->index_]->publish_nan(); this->index_++;
  this->set_timeout("next_ds2438", 20, [this] { this->start_next_ds2438_(); });
}

}  // namespace esphome::ds248x_unified
