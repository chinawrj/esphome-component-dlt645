#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#if defined(USE_ESP32) || defined(USE_ESP_IDF)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#endif

#include <vector>
#include <string>
#include <cstring>
#include <functional>
#include <algorithm>
#include <cstdio>
#include <cmath>

namespace esphome {
namespace dlt645_component {

// FreeRTOS task configuration constants
constexpr uint32_t DLT645_TASK_STACK_SIZE = 4096;  // 4KB stack size
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
constexpr UBaseType_t DLT645_TASK_PRIORITY = 5;    // Medium priority
#endif
constexpr uint32_t DLT645_TRIGGER_INTERVAL_MS = 5000;  // 5 second interval

// DL/T 645-2007 data identifier enumeration definitions
enum class DLT645_DATA_IDENTIFIER : uint32_t {
  DEVICE_ADDRESS      = 0x04000401,  // Device address query
  ACTIVE_POWER_TOTAL  = 0x02030000,  // Total active power
  ENERGY_ACTIVE_TOTAL = 0x00010000,  // Forward active total energy
  VOLTAGE_A_PHASE     = 0x02010100,  // Phase A voltage
  CURRENT_A_PHASE     = 0x02020100,  // Phase A current
  POWER_FACTOR_TOTAL  = 0x02060000,  // Total power factor
  FREQUENCY           = 0x02800002,  // Grid frequency
  ENERGY_REVERSE_TOTAL = 0x00020000, // Reverse active total energy
  DATETIME            = 0x04000101,  // Date and time
  TIME_HMS            = 0x04000102   // Hours, minutes, seconds
};

// Event Group event bit definitions - Event bits corresponding to DL/T 645-2007 data identifiers
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
const EventBits_t EVENT_GENERAL = BIT0;            // BIT0: Keep original general event
const EventBits_t EVENT_DI_DEVICE_ADDRESS = BIT1;  // BIT1: Device address query
const EventBits_t EVENT_DI_ACTIVE_POWER_TOTAL = BIT2;  // BIT2: Total power
const EventBits_t EVENT_DI_ENERGY_ACTIVE_TOTAL = BIT3; // BIT3: Total energy
const EventBits_t EVENT_DI_VOLTAGE_A_PHASE = BIT4;     // BIT4: Phase A voltage
const EventBits_t EVENT_DI_CURRENT_A_PHASE = BIT5;     // BIT5: Phase A current
const EventBits_t EVENT_DI_POWER_FACTOR_TOTAL = BIT6;  // BIT6: Power factor
const EventBits_t EVENT_DI_FREQUENCY = BIT7;           // BIT7: Frequency
const EventBits_t EVENT_DI_ENERGY_REVERSE_TOTAL = BIT8; // BIT8: Reverse total energy
const EventBits_t EVENT_DI_DATETIME = BIT9;            // BIT9: Date and time
const EventBits_t EVENT_DI_TIME_HMS = BIT10;           // BIT10: Hours, minutes, seconds

// Mask for all DL/T 645 event bits (BIT1-BIT10)
const EventBits_t ALL_DLT645_EVENTS = EVENT_DI_DEVICE_ADDRESS | EVENT_DI_ACTIVE_POWER_TOTAL | 
                                      EVENT_DI_ENERGY_ACTIVE_TOTAL | EVENT_DI_VOLTAGE_A_PHASE | 
                                      EVENT_DI_CURRENT_A_PHASE | EVENT_DI_POWER_FACTOR_TOTAL | 
                                      EVENT_DI_FREQUENCY | EVENT_DI_ENERGY_REVERSE_TOTAL | 
                                      EVENT_DI_DATETIME | EVENT_DI_TIME_HMS;

// Mask for all event bits (including original EVENT_GENERAL)
const EventBits_t ALL_EVENTS = EVENT_GENERAL | ALL_DLT645_EVENTS;
#endif

class DLT645Component : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  
  void set_magic_number(uint32_t magic_number) { this->magic_number_ = magic_number; }
  
  // Set total power query ratio control parameter
  void set_power_ratio(int ratio) { this->power_ratio_ = ratio; }
  
