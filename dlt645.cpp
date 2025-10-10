#include "dlt645.h"

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cassert>

#if defined(USE_ESP32) || defined(USE_ESP_IDF)
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

namespace esphome {
namespace dlt645_component {

static const char* const TAG = "dlt645_component";

// Cross-platform function to get milliseconds
uint32_t get_current_time_ms()
{
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
#else
    // Non-ESP32 platform, return a simple timestamp
    return 0;
#endif
}

void DLT645Component::setup()
{
    ESP_LOGCONFIG(TAG, "🚀 Setting up DLT645 component with FreeRTOS task...");
    ESP_LOGCONFIG(TAG, "Magic Number: %lu", (unsigned long)this->magic_number_);

#if defined(USE_ESP32) || defined(USE_ESP_IDF)
    // === Initialize DL/T 645-2007 UART communication variables ===
    ESP_LOGI(TAG, "📡 Initialize DL/T 645-2007 UART communication variables...");

    // Initialize address management variables (corresponding to globals in YAML)
    this->meter_address_bytes_ = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};     // Default broadcast address
    this->broadcast_address_bytes_ = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA}; // Broadcast address
    this->device_address_discovered_ = false;

    // Initialize response processing variables
    this->response_buffer_.clear();
    this->frame_timeout_ms_ = 1000;            // General command 1 second timeout
    this->device_discovery_timeout_ms_ = 2000; // 2
    this->last_data_receive_time_ = 0;
    this->last_sent_data_identifier_ = 0;

    this->current_baud_rate_index_ = 0; // （9600）

    this->command_send_start_time_ = 0;
    this->first_response_byte_time_ = 0;

    ESP_LOGI(TAG, "✅ DL/T 645");

    // === UART ===
    if (!this->init_dlt645_uart()) {
        ESP_LOGE(TAG, "❌ DL/T 645 UART");
        this->mark_failed();
        return;
    }

    // Creating event group
    this->event_group_ = xEventGroupCreate();
    assert(this->event_group_ != nullptr && "Failed to create event group");

    // FreeRTOS
    if (!this->create_dlt645_task()) {
        ESP_LOGE(TAG, "❌ FreeRTOS");
        this->mark_failed();
        return;
    }

    ESP_LOGCONFIG(TAG, "✅ FreeRTOS， %lu ", (unsigned long)(DLT645_TRIGGER_INTERVAL_MS / 1000));
#else
    ESP_LOGW(TAG, "⚠️ ESP32，loop");
#endif

    ESP_LOGCONFIG(TAG, "✅ Hello WorldComponent setup completed");
}

void DLT645Component::loop()
{
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
    // ESP32，FreeRTOS，loop
    this->process_dlt645_events();
#else
    // ESP32
    static uint32_t last_trigger_time = 0;
    uint32_t now = get_current_time_ms();
    if (now - last_trigger_time >= DLT645_TRIGGER_INTERVAL_MS) {
        this->trigger_hello_world_event();
        last_trigger_time = now;
    }
#endif
}

void DLT645Component::dump_config()
{
    ESP_LOGCONFIG(TAG, "Hello World Component (FreeRTOS Task):");
    ESP_LOGCONFIG(TAG, "  Magic Number: %lu", (unsigned long)this->magic_number_);
    ESP_LOGCONFIG(TAG, "  Trigger Interval: %lu ", (unsigned long)(DLT645_TRIGGER_INTERVAL_MS / 1000));
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
    ESP_LOGCONFIG(TAG, "  Task Status: %s", this->task_running_ ? "" : "");
    ESP_LOGCONFIG(TAG, "  Task Stack Size: %lu ", (unsigned long)DLT645_TASK_STACK_SIZE);
    ESP_LOGCONFIG(TAG, "  Task Priority: %d", (int)DLT645_TASK_PRIORITY);
    ESP_LOGCONFIG(TAG, "  Event Group: ");
    ESP_LOGCONFIG(TAG, "  DL/T 645 :");
    ESP_LOGCONFIG(TAG, "    - : %lu ms", (unsigned long)this->frame_timeout_ms_);
    ESP_LOGCONFIG(TAG, "    - : %lu ms", (unsigned long)this->device_discovery_timeout_ms_);
#endif
}

void DLT645Component::trigger_hello_world_event()
{
    ESP_LOGD(TAG, "🌍 Hello World ! Magic Number: %lu", (unsigned long)this->magic_number_);
    this->hello_world_callback_.call(this->magic_number_);
}

#if defined(USE_ESP32) || defined(USE_ESP_IDF)

bool DLT645Component::create_dlt645_task()
{
    if (this->dlt645_task_handle_ != nullptr) {
        ESP_LOGW(TAG, "⚠️ FreeRTOS");
        return true;
    }

    this->task_running_ = true;

    // FreeRTOS - ESP32Camera
    BaseType_t result = xTaskCreate(&DLT645Component::dlt645_task_func, //
                                    "dlt645_task",                      //
                                    DLT645_TASK_STACK_SIZE,             //
                                    this,                               // (this)
                                    DLT645_TASK_PRIORITY,               //
                                    &this->dlt645_task_handle_          //
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "❌ xTaskCreate，: %d", result);
        this->task_running_ = false;
        return false;
    }

    return true;
}

void DLT645Component::destroy_dlt645_task()
{
    if (this->dlt645_task_handle_ == nullptr) {
        return;
    }

    ESP_LOGI(TAG, "🧹 FreeRTOS...");
    this->task_running_ = false;

    vTaskDelay(pdMS_TO_TICKS(100));

    // Delete task
    if (this->dlt645_task_handle_ != nullptr) {
        vTaskDelete(this->dlt645_task_handle_);
        this->dlt645_task_handle_ = nullptr;
    }

    vEventGroupDelete(this->event_group_);
    this->event_group_ = nullptr;

    // === DL/T 645 UART ===
    this->deinit_dlt645_uart();
}

