#include "ds2438_ds248x.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::ds2438_ds248x {
static const char *const TAG = "ds2438_ds248x";

bool DS2438DS248xComponent::wait_(uint8_t *status_out) {
  const uint8_t pointer[2] = {0xE1, 0xF0};
  uint8_t status = 1;
  if (this->write(pointer, 2) != i2c::ERROR_OK)
    return false;
  for (uint16_t retry = 0; retry < 1000; retry++) {
    if (this->read(&status, 1) != i2c::ERROR_OK)
      return false;
    if ((status & 0x01) == 0) {
      if (status_out != nullptr)
        *status_out = status;
      return (status & 0x04) == 0;
    }
  }
  return false;
}

bool DS2438DS248xComponent::set_bridge_config_(uint8_t config) {
  const uint8_t command[2] = {0xD2, static_cast<uint8_t>(config | ((~config & 0x0F) << 4))};
  return this->write(command, 2) == i2c::ERROR_OK && this->wait_();
}

bool DS2438DS248xComponent::configure_bridge_() {
  const uint8_t reset = 0xF0;
  // The temperature hub enables the strong pull-up for its conversion. Start
  // this transaction from a known DS2484 configuration with APU enabled.
  if (this->write(&reset, 1) != i2c::ERROR_OK)
    return false;
  delay(1);
  return this->set_bridge_config_(0x01);  // APU = 1.
}

bool DS2438DS248xComponent::reset_wire_() {
  const uint8_t command = 0xB4;
  uint8_t status = 0;
  return this->write(&command, 1) == i2c::ERROR_OK && this->wait_(&status) && (status & 0x02) != 0;
}

bool DS2438DS248xComponent::write_wire_(uint8_t value) {
  const uint8_t command[2] = {0xA5, value};
  return this->write(command, 2) == i2c::ERROR_OK && this->wait_();
}

bool DS2438DS248xComponent::read_wire_(uint8_t *value) {
  const uint8_t command = 0x96;
  const uint8_t pointer[2] = {0xE1, 0xE1};
  *value = 0;
  if (this->write(&command, 1) != i2c::ERROR_OK || !this->wait_() || this->write(pointer, 2) != i2c::ERROR_OK ||
      this->read(value, 1) != i2c::ERROR_OK)
    return false;
  return true;
}

bool DS2438DS248xComponent::select_() {
  if (!this->reset_wire_() || !this->write_wire_(0x55))
    return false;
  for (uint8_t i = 0; i < 8; i++)
    if (!this->write_wire_((this->ds2438_address_ >> (8 * i)) & 0xFF))
      return false;
  return true;
}

bool DS2438DS248xComponent::command_(uint8_t value, bool strong_pullup) {
  if (!this->select_())
    return false;
  // DS2438 can be parasite powered. The DS2484 automatically removes SPU
  // after the following 1-Wire byte has been issued and the conversion ends.
  if (strong_pullup && !this->set_bridge_config_(0x05))
    return false;
  return this->write_wire_(value);
}

bool DS2438DS248xComponent::read_page_(uint8_t *page) {
  // DS2438 requires Recall Memory before every Read Scratchpad operation.
  // Without it, a responding device can return an all-zero scratchpad page.
  if (!this->command_(0xB8) || !this->write_wire_(0x00))
    return false;
  delay(1);
  if (!this->command_(0xBE) || !this->write_wire_(0x00))
    return false;
  for (uint8_t i = 0; i < 9; i++)
    if (!this->read_wire_(&page[i]))
      return false;
  return crc8(page, 8) == page[8];
}

bool DS2438DS248xComponent::write_ds2438_config_(uint8_t value) {
  return this->command_(0x4E) && this->write_wire_(0x00) && this->write_wire_(value);
}

void DS2438DS248xComponent::setup() {
  if ((this->ds2438_address_ & 0xFF) != FAMILY_CODE)
    this->mark_failed();
}

