#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#if defined(USE_ESP32) || defined(USE_ESP_IDF)
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace esphome {
namespace dlt645_component {

// FreeRTOS task configuration constants
constexpr uint32_t DLT645_TASK_STACK_SIZE = 4096; // 4KB stack size
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
constexpr UBaseType_t DLT645_TASK_PRIORITY = 5; // Medium priority
constexpr UBaseType_t DLT645_REQUEST_QUEUE_LENGTH = 8; // Queue length for incoming requests
#endif
constexpr uint32_t DLT645_TRIGGER_INTERVAL_MS = 5000; // 5 second interval

// DL/T 645-2007 data identifier enumeration definitions
enum class DLT645_DATA_IDENTIFIER : uint32_t
{
    UNKNOWN = 0x00000000,              // Unknown/undefined
    DEVICE_ADDRESS = 0x04000401,       // Device address query
    ACTIVE_POWER_TOTAL = 0x02030000,   // Total active power
    ENERGY_ACTIVE_TOTAL = 0x00010000,  // Forward active total energy
    VOLTAGE_A_PHASE = 0x02010100,      // Phase A voltage
    CURRENT_A_PHASE = 0x02020100,      // Phase A current
    POWER_FACTOR_TOTAL = 0x02060000,   // Total power factor
    FREQUENCY = 0x02800002,            // Grid frequency
    ENERGY_REVERSE_TOTAL = 0x00020000, // Reverse active total energy
    DATETIME = 0x04000101,             // Date and time
    TIME_HMS = 0x04000102              // Hours, minutes, seconds
};

// DL/T 645-2007 request type for service code
// we use this as the index of array, so do not make it too sparse
enum class DLT645_REQUEST_TYPE : uint32_t
{
    READ_POS_START = 0x01,              // Read starting from position
    READ_DEVICE_ADDRESS = 0x01,         // Read device address
    READ_ACTIVE_POWER_TOTAL = 0x02,     // Read total active power
    READ_ENERGY_ACTIVE_TOTAL = 0x03,    // Read forward active total energy
    READ_VOLTAGE_A_PHASE = 0x04,        // Read phase A voltage
    READ_CURRENT_A_PHASE = 0x05,        // Read phase A current
    READ_POWER_FACTOR_TOTAL = 0x06,     // Read total power factor
    READ_FREQUENCY = 0x07,              // Read grid frequency
    READ_ENERGY_REVERSE_TOTAL = 0x08,   // Read reverse active total energy
    READ_DATE = 0x09,                   // Read date
    READ_TIME = 0x0A,                   // Read time
    READ_POS_END = 0x0A,                // Read ending at position
    READ_MAX_EVENTS = READ_POS_END,     // Maximum read events (for cycling)

    WRITE_POS_START = 0x10,             // Write starting from position
    WRITE_DATE = 0x10,                  // Write date (WW DD MM YY)
    WRITE_TIME = 0x11,                  // Write time

    CONTROL_POS_START = 0x20,           // Control starting from position
    CONTROL_BROADCAST_TIME_SYNC = 0x21, // Broadcast time synchronization
    CONTROL_RELAY_CONNECT = 0x22,       // Relay connect
    CONTROL_RELAY_DISCONNECT = 0x23     // Relay disconnect
};

// Event Group event bit definitions - Event bits corresponding to DL/T 645-2007 data identifiers
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
const EventBits_t EVENT_GENERAL = BIT0;                 // BIT0: Keep original general event
const EventBits_t EVENT_DI_DEVICE_ADDRESS = BIT1;       // BIT1: Device address query
const EventBits_t EVENT_DI_ACTIVE_POWER_TOTAL = BIT2;   // BIT2: Total power
const EventBits_t EVENT_DI_ENERGY_ACTIVE_TOTAL = BIT3;  // BIT3: Total energy
const EventBits_t EVENT_DI_VOLTAGE_A_PHASE = BIT4;      // BIT4: Phase A voltage
const EventBits_t EVENT_DI_CURRENT_A_PHASE = BIT5;      // BIT5: Phase A current
const EventBits_t EVENT_DI_POWER_FACTOR_TOTAL = BIT6;   // BIT6: Power factor
const EventBits_t EVENT_DI_FREQUENCY = BIT7;            // BIT7: Frequency
const EventBits_t EVENT_DI_ENERGY_REVERSE_TOTAL = BIT8; // BIT8: Reverse total energy
const EventBits_t EVENT_DI_DATETIME = BIT9;             // BIT9: Date and time
const EventBits_t EVENT_DI_TIME_HMS = BIT10;            // BIT10: Hours, minutes, seconds

// Mask for all DL/T 645 event bits (BIT1-BIT10)
const EventBits_t ALL_DLT645_EVENTS = EVENT_DI_DEVICE_ADDRESS | EVENT_DI_ACTIVE_POWER_TOTAL |
                                      EVENT_DI_ENERGY_ACTIVE_TOTAL | EVENT_DI_VOLTAGE_A_PHASE |
                                      EVENT_DI_CURRENT_A_PHASE | EVENT_DI_POWER_FACTOR_TOTAL | EVENT_DI_FREQUENCY |
                                      EVENT_DI_ENERGY_REVERSE_TOTAL | EVENT_DI_DATETIME | EVENT_DI_TIME_HMS;

// Mask for all event bits (including original EVENT_GENERAL)
const EventBits_t ALL_EVENTS = EVENT_GENERAL | ALL_DLT645_EVENTS;
#endif

class DLT645Component : public Component
{
public:
    void setup() override;
    void loop() override;
    void dump_config() override;

    void set_tx_pin(int pin)
    {
        this->dlt645_tx_pin_ = pin;
    }

    void set_rx_pin(int pin)
    {
        this->dlt645_rx_pin_ = pin;
    }

    void set_baud_rate(int rate)
    {
        this->dlt645_baud_rate_ = rate;
        auto it = std::find(this->baud_rate_list_.begin(), this->baud_rate_list_.end(), rate);
        if (it != this->baud_rate_list_.end()) {
            std::rotate(this->baud_rate_list_.begin(), it, it + 1);
        } else {
            this->baud_rate_list_.insert(this->baud_rate_list_.begin(), rate);
        }
        this->current_baud_rate_index_ = 0;
    }

    void set_rx_buffer_size(int size)
    {
        this->dlt645_rx_buffer_size_ = size;
    }

    // Set total power query ratio control parameter
    void set_power_ratio(int ratio)
    {
        this->power_ratio_ = ratio;
    }
    
    // Set simulation mode
    void set_simulate(bool simulate)
    {
        this->simulate_ = simulate;
    }

    // Set max events count for event polling (called during task initialization)
    void set_max_events(size_t max_events)
    {
        this->max_events_ = max_events;
    }

    // DL/T 645-2007 data identifier independent event callback functions
    void add_on_device_address_callback(std::function<void(uint32_t)>&& callback)
    {
        this->device_address_callback_.add(std::move(callback));
    }
    void add_on_active_power_callback(std::function<void(uint32_t, float)>&& callback)
    {
        this->active_power_callback_.add(std::move(callback));
    }
    void add_on_warning_reverse_power_callback(std::function<void(uint32_t, float)>&& callback)
    {
        this->warning_reverse_power_callback_.add(std::move(callback));
    }
    void add_on_energy_active_callback(std::function<void(uint32_t, float)>&& callback)
    {
        this->energy_active_callback_.add(std::move(callback));
    }
    void add_on_voltage_a_callback(std::function<void(uint32_t, float)>&& callback)
    {
        this->voltage_a_callback_.add(std::move(callback));
    }
    void add_on_current_a_callback(std::function<void(uint32_t, float)>&& callback)
    {
        this->current_a_callback_.add(std::move(callback));
    }
    void add_on_power_factor_callback(std::function<void(uint32_t, float)>&& callback)
    {
        this->power_factor_callback_.add(std::move(callback));
    }
    void add_on_frequency_callback(std::function<void(uint32_t, float)>&& callback)
    {
        this->frequency_callback_.add(std::move(callback));
    }
    void add_on_energy_reverse_callback(std::function<void(uint32_t, float)>&& callback)
    {
        this->energy_reverse_callback_.add(std::move(callback));
    }
    void add_on_datetime_callback(std::function<void(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>&& callback)
    {
        this->datetime_callback_.add(std::move(callback));
    }
    void add_on_time_hms_callback(std::function<void(uint32_t, uint32_t, uint32_t, uint32_t)>&& callback)
    {
        this->time_hms_callback_.add(std::move(callback));
    }

    // DL/T 645-2007 Relay control public methods
    bool relay_trip_action();  // Trip relay (open/disconnect)
    bool relay_close_action(); // Close relay (connect)
    
    // DL/T 645-2007 Date/Time setting public methods (DI-based write commands)
    bool set_datetime_action(); // Set meter date (WW DD MM YY format - 4 bytes, DI=0x04000101)
    bool set_time_action();     // Set meter time (HH mm SS format - 3 bytes, DI=0x04000102)
    
    // DL/T 645-2007 Broadcast time synchronization (Control Code 0x08)
    bool broadcast_time_sync(); // Broadcast time sync (YY MM DD HH mm format - 5 bytes, C=0x08)

protected:
    // FreeRTOS task related methods
    bool create_dlt645_task();
    void destroy_dlt645_task();
    static void dlt645_task_func(void* parameter);
    void process_dlt645_events();

    // === DL/T 645-2007 UART communication related methods ===
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
    bool init_dlt645_uart();   // UART initialization
    void deinit_dlt645_uart(); // UART deinitialization
    bool send_dlt645_frame(const std::vector<uint8_t>& frame,
                           uint32_t timeout_ms = 1000); // Send frame (with configurable timeout)
    void process_uart_data();                           // Process UART data
    void check_and_parse_dlt645_frame();                // Check and parse DL/T 645 frame

    // Dynamic baud rate switching function (only for device discovery command timeout)
    bool change_uart_baud_rate(int new_baud_rate); // Switch UART baud rate
    void cycle_to_next_baud_rate();                // Cycle to next baud rate

    // DL/T 645-2007 frame building and data processing helper functions
    std::vector<uint8_t> build_dlt645_read_frame(const std::vector<uint8_t>& address, uint32_t data_identifier);
    std::vector<uint8_t> build_dlt645_write_frame(const std::vector<uint8_t>& address, uint32_t data_identifier, 
                                                   const std::vector<uint8_t>& write_data);  // Generic write frame builder
    void scramble_dlt645_data(std::vector<uint8_t>& data);                                  // Data scrambling (+0x33)
    void unscramble_dlt645_data(std::vector<uint8_t>& data);                                // Data descrambling (-0x33)
    float bcd_to_float(const std::vector<uint8_t>& bcd_data, int decimal_places);           // BCD to float
    float bcd_to_float_with_sign(const std::vector<uint8_t>& bcd_data, int decimal_places); // BCD to float（）

    // DL/T 645-2007 basic query functions
    bool discover_meter_address();                                                                          // Discover meter address
    bool query_active_power_total();                                                                        // Query total power
    void parse_dlt645_data_by_identifier(uint32_t data_identifier, const std::vector<uint8_t>& data_field); // Parse data by DI

    // DL/T 645-2007 relay control frame building (protected helper)
    std::vector<uint8_t> build_dlt645_relay_control_frame(const std::vector<uint8_t>& address, bool close_relay);
    
    // DL/T 645-2007 date/time write helpers
    std::vector<uint8_t> build_dlt645_write_datetime_frame(const std::vector<uint8_t>& address); // Date: WW DD MM YY (4 bytes, DI=0x04000101)
    std::vector<uint8_t> build_dlt645_write_time_frame(const std::vector<uint8_t>& address);     // Time: HH mm SS (3 bytes, DI=0x04000102)
    std::vector<uint8_t> build_dlt645_broadcast_time_sync_frame(const std::vector<uint8_t>& address); // Broadcast: YY MM DD HH mm (5 bytes, C=0x08)

    // Event polling index management (internal use only)
    enum DLT645_REQUEST_TYPE get_next_event_index();
    
    // Simulation mode helper
    void simulate_measurements_();  // Generate simulated meter readings
    
    // Event polling state (private - cannot be accessed directly from outside)
    size_t max_events_{0};           // Maximum number of events (set during task initialization)
#endif

private:
    enum DLT645_REQUEST_TYPE current_request_type{DLT645_REQUEST_TYPE::READ_DEVICE_ADDRESS};
    
protected:
    // Query ratio control
    int power_ratio_{10};
    int total_power_query_count_{0};
    // Start from ENERGY_ACTIVE_TOTAL to include energy queries in rotation
    enum DLT645_REQUEST_TYPE last_non_power_query_index_{DLT645_REQUEST_TYPE::READ_ENERGY_ACTIVE_TOTAL};
    
    // Simulation mode
    bool simulate_{false};

    // DL/T 645-2007 event callbacks
    CallbackManager<void(uint32_t)> device_address_callback_;      // Device address
    CallbackManager<void(uint32_t, float)> active_power_callback_; // Total active power
    CallbackManager<void(uint32_t, float)>
        warning_reverse_power_callback_;                             // Reverse power warning (triggers only on >=0 to <0 transition)
    CallbackManager<void(uint32_t, float)> energy_active_callback_;  // Total active energy
    CallbackManager<void(uint32_t, float)> voltage_a_callback_;      // Phase A voltage
    CallbackManager<void(uint32_t, float)> current_a_callback_;      // Phase A current
    CallbackManager<void(uint32_t, float)> power_factor_callback_;   // Power factor
    CallbackManager<void(uint32_t, float)> frequency_callback_;      // Grid frequency
    CallbackManager<void(uint32_t, float)> energy_reverse_callback_; // Reverse active energy
    CallbackManager<void(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>
        datetime_callback_; // Date and time (DI, year, month, day, weekday)
    CallbackManager<void(uint32_t, uint32_t, uint32_t, uint32_t)>
        time_hms_callback_; // Time HMS (DI, hour, minute, second)

    // FreeRTOS
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
    TaskHandle_t dlt645_task_handle_{nullptr};
    EventGroupHandle_t event_group_{nullptr};
    QueueHandle_t request_queue_{nullptr};
    bool task_running_{false};
#endif

    // === DL/T 645-2007 UART ===

    int dlt645_tx_pin_{1};
    int dlt645_rx_pin_{2};
    int dlt645_baud_rate_{2400};
    int dlt645_rx_buffer_size_{256};

#if defined(USE_ESP32) || defined(USE_ESP_IDF)
    uart_port_t uart_port_{UART_NUM_1}; // UART1
    bool uart_initialized_{false};      // UART initialization
#endif

    // （YAMLglobals）
    std::vector<uint8_t> meter_address_bytes_;     //
    std::vector<uint8_t> broadcast_address_bytes_; //
    bool device_address_discovered_{false};        //

    // UART（YAML）
    std::vector<uint8_t> response_buffer_;       //
    uint32_t last_data_receive_time_{0};         //
    uint32_t current_command_timeout_ms_{1000};  // (waiting_for_response_)
    uint32_t frame_timeout_ms_{1000};            // (1)
    uint32_t device_discovery_timeout_ms_{2000}; // (2)

    // -
    uint32_t last_sent_data_identifier_{0};        //
    uint32_t switch_baud_rate_when_failed_{false}; //

    // （）
    std::vector<int> baud_rate_list_{1200, 2400, 4800, 9600}; // ，
    size_t current_baud_rate_index_{0};                       //

    //
    uint32_t command_send_start_time_{0};
    uint32_t first_response_byte_time_{0};

    // === DL/T 645（）===
    // FreeRTOS，ESPHome
    float cached_active_power_w_{0.0f};     //  (W)
    float cached_energy_active_kwh_{0.0f};  //  (kWh)
    float cached_voltage_a_v_{0.0f};        // A (V)
    float cached_current_a_a_{0.0f};        // A (A)
    float cached_power_factor_{0.0f};       //
    float cached_frequency_hz_{0.0f};       //  (Hz)
    float cached_energy_reverse_kwh_{0.0f}; //  (kWh)
    std::string cached_datetime_str_{""};   // Date string
    std::string cached_time_hms_str_{""};   // Time HMS string

    // Reverse power detection state tracking
    float last_active_power_w_{0.0f};         // Last power value in W, used to detect >=0 to <0 transition
    bool power_direction_initialized_{false}; // Whether power direction state has been initialized

    // Date components
    uint32_t cached_year_{0};    // Year
    uint32_t cached_month_{0};   // Month
    uint32_t cached_day_{0};     // Day
    uint32_t cached_weekday_{0}; // Weekday (1-7)

    // Time components
    uint32_t cached_hour_{0};   // Hour
    uint32_t cached_minute_{0}; // Minute
    uint32_t cached_second_{0}; // Second
};

// DL/T 645-2007
class DeviceAddressTrigger : public Trigger<uint32_t>
{
public:
    explicit DeviceAddressTrigger(DLT645Component* parent)
    {
        parent->add_on_device_address_callback([this](uint32_t data_identifier) { this->trigger(data_identifier); });
    }
};

class ActivePowerTrigger : public Trigger<uint32_t, float>
{
public:
    explicit ActivePowerTrigger(DLT645Component* parent)
    {
        parent->add_on_active_power_callback([this](uint32_t data_identifier, float power_watts) {
            this->trigger(data_identifier, power_watts); // power_watts W
        });
    }
};

class WarningReversePowerTrigger : public Trigger<uint32_t, float>
{
public:
    explicit WarningReversePowerTrigger(DLT645Component* parent)
    {
        parent->add_on_warning_reverse_power_callback([this](uint32_t data_identifier, float power_watts) {
            this->trigger(data_identifier, power_watts); // >=0<0
        });
    }
};

class EnergyActiveTrigger : public Trigger<uint32_t, float>
{
public:
    explicit EnergyActiveTrigger(DLT645Component* parent)
    {
        parent->add_on_energy_active_callback(
            [this](uint32_t data_identifier, float energy_kwh) { this->trigger(data_identifier, energy_kwh); });
    }
};

class VoltageATrigger : public Trigger<uint32_t, float>
{
public:
    explicit VoltageATrigger(DLT645Component* parent)
    {
        parent->add_on_voltage_a_callback(
            [this](uint32_t data_identifier, float voltage_v) { this->trigger(data_identifier, voltage_v); });
    }
};

class CurrentATrigger : public Trigger<uint32_t, float>
{
public:
    explicit CurrentATrigger(DLT645Component* parent)
    {
        parent->add_on_current_a_callback(
            [this](uint32_t data_identifier, float current_a) { this->trigger(data_identifier, current_a); });
    }
};

class PowerFactorTrigger : public Trigger<uint32_t, float>
{
public:
    explicit PowerFactorTrigger(DLT645Component* parent)
    {
        parent->add_on_power_factor_callback(
            [this](uint32_t data_identifier, float power_factor) { this->trigger(data_identifier, power_factor); });
    }
};

class FrequencyTrigger : public Trigger<uint32_t, float>
{
public:
    explicit FrequencyTrigger(DLT645Component* parent)
    {
        parent->add_on_frequency_callback(
            [this](uint32_t data_identifier, float frequency_hz) { this->trigger(data_identifier, frequency_hz); });
    }
};

class EnergyReverseTrigger : public Trigger<uint32_t, float>
{
public:
    explicit EnergyReverseTrigger(DLT645Component* parent)
    {
        parent->add_on_energy_reverse_callback([this](uint32_t data_identifier, float energy_reverse_kwh) {
            this->trigger(data_identifier, energy_reverse_kwh);
        });
    }
};

class DatetimeTrigger : public Trigger<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>
{
public:
    explicit DatetimeTrigger(DLT645Component* parent)
    {
        parent->add_on_datetime_callback(
            [this](uint32_t data_identifier, uint32_t year, uint32_t month, uint32_t day, uint32_t weekday) {
                this->trigger(data_identifier, year, month, day, weekday);
            });
    }
};

class TimeHmsTrigger : public Trigger<uint32_t, uint32_t, uint32_t, uint32_t>
{
public:
    explicit TimeHmsTrigger(DLT645Component* parent)
    {
        parent->add_on_time_hms_callback(
            [this](uint32_t data_identifier, uint32_t hour, uint32_t minute, uint32_t second) {
                this->trigger(data_identifier, hour, minute, second);
            });
    }
};

// DL/T 645-2007 继电器控制 Actions
template<typename... Ts> class RelayTripAction : public Action<Ts...>
{
public:
    RelayTripAction(DLT645Component* parent) : parent_(parent) {}

    void play(Ts... x) override
    {
        this->parent_->relay_trip_action();
    }

protected:
    DLT645Component* parent_;
};

template<typename... Ts> class RelayCloseAction : public Action<Ts...>
{
public:
    RelayCloseAction(DLT645Component* parent) : parent_(parent) {}

    void play(Ts... x) override
    {
        this->parent_->relay_close_action();
    }

protected:
    DLT645Component* parent_;
};

// DL/T 645-2007 Date/Time Setting Actions
template<typename... Ts> class SetDatetimeAction : public Action<Ts...>
{
public:
    SetDatetimeAction(DLT645Component* parent) : parent_(parent) {}

    void play(Ts... x) override
    {
        this->parent_->set_datetime_action();
    }

protected:
    DLT645Component* parent_;
};

template<typename... Ts> class SetTimeAction : public Action<Ts...>
{
public:
    SetTimeAction(DLT645Component* parent) : parent_(parent) {}

    void play(Ts... x) override
    {
        this->parent_->set_time_action();
    }

protected:
    DLT645Component* parent_;
};

// DL/T 645-2007 Broadcast Time Synchronization Action (Control Code 0x08)
template<typename... Ts> class BroadcastTimeSyncAction : public Action<Ts...>
{
public:
    BroadcastTimeSyncAction(DLT645Component* parent) : parent_(parent) {}

    void play(Ts... x) override
    {
        this->parent_->broadcast_time_sync();
    }

protected:
    DLT645Component* parent_;
};

} // namespace dlt645_component
} // namespace esphome