// Static task function - runs in independent FreeRTOS task
void DLT645Component::dlt645_task_func(void* parameter)
{
    DLT645Component* component = static_cast<DLT645Component*>(parameter);

    ESP_LOGI(TAG, "🚀 FreeRTOS task started, task handle: %p", xTaskGetCurrentTaskHandle());
    ESP_LOGI(TAG, "📊 Task stack high water mark: %lu bytes", (unsigned long)uxTaskGetStackHighWaterMark(nullptr));

    uint32_t last_trigger_time = get_current_time_ms();

    const EventBits_t dlt645_event_bits[] = {
        EVENT_DI_DEVICE_ADDRESS,       // BIT1:  (0x04000401)
        EVENT_DI_ACTIVE_POWER_TOTAL,   // BIT2:  (0x02030000)
        EVENT_DI_ENERGY_ACTIVE_TOTAL,  // BIT3:  (0x00010000)
        EVENT_DI_VOLTAGE_A_PHASE,      // BIT4: A (0x02010100)
        EVENT_DI_CURRENT_A_PHASE,      // BIT5: A (0x02020100)
        EVENT_DI_POWER_FACTOR_TOTAL,   // BIT6:  (0x02060000)
        EVENT_DI_FREQUENCY,            // BIT7:  (0x02800002)
        EVENT_DI_ENERGY_REVERSE_TOTAL, // BIT8:  (0x00020000)
        EVENT_DI_DATETIME,             // BIT9:  (0x04000101)
        EVENT_DI_TIME_HMS              // BIT10:  (0x04000102)
    };

    const char* dlt645_event_names[] = {"", "", "", "A", "A", "", "", "", "", ""};

    const uint32_t dlt645_data_identifiers[] = {
        static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::DEVICE_ADDRESS),       //
        static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::ACTIVE_POWER_TOTAL),   //
        static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::ENERGY_ACTIVE_TOTAL),  //
        static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::VOLTAGE_A_PHASE),      // A
        static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::CURRENT_A_PHASE),      // A
        static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::POWER_FACTOR_TOTAL),   //
        static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::FREQUENCY),            //
        static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::ENERGY_REVERSE_TOTAL), //
        static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::DATETIME),             //
        static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::TIME_HMS)              //
    };

    const size_t num_dlt645_events = sizeof(dlt645_event_bits) / sizeof(dlt645_event_bits[0]);
    size_t current_event_index = 0;

    ESP_LOGI(TAG, "📋 DL/T 645 event loop configured with %d data identifiers", num_dlt645_events);

    // === DL/T 645-2007 UART ===
    const uint32_t UART_READ_INTERVAL_MS = 5;

    //  - DL/T 645 +
    while (component->task_running_) {
        // === 2. DL/T 645 （1s）===
        current_event_index = component->get_next_event_index(current_event_index, num_dlt645_events);
        uint32_t data_identifier = dlt645_data_identifiers[current_event_index];
        const char* event_name = dlt645_event_names[current_event_index];

        ESP_LOGD(TAG, "📡 [%d/%d] DL/T 645: %s (DI: 0x%08X)", current_event_index + 1, num_dlt645_events, event_name,
                 data_identifier);

        // data_identifier，
        auto di_enum = static_cast<DLT645_DATA_IDENTIFIER>(data_identifier);

        bool send_success = false;
        if (di_enum == DLT645_DATA_IDENTIFIER::DEVICE_ADDRESS) {
            component->switch_baud_rate_when_failed_ = true;
#if 0
        send_success = component->discover_meter_address();
#else
            // use power query to discover address
            send_success = component->query_active_power_total();
#endif
        } else if (di_enum == DLT645_DATA_IDENTIFIER::ACTIVE_POWER_TOTAL) {
            component->switch_baud_rate_when_failed_ = false;
            send_success = component->query_active_power_total();
        } else {
            //  - Broadcast address
            component->switch_baud_rate_when_failed_ = false;
            std::vector<uint8_t> query_address = component->meter_address_bytes_;
            if (query_address.empty()) {
                query_address = {0x99, 0x99, 0x99, 0x99, 0x99, 0x99}; // Broadcast address
            }

            component->last_sent_data_identifier_ = data_identifier;

            std::vector<uint8_t> query_frame = component->build_dlt645_read_frame(query_address, data_identifier);
            send_success = component->send_dlt645_frame(query_frame, component->frame_timeout_ms_);
        }

        if (!send_success) {
            ESP_LOGW(TAG, "⚠️ DL/T 645: %s", event_name);
        }
        // ready data immediately after sending
        component->process_uart_data();

        //  - CPU
        vTaskDelay(pdMS_TO_TICKS(5)); // 10ms，
    }

    component->dlt645_task_handle_ = nullptr;
    vTaskDelete(nullptr);
}

