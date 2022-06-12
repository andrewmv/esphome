#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace whynter {

// Supported temperature range in celcius
const float WHYNTER_TEMP_MIN = 16.67;  // 62 Farhenheit
const float WHYNTER_TEMP_MAX = 30.0;   // 86 Farhenheit

class WhynterClimate : public climate_ir::ClimateIR {
 public:
  WhynterClimate()
      // clang-format off
      : climate_ir::ClimateIR(WHYNTER_TEMP_MIN,
                              WHYNTER_TEMP_MAX,
                              1.0f,   // Step
                              true,   // Supports Dry
                              true,   // Supports Fan
                              {climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH}, // Supported Fan Modes
                              {} // Supported Swing Modes
                              ) {}
  // clang-format on
  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override {
    climate_ir::ClimateIR::control(call);
  }

  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  void setup() override;
  void dump_config() override;
  void send_packet_(uint8_t const *message, uint8_t length);
  void send_transmission_(uint8_t const *message, uint8_t length);
  uint8_t compile_hvac_byte_();
  uint8_t compile_set_point_byte_();
  bool send_swing_cmd_{false};
};

}  // namespace whynter
}  // namespace esphome