void DS2438DS248xComponent::update() {
  if (this->busy_ || this->is_failed())
    return;
  this->busy_ = true;
  if (!this->configure_bridge_()) {
    this->fail_("DS2484 configuration failed");
    return;
  }
  if (!this->command_(0x44, true)) {
    this->fail_("DS2438 did not acknowledge reset");
    return;
  }
  this->set_timeout("temperature", CONVERSION_TIME_MS, [this] { this->read_temperature_(); });
}

void DS2438DS248xComponent::read_temperature_() {
  uint8_t page[9];
  if (!this->read_page_(page)) {
    this->fail_("temperature read failed");
    return;
  }
  this->original_config_ = page[0];
  this->temperature_ = static_cast<int16_t>((uint16_t(page[2]) << 8) | page[1]) / 256.0f;
  if (!this->write_ds2438_config_(this->original_config_ | CONFIG_AD) || !this->command_(0xB4, true)) {
    this->fail_("VDD conversion failed");
    return;
  }
  this->set_timeout("vdd", CONVERSION_TIME_MS, [this] { this->read_vdd_(); });
}

void DS2438DS248xComponent::read_vdd_() {
  uint8_t page[9];
  if (!this->read_page_(page)) {
    this->fail_("VDD read failed");
    return;
  }
  this->vdd_ = ((uint16_t(page[4]) << 8) | page[3]) / 100.0f;
  if (!this->write_ds2438_config_(this->original_config_ & ~CONFIG_AD) || !this->command_(0xB4, true)) {
    this->fail_("VAD conversion failed");
    return;
  }
  this->set_timeout("vad", CONVERSION_TIME_MS, [this] { this->read_vad_(); });
}

void DS2438DS248xComponent::read_vad_() {
  uint8_t page[9];
  if (!this->read_page_(page)) {
    this->fail_("VAD read failed");
    return;
  }
  this->write_ds2438_config_(this->original_config_);
  const float vad = ((uint16_t(page[4]) << 8) | page[3]) / 100.0f;
  const float raw = ((vad / this->vdd_) - 0.16f) / 0.0062f;
  const float humidity = raw / (1.0546f - 0.00216f * this->temperature_);
  ESP_LOGD(TAG, "0x%016llX: T=%.2f VDD=%.2f VAD=%.2f RH=%.1f", this->ds2438_address_, this->temperature_,
           this->vdd_, vad, humidity);
  if (!std::isfinite(humidity) || this->vdd_ < 4.0f || this->vdd_ > 5.8f) {
    this->fail_("measurement outside HIH4031 supply range");
    return;
  }
  this->status_clear_warning();
  this->humidity_sensor_->publish_state(humidity);
  if (this->humidity_raw_sensor_ != nullptr) this->humidity_raw_sensor_->publish_state(raw);
  if (this->temperature_sensor_ != nullptr) this->temperature_sensor_->publish_state(this->temperature_);
  if (this->vad_sensor_ != nullptr) this->vad_sensor_->publish_state(vad);
  if (this->vdd_sensor_ != nullptr) this->vdd_sensor_->publish_state(this->vdd_);
  this->busy_ = false;
}

void DS2438DS248xComponent::fail_(const char *message) {
  ESP_LOGW(TAG, "0x%016llX: %s", this->ds2438_address_, message);
  this->status_set_warning(message);
  this->publish_nan_();
  this->busy_ = false;
}

void DS2438DS248xComponent::publish_nan_() {
  this->humidity_sensor_->publish_state(NAN);
  if (this->humidity_raw_sensor_ != nullptr) this->humidity_raw_sensor_->publish_state(NAN);
  if (this->temperature_sensor_ != nullptr) this->temperature_sensor_->publish_state(NAN);
  if (this->vad_sensor_ != nullptr) this->vad_sensor_->publish_state(NAN);
  if (this->vdd_sensor_ != nullptr) this->vdd_sensor_->publish_state(NAN);
}

void DS2438DS248xComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DS2438 via DS248x:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  DS2438 address: 0x%016llX", this->ds2438_address_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::ds2438_ds248x