void DLT645Component::process_dlt645_events()
{
    EventBits_t event_bits = xEventGroupWaitBits(this->event_group_, //
                                                 ALL_EVENTS,         //  (DL/T 645)
                                                 pdTRUE,             //
                                                 pdFALSE,            //
                                                 0                   // （0）
    );

    // （）
    if (event_bits & EVENT_GENERAL) {
        ESP_LOGD(TAG, "📥 EVENT_GENERAL");
        // ESPHome
        this->trigger_hello_world_event();
    }

    // DL/T 645-2007
    struct DLT645EventInfo
    {
        EventBits_t bit;
        uint32_t data_identifier;
        const char* name;
    };

    const DLT645EventInfo dlt645_events[] = {{EVENT_DI_DEVICE_ADDRESS, 0x04000401, ""},
                                             {EVENT_DI_ACTIVE_POWER_TOTAL, 0x02030000, ""},
                                             {EVENT_DI_ENERGY_ACTIVE_TOTAL, 0x00010000, ""},
                                             {EVENT_DI_VOLTAGE_A_PHASE, 0x02010100, "A"},
                                             {EVENT_DI_CURRENT_A_PHASE, 0x02020100, "A"},
                                             {EVENT_DI_POWER_FACTOR_TOTAL, 0x02060000, ""},
                                             {EVENT_DI_FREQUENCY, 0x02800002, ""},
                                             {EVENT_DI_ENERGY_REVERSE_TOTAL, 0x00020000, ""},
                                             {EVENT_DI_DATETIME, 0x04000101, ""},
                                             {EVENT_DI_TIME_HMS, 0x04000102, ""}};

    const size_t num_dlt645_events = sizeof(dlt645_events) / sizeof(dlt645_events[0]);

    // DL/T 645，
    for (size_t i = 0; i < num_dlt645_events; i++) {
        if (event_bits & dlt645_events[i].bit) {
            int bit_num = __builtin_ctzl(dlt645_events[i].bit); //

            ESP_LOGD(TAG, "📥 DL/T 645: %s (DI: 0x%08X, BIT%d)", dlt645_events[i].name,
                     dlt645_events[i].data_identifier, bit_num);

            switch (dlt645_events[i].bit) {
                case EVENT_DI_DEVICE_ADDRESS:
                    this->device_address_callback_.call(static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::DEVICE_ADDRESS));
                    break;
                case EVENT_DI_ACTIVE_POWER_TOTAL:
                    ESP_LOGD(TAG, "📊 : %.1f W", this->cached_active_power_w_);
                    this->active_power_callback_.call(static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::ACTIVE_POWER_TOTAL),
                                                      this->cached_active_power_w_);
                    break;
                case EVENT_DI_ENERGY_ACTIVE_TOTAL:
                    ESP_LOGD(TAG, "🔋 : %.2f kWh", this->cached_energy_active_kwh_);
                    this->energy_active_callback_.call(
                        static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::ENERGY_ACTIVE_TOTAL),
                        this->cached_energy_active_kwh_);
                    break;
                case EVENT_DI_VOLTAGE_A_PHASE:
                    ESP_LOGD(TAG, "🔌 A: %.1f V", this->cached_voltage_a_v_);
                    this->voltage_a_callback_.call(static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::VOLTAGE_A_PHASE),
                                                   this->cached_voltage_a_v_);
                    break;
                case EVENT_DI_CURRENT_A_PHASE:
                    ESP_LOGD(TAG, "🔄 A: %.3f A", this->cached_current_a_a_);
                    this->current_a_callback_.call(static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::CURRENT_A_PHASE),
                                                   this->cached_current_a_a_);
                    break;
                case EVENT_DI_POWER_FACTOR_TOTAL:
                    ESP_LOGD(TAG, "📈 : %.3f", this->cached_power_factor_);
                    this->power_factor_callback_.call(static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::POWER_FACTOR_TOTAL),
                                                      this->cached_power_factor_);
                    break;
                case EVENT_DI_FREQUENCY:
                    ESP_LOGD(TAG, "🌊 : %.2f Hz", this->cached_frequency_hz_);
                    this->frequency_callback_.call(static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::FREQUENCY),
                                                   this->cached_frequency_hz_);
                    break;
                case EVENT_DI_ENERGY_REVERSE_TOTAL:
                    ESP_LOGD(TAG, "🔄 : %.2f kWh", this->cached_energy_reverse_kwh_);
                    this->energy_reverse_callback_.call(
                        static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::ENERGY_REVERSE_TOTAL),
                        this->cached_energy_reverse_kwh_);
                    break;
                case EVENT_DI_DATETIME:
                    ESP_LOGD(TAG, "📅 : %04u-%02u-%02u %u", this->cached_year_, this->cached_month_, this->cached_day_,
                             this->cached_weekday_);
                    this->datetime_callback_.call(static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::DATETIME),
                                                  this->cached_year_, this->cached_month_, this->cached_day_,
                                                  this->cached_weekday_);
                    break;
                case EVENT_DI_TIME_HMS:
                    ESP_LOGD(TAG, "⏰ : %02u:%02u:%02u", this->cached_hour_, this->cached_minute_,
                             this->cached_second_);
                    this->time_hms_callback_.call(static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::TIME_HMS),
                                                  this->cached_hour_, this->cached_minute_, this->cached_second_);
                    break;
                default:
                    ESP_LOGW(TAG, "⚠️ : 0x%08X", dlt645_events[i].bit);
                    break;
            }
        }
    }
}

#endif // defined(USE_ESP32) || defined(USE_ESP_IDF)

#if defined(USE_ESP32) || defined(USE_ESP_IDF)

// === DL/T 645-2007 UART ===

bool DLT645Component::init_dlt645_uart()
{
    ESP_LOGI(TAG, "🔧 DL/T 645-2007 UART...");

    int current_baud_rate = this->baud_rate_list_[this->current_baud_rate_index_];

    // UART
    uart_config_t uart_config = {
        .baud_rate = current_baud_rate,        //
        .data_bits = UART_DATA_8_BITS,         // 8
        .parity = UART_PARITY_EVEN,            //
        .stop_bits = UART_STOP_BITS_1,         // 1
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, //
        .rx_flow_ctrl_thresh = 122,            // RX
        .source_clk = UART_SCLK_DEFAULT,       //
    };

    ESP_LOGI(TAG, "📋 UART: =%d, =8, =, =1", current_baud_rate);

    // UART
    esp_err_t ret = uart_param_config(this->uart_port_, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ UART: %s", esp_err_to_name(ret));
        return false;
    }

    // UART
    ret = uart_set_pin(this->uart_port_, DLT645_TX_PIN, DLT645_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ UART: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "📌 UART: TX=GPIO%d, RX=GPIO%d", DLT645_TX_PIN, DLT645_RX_PIN);

    // UART
    ret = uart_driver_install(this->uart_port_, DLT645_RX_BUFFER_SIZE, 0, 0, nullptr, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ UART: %s", esp_err_to_name(ret));
        return false;
    }

    this->uart_initialized_ = true;

    ESP_LOGI(TAG, "✅ DL/T 645 UART");
    ESP_LOGI(TAG, "   - UART: %d", this->uart_port_);
    ESP_LOGI(TAG, "   - : %d ", DLT645_RX_BUFFER_SIZE);

    return true;
}

void DLT645Component::deinit_dlt645_uart()
{
    if (this->uart_initialized_) {
        ESP_LOGD(TAG, "🧹 DL/T 645 UART...");
        uart_driver_delete(this->uart_port_);
        this->uart_initialized_ = false;
        ESP_LOGD(TAG, "✅ UART");
    }
}

// ===  ===