  // Original generic callback function (for backward compatibility)
  void add_on_hello_world_callback(std::function<void(uint32_t)> &&callback) {
    this->hello_world_callback_.add(std::move(callback));
  }
  
  // DL/T 645-2007 data identifier independent event callback functions
  void add_on_device_address_callback(std::function<void(uint32_t)> &&callback) {
    this->device_address_callback_.add(std::move(callback));
  }
  void add_on_active_power_callback(std::function<void(uint32_t, float)> &&callback) {
    this->active_power_callback_.add(std::move(callback));
  }
  void add_on_warning_reverse_power_callback(std::function<void(uint32_t, float)> &&callback) {
    this->warning_reverse_power_callback_.add(std::move(callback));
  }
  void add_on_energy_active_callback(std::function<void(uint32_t, float)> &&callback) {
    this->energy_active_callback_.add(std::move(callback));
  }
  void add_on_voltage_a_callback(std::function<void(uint32_t, float)> &&callback) {
    this->voltage_a_callback_.add(std::move(callback));
  }
  void add_on_current_a_callback(std::function<void(uint32_t, float)> &&callback) {
    this->current_a_callback_.add(std::move(callback));
  }
  void add_on_power_factor_callback(std::function<void(uint32_t, float)> &&callback) {
    this->power_factor_callback_.add(std::move(callback));
  }
  void add_on_frequency_callback(std::function<void(uint32_t, float)> &&callback) {
    this->frequency_callback_.add(std::move(callback));
  }
  void add_on_energy_reverse_callback(std::function<void(uint32_t, float)> &&callback) {
    this->energy_reverse_callback_.add(std::move(callback));
  }
  void add_on_datetime_callback(std::function<void(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)> &&callback) {
    this->datetime_callback_.add(std::move(callback));
  }
  void add_on_time_hms_callback(std::function<void(uint32_t, uint32_t, uint32_t, uint32_t)> &&callback) {
    this->time_hms_callback_.add(std::move(callback));
  }

 protected:
  void trigger_hello_world_event();
  
  // FreeRTOS task related methods
  bool create_dlt645_task();
  void destroy_dlt645_task();
  static void dlt645_task_func(void* parameter);
  void process_dlt645_events();
  
  // === DL/T 645-2007 UART communication related methods ===
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
  bool init_dlt645_uart();                           // UART initialization
  void deinit_dlt645_uart();                         // UART deinitialization
  bool send_dlt645_frame(const std::vector<uint8_t>& frame, uint32_t timeout_ms = 1000);  // Send frame (with configurable timeout)
  void process_uart_data();                          // Process UART data
  void check_and_parse_dlt645_frame();              // Check and parse DL/T 645 frame
  
  // Dynamic baud rate switching function (only for device discovery command timeout)
  bool change_uart_baud_rate(int new_baud_rate);     // Switch UART baud rate
  void cycle_to_next_baud_rate();                    // Cycle to next baud rate
  
  // DL/T 645-2007 frame building and data processing helper functions
  std::vector<uint8_t> build_dlt645_read_frame(const std::vector<uint8_t>& address, uint32_t data_identifier);
  void scramble_dlt645_data(std::vector<uint8_t>& data);     // Data scrambling (+0x33)
  void unscramble_dlt645_data(std::vector<uint8_t>& data);   // Data descrambling (-0x33)
  float bcd_to_float(const std::vector<uint8_t>& bcd_data, int decimal_places);  // BCD to float
  float bcd_to_float_with_sign(const std::vector<uint8_t>& bcd_data, int decimal_places);  // BCD to float（）
  
  // DL/T 645-2007 
  bool discover_meter_address();                             // 
  bool query_active_power_total();                           // 
  void parse_dlt645_data_by_identifier(uint32_t data_identifier, const std::vector<uint8_t>& data_field);  // DI
  
  // 
  size_t get_next_event_index(size_t current_index, size_t max_events);  // 
#endif
  
  uint32_t magic_number_{42};
  
