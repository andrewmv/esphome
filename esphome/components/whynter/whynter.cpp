#include "whynter.h"
#include "esphome/core/log.h"

namespace esphome {
namespace whynter {

static const char *const TAG = "whynter.climate";

static const int WHYNTER_CARRIER_FREQ = 38000;
static const uint8_t WHYNTER_PACKET_LENGTH = 4;

// clang-format off

// Pulse timings
static const int WHYNTER_INIT_HIGH_US =   8500;
static const int WHYNTER_INIT_LOW_US =    4400;
static const int WHYNTER_ONE_LOW_US =     550;
static const int WHYNTER_ZERO_LOW_US =    1550;
static const int WHYNTER_HIGH_US =        550;

// Packet Type - Byte 0
static const uint8_t WHYNTER_PKT_HEADER =      0x48;

// Power Modes - State Byte 2, Bit 4
static const uint8_t WHYNTER_POWER_ON =        0xA8;
static const uint8_t WHYNTER_POWER_OFF =       0x32;

// Fan Speeds - State Byte 1, Bits 1,2,3
static const uint8_t WHYNTER_FAN_HIGH =        0x08;
static const uint8_t WHYNTER_FAN_MED =         0x04;
static const uint8_t WHYNTER_FAN_MIN =         0x02;

// HVAC Modes - State Byte 1, Bits 4,5,6,7
static const uint8_t WHYNTER_COOL =            0x10;
static const uint8_t WHYNTER_DRY =             0x40;
static const uint8_t WHYNTER_FAN =             0x80;

// clang-format on

void WhynterClimate::setup() {
#ifdef USE_ESP8266
  ESP_LOGW(TAG, "This component is not reliable on the ESP8266 platform - an ESP32 is highly recommended");
#endif
  // If a sensor has been configured, use it report current temp in the frontend
  if (this->sensor_) {
    this->sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;
      // current temperature changed, publish state
      this->publish_state();
    });
    this->current_temperature = this->sensor_->state;
  } else {
    this->current_temperature = NAN;
  }

  // restore set points
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    // restore from defaults
    this->mode = climate::CLIMATE_MODE_OFF;
    // initialize target temperature to some value so that it's not NAN
    this->target_temperature =
        roundf(clamp(this->current_temperature, this->minimum_temperature_, this->maximum_temperature_));
    this->fan_mode = climate::CLIMATE_FAN_LOW;
    this->preset = climate::CLIMATE_PRESET_NONE;
  }

  // Never send nan to HA
  if (std::isnan(this->target_temperature)) {
    this->target_temperature = 24;
  }
}

void WhynterClimate::dump_config() {
  LOG_CLIMATE("", "IR Climate", this);
  ESP_LOGCONFIG(TAG, "  Min. Temperature: %.1f°C", this->minimum_temperature_);
  ESP_LOGCONFIG(TAG, "  Max. Temperature: %.1f°C", this->maximum_temperature_);
  ESP_LOGCONFIG(TAG, "  Supports HEAT: %s", YESNO(this->supports_heat_));
  ESP_LOGCONFIG(TAG, "  Supports COOL: %s", YESNO(this->supports_cool_));
}

uint8_t WhynterClimate::compile_hvac_byte_() {
  uint8_t hvac_byte = 0x00;
  // HVAC Mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_FAN_ONLY:
      hvac_byte |= WHYNTER_FAN;
      break;
    case climate::CLIMATE_MODE_DRY:
      hvac_byte |= WHYNTER_DRY;
      break;
    default:
    case climate::CLIMATE_MODE_COOL:
      hvac_byte |= WHYNTER_COOL;
      break;
  }

  // Fan Mode
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_HIGH:
      hvac_byte |= WHYNTER_FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      hvac_byte |= WHYNTER_FAN_MED;
      break;
    default:
    case climate::CLIMATE_FAN_LOW:
      hvac_byte |= WHYNTER_FAN_MIN;
      break;
  }

  return hvac_byte;
}

uint8_t WhynterClimate::compile_set_point_byte_() {
  // ESPHome always presents this number in celcius.
  float target_celcius = clamp<float>(this->target_temperature, WHYNTER_TEMP_MIN, WHYNTER_TEMP_MAX);
  uint8_t target_farhenheit = roundf(target_celcius * 1.8 + 32.0);
  return (target_farhenheit);
}

void WhynterClimate::transmit_state() {
  uint8_t remote_state[WHYNTER_PACKET_LENGTH] = {0};

  remote_state[0] = WHYNTER_PKT_HEADER;
  remote_state[1] = this->compile_hvac_byte_();

  // Power
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    remote_state[2] = WHYNTER_POWER_OFF;
  } else {
    remote_state[2] = WHYNTER_POWER_ON;
  }

  // Temperature Set Point
  remote_state[3] = compile_set_point_byte_();

  send_transmission_(remote_state, WHYNTER_PACKET_LENGTH);
}

void WhynterClimate::send_transmission_(uint8_t const *message, uint8_t length) {
  ESP_LOGV(TAG, "Sending whynter code: 0x%2X %2X %2X %2X", message[0], message[1], message[2], message[3]);
  this->send_packet_(message, WHYNTER_PACKET_LENGTH);
}

void WhynterClimate::send_packet_(uint8_t const *message, uint8_t length) {
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(WHYNTER_CARRIER_FREQ);

  // Start signal
  data->mark(WHYNTER_INIT_HIGH_US);
  data->space(WHYNTER_INIT_LOW_US);

  // Data
  for (uint8_t msgbyte = 0; msgbyte < length; msgbyte++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      data->mark(WHYNTER_HIGH_US);
      if (message[msgbyte] & (0x01 << bit)) {  // shift bits out left to right
        data->space(WHYNTER_ZERO_LOW_US);
      } else {
        data->space(WHYNTER_ONE_LOW_US);
      }
    }
  }
  // End the last bit
  data->mark(WHYNTER_HIGH_US);

  // Stop signal
  data->space(WHYNTER_INIT_LOW_US);

  transmit.perform();
}

}  // namespace whynter
}  // namespace esphome