bool DLT645Component::change_uart_baud_rate(int new_baud_rate)
{
    if (!this->uart_initialized_) {
        ESP_LOGE(TAG, "❌ UART，");
        return false;
    }

    ESP_LOGD(TAG, "� UART: %d", new_baud_rate);

    // UART
    uart_wait_tx_done(this->uart_port_, pdMS_TO_TICKS(100));
    uart_flush_input(this->uart_port_);

    uart_driver_delete(this->uart_port_);
    this->uart_initialized_ = false;

    // UART
    uart_config_t uart_config = {
        .baud_rate = new_baud_rate, //
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(this->uart_port_, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ UART: %s", esp_err_to_name(ret));
        return false;
    }

    // UART
    ret = uart_set_pin(this->uart_port_, DLT645_TX_PIN, DLT645_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ UART: %s", esp_err_to_name(ret));
        return false;
    }

    // UART
    ret = uart_driver_install(this->uart_port_, DLT645_RX_BUFFER_SIZE, 0, 0, nullptr, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ UART: %s", esp_err_to_name(ret));
        return false;
    }

    this->uart_initialized_ = true;

    ESP_LOGW(TAG, "✅ DL/T 645 UART: %d", new_baud_rate);
    return true;
}

void DLT645Component::cycle_to_next_baud_rate()
{
    int current_baud_rate = this->baud_rate_list_[this->current_baud_rate_index_];

    this->current_baud_rate_index_ = (this->current_baud_rate_index_ + 1) % this->baud_rate_list_.size();
    int next_baud_rate = this->baud_rate_list_[this->current_baud_rate_index_];

    ESP_LOGW(TAG, " %d : %d (: %d/%d)", current_baud_rate, next_baud_rate, this->current_baud_rate_index_,
             this->baud_rate_list_.size());

    if (!this->change_uart_baud_rate(next_baud_rate)) {
        ESP_LOGE(TAG, "❌ ，");
    }
}

bool DLT645Component::send_dlt645_frame(const std::vector<uint8_t>& frame_data, uint32_t timeout_ms)
{
    if (!this->uart_initialized_) {
        ESP_LOGE(TAG, "❌ UART，");
        return false;
    }

    //  -
    std::string hex_frame = "";
    for (size_t i = 0; i < frame_data.size(); i++) {
        char hex_str[4];
        sprintf(hex_str, "%02X ", frame_data[i]);
        hex_frame += hex_str;
    }
    ESP_LOGD(TAG, "📤 DL/T 645 (%d, %dms): %s", frame_data.size(), timeout_ms, hex_frame.c_str());

    uart_flush_input(this->uart_port_);
    this->response_buffer_.clear();

    int bytes_written = uart_write_bytes(this->uart_port_, frame_data.data(), frame_data.size());

    if (bytes_written != frame_data.size()) {
        ESP_LOGE(TAG, "❌ UART， %d ， %d ", frame_data.size(), bytes_written);
        return false;
    }

    uart_wait_tx_done(this->uart_port_, pdMS_TO_TICKS(500));

    this->current_command_timeout_ms_ = timeout_ms;
    this->last_data_receive_time_ = get_current_time_ms();

    ESP_LOGD(TAG, "✅  %d  DL/T 645，: %dms", frame_data.size(), timeout_ms);
    return true;
}

void DLT645Component::process_uart_data()
{
    if (!this->uart_initialized_) {
        return;
    }

    // ：send_dlt645_frame()current_command_timeout_ms_
    uint32_t timeout_ms = this->current_command_timeout_ms_;
    bool is_device_discovery = (this->last_sent_data_identifier_ == 0x04000401);

    ESP_LOGD(TAG, "📡 UART，: %dms (: %s, DI: 0x%08X)", timeout_ms, is_device_discovery ? "" : "",
             this->last_sent_data_identifier_);

    // === 1: current_command_timeout_ms_， ===
    uint8_t data[256];
    // timeout for first byte read
    int bytes_read = uart_read_bytes(this->uart_port_, data, 1, pdMS_TO_TICKS(timeout_ms));

    if (bytes_read <= 0) {
        // ，
        uint32_t current_time = get_current_time_ms();
        uint32_t actual_wait_time = current_time - this->last_data_receive_time_;

        // （）
        ESP_LOGE(TAG, "⏰ DL/T 645， (: %dms, : %dms, DI: 0x%08X)", actual_wait_time, this->current_command_timeout_ms_,
                 this->last_sent_data_identifier_);
        this->response_buffer_.clear();
        if (this->switch_baud_rate_when_failed_) {
            this->cycle_to_next_baud_rate();
        }
        return;
    }

    // === 2: ， ===
    int total_bytes_read = 0;

    for (int i = 0; i < bytes_read; i++) {
        this->response_buffer_.push_back(data[i]);
    }
    total_bytes_read += bytes_read;

    ESP_LOGD(TAG, "📨  %d ", bytes_read);

    // === 3: 20ms， ===
    while (true) {
        bytes_read = uart_read_bytes(this->uart_port_, data, sizeof(data), pdMS_TO_TICKS(20));

        if (bytes_read <= 0) {
            // 20ms，
            ESP_LOGD(TAG, "📦 20ms，");
            break;
        }

        for (int i = 0; i < bytes_read; i++) {
            this->response_buffer_.push_back(data[i]);
        }
        total_bytes_read += bytes_read;

        ESP_LOGD(TAG, "📨  %d ", bytes_read);
    }

    // === 4:  ===
    if (total_bytes_read > 0) {
        ESP_LOGD(TAG, "📥  %d ，: %d", total_bytes_read, this->response_buffer_.size());

        this->last_data_receive_time_ = get_current_time_ms();

        this->check_and_parse_dlt645_frame();
    }
}

void DLT645Component::check_and_parse_dlt645_frame()
{
    if (this->response_buffer_.size() < 12) {
        return;
    }

    ESP_LOGD(TAG, "📦 DL/T 645 (%d)", this->response_buffer_.size());

    std::string hex_data = "";
    for (size_t i = 0; i < this->response_buffer_.size(); i++) {
        char hex_str[4];
        sprintf(hex_str, "%02X ", this->response_buffer_[i]);
        hex_data += hex_str;
    }
    ESP_LOGD(TAG, "� : %s", hex_data.c_str());

    //  ( 0xFE)
    size_t frame_start = 0;
    while (frame_start < this->response_buffer_.size() && this->response_buffer_[frame_start] == 0xFE) {
        frame_start++;
    }

    if (frame_start >= this->response_buffer_.size() || this->response_buffer_[frame_start] != 0x68) {
        ESP_LOGW(TAG, "⚠️  (0x68)");
        this->response_buffer_.clear();
        return;
    }

    ESP_LOGD(TAG, "🔍 ，: %d", frame_start);

    //  (:  + 6 +  +  +  +  + )
    size_t required_length = frame_start + 12;
    if (this->response_buffer_.size() < required_length) {
        ESP_LOGD(TAG, "📦 ，...");
        return;
    }

    size_t idx = frame_start;

    //  ( frame_start + 7)
    if (idx + 7 >= this->response_buffer_.size() || this->response_buffer_[idx + 7] != 0x68) {
        ESP_LOGW(TAG, "⚠️  (0x68) ");
        this->response_buffer_.clear();
        return;
    }

    //  (6)
    std::vector<uint8_t> address(6);
    for (int i = 0; i < 6; i++) {
        address[i] = this->response_buffer_[idx + 1 + i];
    }

    uint8_t control_code = this->response_buffer_[idx + 8];
    uint8_t data_length = this->response_buffer_[idx + 9];

    ESP_LOGD(TAG, "📋 : %02X %02X %02X %02X %02X %02X, : 0x%02X, : %d", address[0], address[1], address[2], address[3],
             address[4], address[5], control_code, data_length);

    //  (0x91)  (0xD1/0xB1)
    if (control_code == 0xD1 || control_code == 0xB1) {
        ESP_LOGW(TAG, "⚠️ ，: 0x%02X", control_code);
        this->response_buffer_.clear();
        return;
    }

    if (control_code != 0x91) {
        ESP_LOGW(TAG, "⚠️ : 0x%02X", control_code);
        this->response_buffer_.clear();
        return;
    }

    size_t frame_total_length = idx + 10 + data_length + 2; // +2 for checksum and end delimiter
    if (this->response_buffer_.size() < frame_total_length) {
        ESP_LOGD(TAG, "📦  ( %d ， %d )", frame_total_length, this->response_buffer_.size());
        return;
    }

    if (this->response_buffer_[frame_total_length - 1] != 0x16) {
        ESP_LOGW(TAG, "⚠️  (0x16) : 0x%02X", this->response_buffer_[frame_total_length - 1]);
        this->response_buffer_.clear();
        return;
    }

    uint8_t calculated_checksum = 0;
    for (size_t i = idx; i < idx + 10 + data_length; i++) {
        calculated_checksum += this->response_buffer_[i];
    }
    uint8_t received_checksum = this->response_buffer_[idx + 10 + data_length];

    if (calculated_checksum != received_checksum) {
        ESP_LOGW(TAG, "⚠️  (: 0x%02X, : 0x%02X)", calculated_checksum, received_checksum);
        this->response_buffer_.clear();
        return;
    }

    ESP_LOGD(TAG, "✅ DL/T 645，");

    std::vector<uint8_t> data_field(data_length);
    for (int i = 0; i < data_length; i++) {
        data_field[i] = this->response_buffer_[idx + 10 + i];
    }

    //  ( 0x33)
    unscramble_dlt645_data(data_field);

    //  (4，LSB)
    if (data_length >= 4) {
        uint32_t data_identifier = data_field[0] | (data_field[1] << 8) | (data_field[2] << 16) | (data_field[3] << 24);

        ESP_LOGD(TAG, "🎯 : 0x%08X", data_identifier);

        parse_dlt645_data_by_identifier(data_identifier, data_field);
    }

    if (address[0] != 0x99 || address[1] != 0x99) { // Broadcast address
        // Check if address has actually changed before updating
        bool address_changed = false;
        if (this->meter_address_bytes_.size() != 6) {
            address_changed = true;
        } else {
            for (int i = 0; i < 6; i++) {
                if (this->meter_address_bytes_[i] != address[i]) {
                    address_changed = true;
                    break;
                }
            }
        }
        if (address_changed) {
            // Log the address change with old and new values
            if (this->meter_address_bytes_.size() == 6) {
                ESP_LOGW(TAG,
                         "📍 电表地址已更新 (Meter address changed): 旧地址 %02X %02X %02X %02X %02X %02X -> 新地址 "
                         "%02X %02X %02X %02X %02X %02X",
                         this->meter_address_bytes_[0], this->meter_address_bytes_[1], this->meter_address_bytes_[2],
                         this->meter_address_bytes_[3], this->meter_address_bytes_[4], this->meter_address_bytes_[5],
                         address[0], address[1], address[2], address[3], address[4], address[5]);
            } else {
                ESP_LOGW(TAG, "📍 电表地址已发现 (Meter address discovered): %02X %02X %02X %02X %02X %02X", address[0],
                         address[1], address[2], address[3], address[4], address[5]);
            }
            this->meter_address_bytes_ = address;
            this->device_address_discovered_ = true;
        }
    }

    this->response_buffer_.clear();
    ESP_LOGD(TAG, "📦 DL/T 645");
}

// ============= DL/T 645-2007  =============

std::vector<uint8_t> DLT645Component::build_dlt645_read_frame(const std::vector<uint8_t>& address,
                                                              uint32_t data_identifier)
{
    std::vector<uint8_t> frame;

    // 1.  (，)
    frame.push_back(0xFE);
    frame.push_back(0xFE);

    // 2.
    frame.push_back(0x68);

    // 3.  (6，LSB)
    for (size_t i = 0; i < 6 && i < address.size(); i++) {
        frame.push_back(address[i]);
    }

    // 4.
    frame.push_back(0x68);

    // 5.  (0x11 = )
    frame.push_back(0x11);

    // 6.  (4)
    frame.push_back(0x04);

    // 7. ： (4，LSB，0x33)
    uint8_t di_bytes[4];
    di_bytes[0] = (data_identifier & 0xFF) + 0x33;
    di_bytes[1] = ((data_identifier >> 8) & 0xFF) + 0x33;
    di_bytes[2] = ((data_identifier >> 16) & 0xFF) + 0x33;
    di_bytes[3] = ((data_identifier >> 24) & 0xFF) + 0x33;

    for (int i = 0; i < 4; i++) {
        frame.push_back(di_bytes[i]);
    }

    // 8.  (0x68256)
    uint8_t checksum = 0;
    for (size_t i = 2; i < frame.size(); i++) { //
        checksum += frame[i];
    }
    frame.push_back(checksum);

    // 9.
    frame.push_back(0x16);

    ESP_LOGD(TAG, "🔧 DL/T 645: =%02X%02X%02X%02X%02X%02X, DI=0x%08X", address[0], address[1], address[2], address[3],
             address[4], address[5], data_identifier);

    return frame;
}

// /
void DLT645Component::scramble_dlt645_data(std::vector<uint8_t>& data)
{
    for (size_t i = 0; i < data.size(); i++) {
        data[i] += 0x33;
    }
}

void DLT645Component::unscramble_dlt645_data(std::vector<uint8_t>& data)
{
    for (size_t i = 0; i < data.size(); i++) {
        data[i] -= 0x33;
    }
}

// BCD
float DLT645Component::bcd_to_float(const std::vector<uint8_t>& bcd_data, int decimal_places)
{
    uint32_t int_value = 0;
    uint32_t multiplier = 1;

    for (size_t i = 0; i < bcd_data.size(); i++) {
        uint8_t low_nibble = bcd_data[i] & 0x0F;
        uint8_t high_nibble = (bcd_data[i] >> 4) & 0x0F;

        // BCD
        if (low_nibble > 9 || high_nibble > 9) {
            ESP_LOGW(TAG, "⚠️ BCD: 0x%02X", bcd_data[i]);
            return 0.0f;
        }

        int_value += low_nibble * multiplier;
        multiplier *= 10;
        int_value += high_nibble * multiplier;
        multiplier *= 10;
    }

    return (float)int_value / pow(10, decimal_places);
}

// DL/T 645-2007 BCD
float DLT645Component::bcd_to_float_with_sign(const std::vector<uint8_t>& bcd_data, int decimal_places)
{
    if (bcd_data.empty()) {
        ESP_LOGW(TAG, "⚠️ BCD");
        return 0.0f;
    }

    //  ()
    bool is_negative = (bcd_data.back() & 0x80) != 0;

    std::vector<uint8_t> clean_bcd_data = bcd_data;
    clean_bcd_data.back() &= 0x7F; //

    ESP_LOGD(TAG, "📊 BCD: =0x%02X, =0x%02X, =%s", bcd_data.back(), clean_bcd_data.back(), is_negative ? "" : "");

    // BCD
    float result = bcd_to_float(clean_bcd_data, decimal_places);

    return is_negative ? -result : result;
}

// ============= DL/T 645-2007  =============

bool DLT645Component::discover_meter_address()
{
    if (!this->uart_initialized_) {
        ESP_LOGE(TAG, "❌ UART，");
        return false;
    }

    ESP_LOGD(TAG, "🔍 DL/T 645...");

    // Broadcast address 99 99 99 99 99 99
    std::vector<uint8_t> broadcast_address = {0x99, 0x99, 0x99, 0x99, 0x99, 0x99};

    // ： (0x04000401)
    uint32_t device_address_di = 0x04000401;

    this->last_sent_data_identifier_ = device_address_di;

    std::vector<uint8_t> discover_frame = build_dlt645_read_frame(broadcast_address, device_address_di);

    ESP_LOGD(TAG, "📡 ，Broadcast addressDI=0x04000401");

    // ，
    bool success = send_dlt645_frame(discover_frame, this->device_discovery_timeout_ms_);

    if (success) {
        ESP_LOGD(TAG, "✅ ，...");
    } else {
        ESP_LOGE(TAG, "❌ ");
    }

    return success;
}

bool DLT645Component::query_active_power_total()
{
    if (!this->uart_initialized_) {
        ESP_LOGE(TAG, "❌ UART，");
        return false;
    }

    if (this->meter_address_bytes_.empty() ||
        (this->meter_address_bytes_.size() == 6 && this->meter_address_bytes_[0] == 0x99)) {
        ESP_LOGW(TAG, "⚠️ ，Broadcast address");
        // Broadcast address，
    }

    ESP_LOGD(TAG, "⚡ DL/T 645...");

    // （），Broadcast address
    std::vector<uint8_t> meter_address = this->meter_address_bytes_;
    if (meter_address.empty()) {
        meter_address = {0x99, 0x99, 0x99, 0x99, 0x99, 0x99}; // Broadcast address
        ESP_LOGD(TAG, "📡 Broadcast address");
    } else {
        ESP_LOGD(TAG, "📡 : %02X %02X %02X %02X %02X %02X", meter_address[0], meter_address[1], meter_address[2],
                 meter_address[3], meter_address[4], meter_address[5]);
    }

    // ： (0x02030000)
    uint32_t active_power_total_di = 0x02030000;

    this->last_sent_data_identifier_ = active_power_total_di;

    std::vector<uint8_t> power_query_frame = build_dlt645_read_frame(meter_address, active_power_total_di);

    ESP_LOGD(TAG, "📊 ，DI=0x02030000");

    // ，
    bool success = send_dlt645_frame(power_query_frame, this->frame_timeout_ms_);

    if (success) {
        ESP_LOGD(TAG, "✅ ，...");
    } else {
        ESP_LOGE(TAG, "❌ ");
    }

    return success;
}

// DL/T 645
void DLT645Component::parse_dlt645_data_by_identifier(uint32_t data_identifier, const std::vector<uint8_t>& data_field)
{
    ESP_LOGD(TAG, "🔍 DL/T 645 - DI: 0x%08X, : %d", data_identifier, data_field.size());

    //  (4)，
    if (data_field.size() <= 4) {
        ESP_LOGW(TAG, "⚠️ ，");
        return;
    }

    std::vector<uint8_t> actual_data(data_field.begin() + 4, data_field.end());

    std::string hex_str = "";
    for (size_t i = 0; i < actual_data.size(); i++) {
        char hex[4];
        sprintf(hex, "%02X ", actual_data[i]);
        hex_str += hex;
    }
    ESP_LOGD(TAG, "📊  (%d): %s", actual_data.size(), hex_str.c_str());

    // data_identifier，
    auto di_enum = static_cast<DLT645_DATA_IDENTIFIER>(data_identifier);

    switch (di_enum) {
        case DLT645_DATA_IDENTIFIER::DEVICE_ADDRESS: { //
            ESP_LOGW(TAG, "🔍 [] ");
            // ，

            // （）
            xEventGroupSetBits(this->event_group_, EVENT_DI_DEVICE_ADDRESS);
            break;
        }

        case DLT645_DATA_IDENTIFIER::ACTIVE_POWER_TOTAL: { //
            if (actual_data.size() >= 3) {
                // DL/T 645：3BCD，XX.XXXX kW (4)

                ESP_LOGD(TAG, "📊 : %02X %02X %02X", actual_data[0], actual_data[1], actual_data[2]);

                // BCD
                float power_kw = bcd_to_float_with_sign(actual_data, 4);

                // W
                float power_w = power_kw * 1000.0f;

                ESP_LOGD(TAG, "⚡ [] %.1f W (%.4f kW)", power_w, power_kw);

                // Detect reverse power (<0) and trigger warning
                // Priority 1: Trigger warning immediately when reverse power is detected
                // Priority 2: Avoid duplicate warnings (do not trigger again when power remains negative)
                if (power_w < 0.0f) {
                    // Current power is negative (reverse power / feeding back to grid)
                    if (!this->power_direction_initialized_) {
                        // First reading is reverse power, trigger warning immediately
                        ESP_LOGW(TAG, "⚠️ Reverse power detected on first reading: %.1f W", power_w);
                        this->warning_reverse_power_callback_.call(
                            static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::ACTIVE_POWER_TOTAL), power_w);
                        this->power_direction_initialized_ = true;
                    } else if (this->last_active_power_w_ >= 0.0f) {
                        // Power transitioned from positive/zero (>=0) to negative (<0), trigger warning
                        ESP_LOGW(TAG, "⚠️ Power direction reversed from >=0 to <0: %.1f W -> %.1f W",
                                 this->last_active_power_w_, power_w);
                        this->warning_reverse_power_callback_.call(
                            static_cast<uint32_t>(DLT645_DATA_IDENTIFIER::ACTIVE_POWER_TOTAL), power_w);
                    } else {
                        // Power remains negative, do not trigger warning (avoid duplicates)
                        ESP_LOGD(TAG, "🔄 Power remains negative: %.1f W (warning not triggered)", power_w);
                    }
                } else {
                    // Current power is positive or zero
                    if (!this->power_direction_initialized_) {
                        // First reading, initialize state
                        this->power_direction_initialized_ = true;
                        ESP_LOGD(TAG, "🔧 Power direction state initialized: %.1f W", power_w);
                    }
                }

                // Save current power value for next comparison
                this->last_active_power_w_ = power_w;

                // （）
                this->cached_active_power_w_ = power_w;
                xEventGroupSetBits(this->event_group_, EVENT_DI_ACTIVE_POWER_TOTAL);
            } else {
                ESP_LOGW(TAG, "⚠️ ");
            }
            break;
        }

        case DLT645_DATA_IDENTIFIER::ENERGY_ACTIVE_TOTAL: { //
            if (actual_data.size() >= 4) {
                // DL/T 645：4BCD，XXXXXX.XX kWh (2)
                float energy_kwh = bcd_to_float(actual_data, 2);

                ESP_LOGD(TAG, "🔋 [] %.2f kWh", energy_kwh);

                // （）
                this->cached_energy_active_kwh_ = energy_kwh;
                xEventGroupSetBits(this->event_group_, EVENT_DI_ENERGY_ACTIVE_TOTAL);
            } else {
                ESP_LOGW(TAG, "⚠️ ");
            }
            break;
        }

        case DLT645_DATA_IDENTIFIER::VOLTAGE_A_PHASE: { // A
            if (actual_data.size() >= 2) {
                // DL/T 645：2BCD，XXX.X V (1)
                float voltage_v = bcd_to_float(actual_data, 1);

                ESP_LOGD(TAG, "🔌 [A] %.1f V", voltage_v);

                // （）
                this->cached_voltage_a_v_ = voltage_v;
                xEventGroupSetBits(this->event_group_, EVENT_DI_VOLTAGE_A_PHASE);
            } else {
                ESP_LOGW(TAG, "⚠️ A");
            }
            break;
        }

        case DLT645_DATA_IDENTIFIER::CURRENT_A_PHASE: { // A
            if (actual_data.size() >= 3) {
                float current_a = bcd_to_float_with_sign(actual_data, 3);

                ESP_LOGD(TAG, "🔄 [A] %.3f A", current_a);
                // （）
                this->cached_current_a_a_ = current_a;
                xEventGroupSetBits(this->event_group_, EVENT_DI_CURRENT_A_PHASE);
            } else {
                ESP_LOGW(TAG, "⚠️ A");
            }
            break;
        }

        case DLT645_DATA_IDENTIFIER::POWER_FACTOR_TOTAL: { //
            if (actual_data.size() >= 2) {
                // DL/T 645：2BCD，X.XXX (3)
                ESP_LOGD(TAG, "📊 : %02X %02X", actual_data[0], actual_data[1]);
                float power_factor = bcd_to_float_with_sign(actual_data, 3);
                ESP_LOGD(TAG, "📈 [] %.3f", power_factor);

                // （）
                this->cached_power_factor_ = power_factor;
                xEventGroupSetBits(this->event_group_, EVENT_DI_POWER_FACTOR_TOTAL);
            } else {
                ESP_LOGW(TAG, "⚠️ ");
            }
            break;
        }

        case DLT645_DATA_IDENTIFIER::FREQUENCY: { //
            if (actual_data.size() >= 2) {
                // DL/T 645：2BCD，XX.XX Hz (2)
                float frequency_hz = bcd_to_float(actual_data, 2);

                ESP_LOGD(TAG, "🌊 [] %.2f Hz", frequency_hz);

                // （）
                this->cached_frequency_hz_ = frequency_hz;
                xEventGroupSetBits(this->event_group_, EVENT_DI_FREQUENCY);
            } else {
                ESP_LOGW(TAG, "⚠️ ");
            }
            break;
        }

        case DLT645_DATA_IDENTIFIER::ENERGY_REVERSE_TOTAL: { //
            if (actual_data.size() >= 4) {
                // DL/T 645：4BCD，XXXXXX.XX kWh (2)
                float energy_kwh = bcd_to_float(actual_data, 2);

                ESP_LOGD(TAG, "🔄 [] %.2f kWh", energy_kwh);

                // （）
                this->cached_energy_reverse_kwh_ = energy_kwh;
                xEventGroupSetBits(this->event_group_, EVENT_DI_ENERGY_REVERSE_TOTAL);
            } else {
                ESP_LOGW(TAG, "⚠️ ");
            }
            break;
        }

        case DLT645_DATA_IDENTIFIER::DATETIME: { //
            std::string hex_str = "";
            for (size_t i = 0; i < actual_data.size(); i++) {
                char hex[4];
                sprintf(hex, "%02X ", actual_data[i]);
                hex_str += hex;
            }
            ESP_LOGD(TAG, "📊  (%d): %s", actual_data.size(), hex_str.c_str());

            if (actual_data.size() == 4) {
                // 4 WDMY - ：---
                auto bcd_to_byte = [](uint8_t bcd) -> int { return ((bcd >> 4) & 0x0F) * 10 + (bcd & 0x0F); };

                // WDMY：00 05 10 25 →  5 10 2025
                int week_day = bcd_to_byte(actual_data[0]); // W:  (0=, 1=, ...)
                int day = bcd_to_byte(actual_data[1]);      // D:  (1-31)
                int month = bcd_to_byte(actual_data[2]);    // M:  (1-12)
                int year = bcd_to_byte(actual_data[3]);     // Y:  (0-99, 20xx)

                bool is_valid =
                    (week_day <= 6) && (day >= 1 && day <= 31) && (month >= 1 && month <= 12) && (year <= 99);

                char datetime_str[64];

                if (is_valid) {
                    // ：25 -> 2025
                    int full_year = (year < 50) ? 2000 + year : 1900 + year;

                    const char* weekdays[] = {"", "", "", "", "", "", ""};

                    snprintf(datetime_str, sizeof(datetime_str), "%04d-%02d-%02d (%s)", full_year, month, day,
                             weekdays[week_day]);

                    ESP_LOGD(TAG, "📅 [-4WDMY] %s", datetime_str);

                    this->cached_year_ = full_year;
                    this->cached_month_ = month;
                    this->cached_day_ = day;
                    this->cached_weekday_ = week_day + 1; // 1-7 (1=, 7=)
                } else {
                    // ，
                    snprintf(datetime_str, sizeof(datetime_str), "INVALID_WDMY: W%02d-D%02d-M%02d-Y%02d", week_day, day,
                             month, year);
                    ESP_LOGW(TAG, "❌ : %s", datetime_str);
                }

                // （）
                this->cached_datetime_str_ = std::string(datetime_str);
                xEventGroupSetBits(this->event_group_, EVENT_DI_DATETIME);
            } else if (actual_data.size() >= 6) {
                // 6：DL/T 645-2007
                char datetime_str[32];
                snprintf(datetime_str, sizeof(datetime_str), "%02X%02X%02X%02X%02X%02X", actual_data[1], actual_data[0],
                         actual_data[2], actual_data[3], actual_data[4], actual_data[5]);

                ESP_LOGD(TAG, "📅 [-6+] %s", datetime_str);

                // （）
                this->cached_datetime_str_ = std::string(datetime_str);
                xEventGroupSetBits(this->event_group_, EVENT_DI_DATETIME);
            } else {
                ESP_LOGW(TAG, "❌ : %d  - : %s", actual_data.size(), hex_str.c_str());
            }
            break;
        }

        case DLT645_DATA_IDENTIFIER::TIME_HMS: { //
            if (actual_data.size() >= 3) {
                // BCDlambda
                auto bcd_to_byte = [](uint8_t bcd) -> int { return ((bcd >> 4) & 0x0F) * 10 + (bcd & 0x0F); };

                int hour = bcd_to_byte(actual_data[0]);
                int minute = bcd_to_byte(actual_data[1]);
                int second = bcd_to_byte(actual_data[2]);

                char time_hms_str[16];
                snprintf(time_hms_str, sizeof(time_hms_str), "%02d%02d%02d", hour, minute, second);

                ESP_LOGD(TAG, "⏰ [] %s", time_hms_str);

                this->cached_hour_ = hour;
                this->cached_minute_ = minute;
                this->cached_second_ = second;

                // （）
                this->cached_time_hms_str_ = std::string(time_hms_str);
                xEventGroupSetBits(this->event_group_, EVENT_DI_TIME_HMS);
            } else {
                ESP_LOGW(TAG, "⚠️ ");
            }
            break;
        }

        default: {
            ESP_LOGW(TAG, "⚠️ : 0x%08X", data_identifier);
            break;
        }
    }
}