  // Query ratio control
  int power_ratio_{10};                        // 10:1，101
  int total_power_query_count_{0};            // 
  size_t last_non_power_query_index_{2};      // ""（2，）
  
  // （）
  CallbackManager<void(uint32_t)> hello_world_callback_;
  
  // DL/T 645-2007 
  CallbackManager<void(uint32_t)> device_address_callback_;    // 
  CallbackManager<void(uint32_t, float)> active_power_callback_;      // 
  CallbackManager<void(uint32_t, float)> warning_reverse_power_callback_;  // （>=0<0）
  CallbackManager<void(uint32_t, float)> energy_active_callback_;     // 
  CallbackManager<void(uint32_t, float)> voltage_a_callback_;         // A
  CallbackManager<void(uint32_t, float)> current_a_callback_;         // A
  CallbackManager<void(uint32_t, float)> power_factor_callback_;      // 
  CallbackManager<void(uint32_t, float)> frequency_callback_;         // 
  CallbackManager<void(uint32_t, float)> energy_reverse_callback_;    // 
  CallbackManager<void(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)> datetime_callback_;          //  (DI, year, month, day, weekday)
  CallbackManager<void(uint32_t, uint32_t, uint32_t, uint32_t)> time_hms_callback_;          //  (DI, hour, minute, second)
  
  // FreeRTOS
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
  TaskHandle_t dlt645_task_handle_{nullptr};
  EventGroupHandle_t event_group_{nullptr};
  bool task_running_{false};
#endif

  // === DL/T 645-2007 UART ===
  
  // UART（YAML）
  static constexpr int DLT645_TX_PIN = 1;        // GPIO1 - 
  static constexpr int DLT645_RX_PIN = 2;        // GPIO2 - 
  static constexpr int DLT645_BAUD_RATE = 2400;  // 
  static constexpr int DLT645_RX_BUFFER_SIZE = 256;  // 
  
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
  uart_port_t uart_port_{UART_NUM_1};           // UART1
  bool uart_initialized_{false};                // UART initialization
#endif
  
  // （YAMLglobals）
  std::vector<uint8_t> meter_address_bytes_;     // 
  std::vector<uint8_t> broadcast_address_bytes_; // 
  bool device_address_discovered_{false};       // 
  
  // UART（YAML）
  std::vector<uint8_t> response_buffer_;         // 
  uint32_t last_data_receive_time_{0};           // 
  uint32_t current_command_timeout_ms_{1000};    // (waiting_for_response_)
  uint32_t frame_timeout_ms_{1000};              // (1)
  uint32_t device_discovery_timeout_ms_{2000};   // (2)
  
  // -
  uint32_t last_sent_data_identifier_{0};        // 
  uint32_t switch_baud_rate_when_failed_{false}; // 
  
  // （）
  std::vector<int> baud_rate_list_{9600, 4800, 2400, 1200};  // ，
  size_t current_baud_rate_index_{0};            // 

  
  // 
  uint32_t command_send_start_time_{0};
  uint32_t first_response_byte_time_{0};
  
  // === DL/T 645（）===
  // FreeRTOS，ESPHome
  float cached_active_power_w_{0.0f};            //  (W)
  float cached_energy_active_kwh_{0.0f};         //  (kWh)
  float cached_voltage_a_v_{0.0f};               // A (V)
  float cached_current_a_a_{0.0f};               // A (A)
  float cached_power_factor_{0.0f};              // 
  float cached_frequency_hz_{0.0f};              //  (Hz)
  float cached_energy_reverse_kwh_{0.0f};        //  (kWh)
  std::string cached_datetime_str_{""};          // 
  std::string cached_time_hms_str_{""};          // 
  
  // 
  float last_active_power_w_{0.0f};              // W，>=0<0
  bool power_direction_initialized_{false};      // 
  
  // 
  uint32_t cached_year_{0};                      // 
  uint32_t cached_month_{0};                     // 
  uint32_t cached_day_{0};                       // 
  uint32_t cached_weekday_{0};                   //  (1-7)
  