// =============  =============

size_t DLT645Component::get_next_event_index(size_t current_index, size_t max_events)
{
    if (false == this->device_address_discovered_) {
        // ，0
        return 0;
    }
    // （）
    size_t next_index = (current_index + 1) % max_events;

    // ，（index 0）
    if (next_index == 0 && this->device_address_discovered_) {
        ESP_LOGD(TAG, "⏭️ ，");
        next_index = 1; // （）
    }

    // === ： vs  = N:1 ===
    if (this->device_address_discovered_) {
        // 1:  (index=1)
        if (current_index == 1) {
            this->total_power_query_count_++;

            // N，
            if (this->total_power_query_count_ < this->power_ratio_) {
                ESP_LOGD(TAG, "🔋  (%d/%d)", this->total_power_query_count_, this->power_ratio_);
                next_index = 1; //
            } else {
                // N，
                ESP_LOGD(TAG, "🔄 Total power query ratio (%d)，", this->power_ratio_);

                this->total_power_query_count_ = 0;

                next_index = this->last_non_power_query_index_;

                // （）
                this->last_non_power_query_index_ = (this->last_non_power_query_index_ + 1);

                // index 0,1，index 2（）
                if (this->last_non_power_query_index_ >= max_events || this->last_non_power_query_index_ <= 1) {
                    this->last_non_power_query_index_ = 2;
                }
            }
        }
        // 2:  ()，
        else if (current_index >= 2) {
            ESP_LOGD(TAG, "🔄  (index=%d)，", current_index);
            next_index = 1; //
        }
    }

    if (next_index == 0) {
        ESP_LOGD(TAG, "🔄 DL/T 645，...");
    } else if (next_index == 1 && this->device_address_discovered_) {
        ESP_LOGD(TAG, "⚡  (index=1)");
    } else if (next_index >= 2) {
        ESP_LOGD(TAG, "📊  (index=%d)", next_index);
    }

    if (this->device_address_discovered_) {
        ESP_LOGD(TAG, "📊  - : %d/%d, : %d", this->total_power_query_count_, this->power_ratio_,
                 this->last_non_power_query_index_);
    }

    return next_index;
}

#endif // defined(USE_ESP32) || defined(USE_ESP_IDF)

} // namespace dlt645_component
} // namespace esphome