  // 
  uint32_t cached_hour_{0};                      // 
  uint32_t cached_minute_{0};                    // 
  uint32_t cached_second_{0};                    // 
};

// （）
class HelloWorldTrigger : public Trigger<uint32_t> {
 public:
  explicit HelloWorldTrigger(DLT645Component *parent) {
    parent->add_on_hello_world_callback([this](uint32_t magic_number) {
      this->trigger(magic_number);
    });
  }
};

// DL/T 645-2007 
class DeviceAddressTrigger : public Trigger<uint32_t> {
 public:
  explicit DeviceAddressTrigger(DLT645Component *parent) {
    parent->add_on_device_address_callback([this](uint32_t data_identifier) {
      this->trigger(data_identifier);
    });
  }
};

class ActivePowerTrigger : public Trigger<uint32_t, float> {
 public:
  explicit ActivePowerTrigger(DLT645Component *parent) {
    parent->add_on_active_power_callback([this](uint32_t data_identifier, float power_watts) {
      this->trigger(data_identifier, power_watts);  // power_watts W
    });
  }
};

class WarningReversePowerTrigger : public Trigger<uint32_t, float> {
 public:
  explicit WarningReversePowerTrigger(DLT645Component *parent) {
    parent->add_on_warning_reverse_power_callback([this](uint32_t data_identifier, float power_watts) {
      this->trigger(data_identifier, power_watts);  // >=0<0
    });
  }
};

class EnergyActiveTrigger : public Trigger<uint32_t, float> {
 public:
  explicit EnergyActiveTrigger(DLT645Component *parent) {
    parent->add_on_energy_active_callback([this](uint32_t data_identifier, float energy_kwh) {
      this->trigger(data_identifier, energy_kwh);
    });
  }
};

class VoltageATrigger : public Trigger<uint32_t, float> {
 public:
  explicit VoltageATrigger(DLT645Component *parent) {
    parent->add_on_voltage_a_callback([this](uint32_t data_identifier, float voltage_v) {
      this->trigger(data_identifier, voltage_v);
    });
  }
};

class CurrentATrigger : public Trigger<uint32_t, float> {
 public:
  explicit CurrentATrigger(DLT645Component *parent) {
    parent->add_on_current_a_callback([this](uint32_t data_identifier, float current_a) {
      this->trigger(data_identifier, current_a);
    });
  }
};

class PowerFactorTrigger : public Trigger<uint32_t, float> {
 public:
  explicit PowerFactorTrigger(DLT645Component *parent) {
    parent->add_on_power_factor_callback([this](uint32_t data_identifier, float power_factor) {
      this->trigger(data_identifier, power_factor);
    });
  }
};

class FrequencyTrigger : public Trigger<uint32_t, float> {
 public:
  explicit FrequencyTrigger(DLT645Component *parent) {
    parent->add_on_frequency_callback([this](uint32_t data_identifier, float frequency_hz) {
      this->trigger(data_identifier, frequency_hz);
    });
  }
};

class EnergyReverseTrigger : public Trigger<uint32_t, float> {
 public:
  explicit EnergyReverseTrigger(DLT645Component *parent) {
    parent->add_on_energy_reverse_callback([this](uint32_t data_identifier, float energy_reverse_kwh) {
      this->trigger(data_identifier, energy_reverse_kwh);
    });
  }
};

class DatetimeTrigger : public Trigger<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t> {
 public:
  explicit DatetimeTrigger(DLT645Component *parent) {
    parent->add_on_datetime_callback([this](uint32_t data_identifier, uint32_t year, uint32_t month, uint32_t day, uint32_t weekday) {
      this->trigger(data_identifier, year, month, day, weekday);
    });
  }
};

class TimeHmsTrigger : public Trigger<uint32_t, uint32_t, uint32_t, uint32_t> {
 public:
  explicit TimeHmsTrigger(DLT645Component *parent) {
    parent->add_on_time_hms_callback([this](uint32_t data_identifier, uint32_t hour, uint32_t minute, uint32_t second) {
      this->trigger(data_identifier, hour, minute, second);
    });
  }
};

}  // namespace dlt645_component
}  // namespace esphome