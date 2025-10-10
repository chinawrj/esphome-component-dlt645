#include "dlt645.h"

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cassert>
#include <cstdint>

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

struct dlt645_request_info {
    const char* name;
    enum DLT645_REQUEST_TYPE request_type;
    union {
        enum DLT645_DATA_IDENTIFIER data_identifier;
    };
};

static struct dlt645_request_info dlt645_request_infos[] = {
    //read Data Identifier section
    {"Device Address", DLT645_REQUEST_TYPE::READ_DEVICE_ADDRESS, DLT645_DATA_IDENTIFIER::DEVICE_ADDRESS},
    {"Active Power Total", DLT645_REQUEST_TYPE::READ_ACTIVE_POWER_TOTAL, DLT645_DATA_IDENTIFIER::ACTIVE_POWER_TOTAL},
    {"Energy Active Total", DLT645_REQUEST_TYPE::READ_ENERGY_ACTIVE_TOTAL, DLT645_DATA_IDENTIFIER::ENERGY_ACTIVE_TOTAL},
    {"Voltage A Phase", DLT645_REQUEST_TYPE::READ_VOLTAGE_A_PHASE, DLT645_DATA_IDENTIFIER::VOLTAGE_A_PHASE},
    {"Current A Phase", DLT645_REQUEST_TYPE::READ_CURRENT_A_PHASE, DLT645_DATA_IDENTIFIER::CURRENT_A_PHASE},
    {"Power Factor Total", DLT645_REQUEST_TYPE::READ_POWER_FACTOR_TOTAL, DLT645_DATA_IDENTIFIER::POWER_FACTOR_TOTAL},
    {"Frequency", DLT645_REQUEST_TYPE::READ_FREQUENCY, DLT645_DATA_IDENTIFIER::FREQUENCY},
    {"Energy Reverse Total", DLT645_REQUEST_TYPE::READ_ENERGY_REVERSE_TOTAL, DLT645_DATA_IDENTIFIER::ENERGY_REVERSE_TOTAL},
    {"DateTime", DLT645_REQUEST_TYPE::READ_DATE, DLT645_DATA_IDENTIFIER::DATETIME},
    {"Time HMS", DLT645_REQUEST_TYPE::READ_TIME, DLT645_DATA_IDENTIFIER::TIME_HMS},
    //write Data Identifier section
    {"Write Date", DLT645_REQUEST_TYPE::WRITE_DATE, DLT645_DATA_IDENTIFIER::DATETIME},
    {"Write Time", DLT645_REQUEST_TYPE::WRITE_TIME, DLT645_DATA_IDENTIFIER::TIME_HMS},
    //control command section
    {"Relay Connect", DLT645_REQUEST_TYPE::CONTROL_RELAY_CONNECT, DLT645_DATA_IDENTIFIER::UNKNOWN},
    {"Relay Disconnect", DLT645_REQUEST_TYPE::CONTROL_RELAY_DISCONNECT, DLT645_DATA_IDENTIFIER::UNKNOWN},
};


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

    const size_t num_dlt645_events = sizeof(dlt645_event_bits) / sizeof(dlt645_event_bits[0]);
    
    // 初始化组件的事件管理参数
    component->set_max_events(num_dlt645_events);

    ESP_LOGI(TAG, "📋 DL/T 645 event loop configured with %d data identifiers", num_dlt645_events);

    // === DL/T 645-2007 UART ===
    const uint32_t UART_READ_INTERVAL_MS = 5;

    //  - DL/T 645 +
    while (component->task_running_) {
        // === 2. DL/T 645 （1s）===
        enum DLT645_REQUEST_TYPE next_request_type = component->get_next_event_index();
        size_t request_index = sizeof(dlt645_request_infos) / sizeof(dlt645_request_infos[0]);
        for (size_t i = 0; i < sizeof (dlt645_request_infos) / sizeof(dlt645_request_infos[0]); i++) {
            if (dlt645_request_infos[i].request_type == next_request_type) {
                request_index = i;
                break;
            }
        }
        if (request_index == sizeof(dlt645_request_infos) / sizeof(dlt645_request_infos[0])) {
            ESP_LOGE(TAG, "❌ DL/T 645: Unknown request type %d", next_request_type);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        uint32_t data_identifier = static_cast<uint32_t>(dlt645_request_infos[request_index].data_identifier);
        const char* event_name = dlt645_request_infos[request_index].name;

        ESP_LOGD(TAG, "📡 [%d/%d] DL/T 645: %s (DI: 0x%08X)", current_index + 1, num_dlt645_events, event_name,
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
        } else {
            // Unified code path for all data identifier queries (including ACTIVE_POWER_TOTAL)
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

    // Dump frame data
    ESP_LOGI(TAG, "📦 DL/T 645 frame data: %s", hex_frame.c_str());
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
        ESP_LOGW(TAG, "📦 ，...");
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

    ESP_LOGI(TAG, "📋 Frame parsed: Address=%02X %02X %02X %02X %02X %02X, Control=0x%02X, DataLen=%d", 
             address[0], address[1], address[2], address[3], address[4], address[5], control_code, data_length);

    // Check for error responses (0xD1, 0xB1 for read errors, 0xDC, 0xBC for control errors)
    if (control_code == 0xD1 || control_code == 0xB1) {
        ESP_LOGW(TAG, "⚠️ Meter returned READ ERROR response, control code: 0x%02X", control_code);
        this->response_buffer_.clear();
        return;
    }
    
    if (control_code == 0xDC || control_code == 0xBC) {
        ESP_LOGE(TAG, "❌ Meter returned CONTROL ERROR response, control code: 0x%02X", control_code);
        // Extract error code if available
        if (data_length > 0 && data_length < 10) {
            ESP_LOGE(TAG, "   Error details: data_length=%d", data_length);
            // Print error data field
            std::string error_hex = "";
            for (int i = 0; i < data_length && (idx + 10 + i) < this->response_buffer_.size(); i++) {
                char hex_str[4];
                sprintf(hex_str, "%02X ", this->response_buffer_[idx + 10 + i]);
                error_hex += hex_str;
            }
            ESP_LOGE(TAG, "   Error data: %s", error_hex.c_str());
        }
        this->response_buffer_.clear();
        return;
    }

    // Check for valid response codes: 0x91 (read data response) or 0x9C (control command response)
    if (control_code != 0x91 && control_code != 0x9C) {
        ESP_LOGW(TAG, "⚠️ Unknown control code: 0x%02X (expected 0x91 for read or 0x9C for control)", control_code);
        this->response_buffer_.clear();
        return;
    }
    
    // Special handling for control command responses (0x9C)
    if (control_code == 0x9C) {
        if (data_length == 0) {
            ESP_LOGI(TAG, "✅ Control command executed successfully (0x9C, data_length=0)");
        } else {
            ESP_LOGI(TAG, "✅ Control command response received (0x9C, data_length=%d)", data_length);
        }
        this->response_buffer_.clear();
        return;
    }

    size_t frame_total_length = idx + 10 + data_length + 2; // +2 for checksum and end delimiter
    if (this->response_buffer_.size() < frame_total_length) {
        ESP_LOGW(TAG, "📦 Frame incomplete (expected %d bytes, got %d)", frame_total_length, this->response_buffer_.size());
        return;
    }

    if (this->response_buffer_[frame_total_length - 1] != 0x16) {
        ESP_LOGW(TAG, "⚠️ Invalid end delimiter (expected 0x16): 0x%02X", this->response_buffer_[frame_total_length - 1]);
        this->response_buffer_.clear();
        return;
    }

    uint8_t calculated_checksum = 0;
    for (size_t i = idx; i < idx + 10 + data_length; i++) {
        calculated_checksum += this->response_buffer_[i];
    }
    uint8_t received_checksum = this->response_buffer_[idx + 10 + data_length];

    if (calculated_checksum != received_checksum) {
        ESP_LOGW(TAG, "⚠️ Checksum mismatch (calculated: 0x%02X, received: 0x%02X)", calculated_checksum, received_checksum);
        this->response_buffer_.clear();
        return;
    }

    ESP_LOGD(TAG, "✅ DL/T 645 frame validation passed");

    std::vector<uint8_t> data_field(data_length);
    for (int i = 0; i < data_length; i++) {
        data_field[i] = this->response_buffer_[idx + 10 + i];
    }

    // Unscramble data field (subtract 0x33 from each byte)
    unscramble_dlt645_data(data_field);

    // Extract data identifier (4 bytes, LSB first)
    if (data_length >= 4) {
        uint32_t data_identifier = data_field[0] | (data_field[1] << 8) | (data_field[2] << 16) | (data_field[3] << 24);

        ESP_LOGD(TAG, "🎯 Data Identifier: 0x%08X", data_identifier);

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

// ============= DL/T 645-2007 Frame Builder =============

/**
 * @brief Build DL/T 645-2007 read data command frame (Master station request to read meter data)
 * 
 * Constructs a read data command frame according to DL/T 645-2007 protocol standard Section 7.1 "Transmission Frame Format".
 * Frame format: [Preamble] [Start] [Address] [Start] [Control] [Length] [Data] [Checksum] [End]
 * 
 * @param address Meter address (6 bytes, BCD format, LSB first / Little-Endian)
 *                - Unicast address: Specific meter address (e.g., 12 90 78 56 34 12 for meter ID 123456789012)
 *                - Broadcast address: 99 99 99 99 99 99 or AA AA AA AA AA AA
 * @param data_identifier Data identifier (4 bytes, LSB first), refer to DL/T 645-2007 Appendix A
 *                        Examples: 0x02010100 = Phase A Voltage
 *                                 0x02030000 = Total Active Power
 *                                 0x00010000 = Total Active Energy
 * @return Complete DL/T 645-2007 read data command frame ready for UART transmission
 */
std::vector<uint8_t> DLT645Component::build_dlt645_read_frame(const std::vector<uint8_t>& address,
                                                              uint32_t data_identifier)
{
    std::vector<uint8_t> frame;

    // 1. Preamble (0xFE) - Protocol Section 7.1.1
    // Purpose: Wake up receiver and prepare for data reception (optional, 0-4 bytes)
    // Some meters don't require preamble, some need 1-4 preamble bytes
    frame.push_back(0xFE);
    frame.push_back(0xFE);

    // 2. Start delimiter (0x68) - Protocol Section 7.1.2
    // Marks the beginning of a frame
    frame.push_back(0x68);

    // 3. Address field A0-A5 (6 bytes) - Protocol Section 7.1.3
    // BCD format, low byte first (Little-Endian)
    // Example: Meter ID 123456789012 → A0=12H A1=90H A2=78H A3=56H A4=34H A5=12H
    for (size_t i = 0; i < 6 && i < address.size(); i++) {
        frame.push_back(address[i]);
    }

    // 4. Second start delimiter (0x68) - Protocol Section 7.1.2
    // Enhances transmission reliability, helps verify correct address field reception
    frame.push_back(0x68);

    // 5. Control code C (1 byte) - Protocol Section 7.1.4
    // Bit7-6: Transfer direction (0=sent by master station)
    // Bit5: Slave abnormal response flag (0=normal)  
    // Bit4: Subsequent frame flag (0=no subsequent frames)
    // Bit3-0: Function code (0001=read data)
    // 0x11 = 0001 0001 = Master station read data command
    frame.push_back(0x11);

    // 6. Data length L (1 byte) - Protocol Section 7.1.5
    // Number of bytes in data field, fixed at 4 for read data command (data identifier length)
    frame.push_back(0x04);

    // 7. Data field DATA - Protocol Section 7.1.6
    // Data identifier DI3 DI2 DI1 DI0 (4 bytes, low byte first)
    // CRITICAL: Each byte must be scrambled by adding 0x33 during transmission (Protocol Section 7.1.6.1)
    // Example: DI=0x02010100 → Transmitted as 33 34 34 35
    uint8_t di_bytes[4];
    di_bytes[0] = (data_identifier & 0xFF) + 0x33;         // DI0 + 0x33
    di_bytes[1] = ((data_identifier >> 8) & 0xFF) + 0x33;  // DI1 + 0x33
    di_bytes[2] = ((data_identifier >> 16) & 0xFF) + 0x33; // DI2 + 0x33
    di_bytes[3] = ((data_identifier >> 24) & 0xFF) + 0x33; // DI3 + 0x33

    for (int i = 0; i < 4; i++) {
        frame.push_back(di_bytes[i]);
    }

    // 8. Checksum CS (1 byte) - Protocol Section 7.1.7
    // Modulo 256 sum of all bytes from first start delimiter (0x68) to checksum (exclusive)
    // Preamble bytes are NOT included in checksum calculation
    uint8_t checksum = 0;
    for (size_t i = 2; i < frame.size(); i++) { // Skip 2 preamble bytes (0xFE)
        checksum += frame[i];
    }
    frame.push_back(checksum);

    // 9. End delimiter (0x16) - Protocol Section 7.1.8
    // Marks the end of a frame
    frame.push_back(0x16);

    // Debug log: Display constructed frame information
    ESP_LOGD(TAG, "🔧 Build DL/T 645 read frame: Address=%02X%02X%02X%02X%02X%02X, DataID=0x%08X", address[0], address[1], address[2], address[3],
             address[4], address[5], data_identifier);

    return frame;
}

/**
 * @brief Build DL/T 645-2007 generic write data command frame
 * 
 * Constructs a write data command frame according to DL/T 645-2007 protocol standard.
 * This is a generic write frame builder that can be used for various write operations:
 * - Setting date/time (DI: 0x04000101)
 * - Modifying parameters
 * - Password configuration
 * - Any other write operations defined in DL/T 645-2007
 * 
 * Frame format: [Preamble] [Start] [Address] [Start] [Control] [Length] [Data] [Checksum] [End]
 * 
 * Control Code: 0x14 (Write data command)
 * Data Field Structure:
 *   - DI3-DI0 (4 bytes): Data identifier (LSB first)
 *   - Data (n bytes): Data content to write (will be scrambled with +0x33)
 * 
 * @param address Meter address (6 bytes, BCD format, LSB first)
 * @param data_identifier Data identifier (4 bytes, LSB first), refer to DL/T 645-2007 Appendix A
 * @param write_data Raw data to write (will be automatically scrambled inside this function)
 * @return Complete DL/T 645-2007 write data command frame ready for UART transmission
 * 
 * @note Data will be automatically scrambled (+0x33) before transmission
 * @warning Broadcast address (AA AA AA AA AA AA or 99 99 99 99 99 99) is NOT allowed for write operations
 * @warning Some meters may require password authorization for certain write operations
 * 
 * @example Setting date/time (2025-10-10 15:30):
 *   std::vector<uint8_t> datetime_data = {0x19, 0x0A, 0x0A, 0x0F, 0x1E}; // YY MM DD HH mm (BCD, not scrambled)
 *   auto frame = build_dlt645_write_frame(address, 0x04000101, datetime_data);
 */
std::vector<uint8_t> DLT645Component::build_dlt645_write_frame(const std::vector<uint8_t>& address,
                                                                uint32_t data_identifier,
                                                                const std::vector<uint8_t>& write_data)
{
    std::vector<uint8_t> frame;

    // 1. Preamble (0xFE) - Wake up receiver
    frame.insert(frame.end(), {0xFE, 0xFE, 0xFE, 0xFE});

    // 2. Start delimiter (0x68)
    frame.push_back(0x68);

    // 3. Address field (6 bytes)
    for (size_t i = 0; i < 6 && i < address.size(); i++) {
        frame.push_back(address[i]);
    }

    // 4. Second start delimiter (0x68)
    frame.push_back(0x68);

    // 5. Control code: 0x14 (Write data command)
    // 0x14 = 0001 0100
    // Bit7-6: 00 = Master station
    // Bit5: 0 = Normal operation
    // Bit4: 1 = Has data
    // Bit3-0: 0100 = Write data
    frame.push_back(0x14);

    // 6. Data length: 4 bytes (DI) + write_data.size()
    uint8_t data_length = 4 + write_data.size();
    frame.push_back(data_length);

    // 7. Data field (will be scrambled with +0x33)
    // DI3-DI0: Data identifier (4 bytes, LSB first)
    frame.push_back((data_identifier & 0xFF) + 0x33);         // DI0
    frame.push_back(((data_identifier >> 8) & 0xFF) + 0x33);  // DI1
    frame.push_back(((data_identifier >> 16) & 0xFF) + 0x33); // DI2
    frame.push_back(((data_identifier >> 24) & 0xFF) + 0x33); // DI3

    // Data content (scramble each byte with +0x33)
    for (size_t i = 0; i < write_data.size(); i++) {
        frame.push_back(write_data[i] + 0x33);
    }

    // 8. Checksum CS (1 byte)
    // Calculate from first 0x68 (skip 4 preamble bytes)
    uint8_t checksum = 0;
    for (size_t i = 4; i < frame.size(); i++) {  // Start from first 0x68
        checksum += frame[i];
    }
    frame.push_back(checksum);

    // 9. End delimiter (0x16)
    frame.push_back(0x16);

    // Debug log
    ESP_LOGD(TAG, "🔧 Build DL/T 645 generic write frame: Address=%02X%02X%02X%02X%02X%02X, DI=0x%08X, DataLen=%d",
             address[0], address[1], address[2], address[3], address[4], address[5],
             data_identifier, write_data.size());

    return frame;
}

/**
 * @brief Build DL/T 645-2007 date/time write command frame (Set meter date and time)
 * 
 * This is a convenience wrapper that uses the generic build_dlt645_write_frame() function.
 * Automatically retrieves current system time and formats it as BCD data.
 * 
 * @param address Meter address (6 bytes, BCD format, LSB first)
 * @return Complete DL/T 645-2007 write datetime command frame ready for UART transmission
 * 
 * @note Automatically retrieves current system time via time() function
 * @warning Some meters may require password authorization for write operations
 * @warning Broadcast address is NOT allowed for write operations
 */
std::vector<uint8_t> DLT645Component::build_dlt645_write_datetime_frame(const std::vector<uint8_t>& address)
{
    // Prepare date data in BCD format (4 bytes: WW DD MM YY)
    // DL/T 645-2007: DI=0x04000101 uses 4-byte format for DATE ONLY
    std::vector<uint8_t> datetime_data;

#if defined(USE_ESP32) || defined(USE_ESP_IDF)
    // Get current system time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Convert to BCD format (NOT scrambled yet - will be scrambled in build_dlt645_write_frame)
    
    // Byte 0: Week (WW) - day of week (0=Sunday, 1=Monday, ..., 6=Saturday)
    // tm_wday: 0=Sunday, 1=Monday, ..., 6=Saturday
    uint8_t week_bcd = ((timeinfo.tm_wday / 10) << 4) | (timeinfo.tm_wday % 10);
    datetime_data.push_back(week_bcd);

    // Byte 1: Day (DD) - day of month
    uint8_t day_bcd = ((timeinfo.tm_mday / 10) << 4) | (timeinfo.tm_mday % 10);
    datetime_data.push_back(day_bcd);

    // Byte 2: Month (MM)
    int month = timeinfo.tm_mon + 1;  // tm_mon is 0-11
    uint8_t mon_bcd = ((month / 10) << 4) | (month % 10);
    datetime_data.push_back(mon_bcd);

    // Byte 3: Year (YY) - last 2 digits
    int year = (timeinfo.tm_year + 1900) % 100;
    uint8_t year_bcd = ((year / 10) << 4) | (year % 10);
    datetime_data.push_back(year_bcd);

    // Convert day of week to string for logging
    const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    ESP_LOGI(TAG, "� Setting meter date: 20%02d-%02d-%02d (%s)", 
             year, month, timeinfo.tm_mday, weekdays[timeinfo.tm_wday]);
#else
    // Non-ESP32 platform: use dummy date (Thursday, 2025-10-10)
    datetime_data = {0x04, 0x10, 0x10, 0x25};  // WW DD MM YY (BCD, not scrambled)
    ESP_LOGW(TAG, "⚠️ Non-ESP32 platform: using dummy date Thu 2025-10-10");
#endif

    // Use generic write frame builder with DI=0x04000101 (Date only - 4 bytes)
    return build_dlt645_write_frame(address, 0x04000101, datetime_data);
}

/**
 * @brief Build DL/T 645-2007 time write command frame (Set meter time only)
 * 
 * This is a convenience wrapper that uses the generic build_dlt645_write_frame() function.
 * Automatically retrieves current system time and formats it as BCD data.
 * 
 * DL/T 645-2007: DI=0x04000102 uses 3-byte format for TIME ONLY (HH mm SS)
 * 
 * @param address Meter address (6 bytes, BCD format, LSB first)
 * @return Complete DL/T 645-2007 write time command frame ready for UART transmission
 * 
 * @note Automatically retrieves current system time via time() function
 * @warning Some meters may require password authorization for write operations
 * @warning Broadcast address is NOT allowed for write operations
 */
std::vector<uint8_t> DLT645Component::build_dlt645_write_time_frame(const std::vector<uint8_t>& address)
{
    // Prepare time data in BCD format (3 bytes: HH mm SS)
    // DL/T 645-2007: DI=0x04000102 uses 3-byte format for TIME ONLY
    std::vector<uint8_t> time_data;

#if defined(USE_ESP32) || defined(USE_ESP_IDF)
    // Get current system time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Convert to BCD format (NOT scrambled yet - will be scrambled in build_dlt645_write_frame)
    
    // Byte 0: Hour (HH)
    uint8_t hour_bcd = ((timeinfo.tm_hour / 10) << 4) | (timeinfo.tm_hour % 10);
    time_data.push_back(hour_bcd);

    // Byte 1: Minute (mm)
    uint8_t min_bcd = ((timeinfo.tm_min / 10) << 4) | (timeinfo.tm_min % 10);
    time_data.push_back(min_bcd);

    // Byte 2: Second (SS)
    uint8_t sec_bcd = ((timeinfo.tm_sec / 10) << 4) | (timeinfo.tm_sec % 10);
    time_data.push_back(sec_bcd);

    ESP_LOGI(TAG, "🕐 Setting meter time: %02d:%02d:%02d", 
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
#else
    // Non-ESP32 platform: use dummy time (12:00:00)
    time_data = {0x12, 0x00, 0x00};  // HH mm SS (BCD, not scrambled)
    ESP_LOGW(TAG, "⚠️ Non-ESP32 platform: using dummy time 12:00:00");
#endif

    // Use generic write frame builder with DI=0x04000102 (Time only - 3 bytes)
    return build_dlt645_write_frame(address, 0x04000102, time_data);
}

/**
 * @brief Build DL/T 645-2007 broadcast time synchronization frame (Control Code 0x08)
 * 
 * Constructs a broadcast time synchronization frame according to DL/T 645-2007 protocol.
 * This command uses Control Code 0x08 for one-shot time sync (year, month, day, hour, minute).
 * 
 * Frame format: [Preamble] [Start] [Address] [Start] [Control=0x08] [Length=05] [Data] [Checksum] [End]
 * 
 * Control Code: 0x08 (Broadcast time synchronization)
 * Data Field Structure (5 bytes before scrambling, little-endian BCD):
 *   - Byte 0: Year (YY) - Last 2 digits, e.g., 2025 → 0x25
 *   - Byte 1: Month (MM) - 01-12, e.g., October → 0x10
 *   - Byte 2: Day (DD) - 01-31, e.g., 10th → 0x10
 *   - Byte 3: Hour (HH) - 00-23
 *   - Byte 4: Minute (mm) - 00-59
 * 
 * Data Scrambling: All data bytes are scrambled by adding 0x33 before transmission
 * 
 * @param address Broadcast address (typically 0x99 repeated 6 times for broadcast)
 * @return Complete DL/T 645-2007 broadcast time sync frame ready for UART transmission
 * 
 * @note This command is typically sent to broadcast address (99 99 99 99 99 99)
 * @note Format follows GitHub implementation: https://github.com/600888/dlt645
 * @warning Some meters may reject broadcast time sync if not authorized
 */
std::vector<uint8_t> DLT645Component::build_dlt645_broadcast_time_sync_frame(const std::vector<uint8_t>& address)
{
    std::vector<uint8_t> frame;

#if defined(USE_ESP32) || defined(USE_ESP_IDF)
    // Get current system time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // 1. Preamble (0xFE) - Wake up receiver (optional but recommended for broadcast)
    frame.insert(frame.end(), {0xFE, 0xFE, 0xFE, 0xFE});

    // 2. Start delimiter (0x68)
    frame.push_back(0x68);

    // 3. Address field (6 bytes) - typically broadcast address
    for (size_t i = 0; i < 6 && i < address.size(); i++) {
        frame.push_back(address[i]);
    }

    // 4. Second start delimiter (0x68)
    frame.push_back(0x68);

    // 5. Control code: 0x08 (Broadcast time synchronization)
    frame.push_back(0x08);

    // 6. Data length: 5 bytes
    frame.push_back(0x05);

    // 7. Data field (5 bytes, scrambled with +0x33)
    // Byte 0: Year (last 2 digits) - BCD format
    int year = timeinfo.tm_year + 1900;  // tm_year is years since 1900
    uint8_t year_bcd = ((year % 100) / 10 << 4) | ((year % 100) % 10);
    frame.push_back(year_bcd + 0x33);  // Scramble

    // Byte 1: Month (01-12) - BCD format
    int month = timeinfo.tm_mon + 1;  // tm_mon is 0-11
    uint8_t month_bcd = ((month / 10) << 4) | (month % 10);
    frame.push_back(month_bcd + 0x33);  // Scramble

    // Byte 2: Day (01-31) - BCD format
    uint8_t day_bcd = ((timeinfo.tm_mday / 10) << 4) | (timeinfo.tm_mday % 10);
    frame.push_back(day_bcd + 0x33);  // Scramble

    // Byte 3: Hour (00-23) - BCD format
    uint8_t hour_bcd = ((timeinfo.tm_hour / 10) << 4) | (timeinfo.tm_hour % 10);
    frame.push_back(hour_bcd + 0x33);  // Scramble

    // Byte 4: Minute (00-59) - BCD format
    uint8_t min_bcd = ((timeinfo.tm_min / 10) << 4) | (timeinfo.tm_min % 10);
    frame.push_back(min_bcd + 0x33);  // Scramble

    // 8. Calculate checksum (modulo 256 sum from first 0x68 to last data byte)
    uint8_t checksum = 0;
    for (size_t i = 4; i < frame.size(); i++) {  // Skip preamble
        checksum += frame[i];
    }
    frame.push_back(checksum);

    // 9. End delimiter (0x16)
    frame.push_back(0x16);

    ESP_LOGI(TAG, "📡 Broadcast time sync: %04d-%02d-%02d %02d:%02d", 
             year, month, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min);
    ESP_LOGI(TAG, "   Frame size: %d bytes, Control Code: 0x08", frame.size());

#else
    // Non-ESP32 platform: return empty frame
    ESP_LOGE(TAG, "⚠️ Broadcast time sync not supported on non-ESP32 platforms");
#endif

    return frame;
}

/**
 * @brief Build DL/T 645-2007 relay control command frame (Trip/Close relay command)
 * 
 * Constructs a relay control command frame according to DL/T 645-2007 protocol.
 * Frame format: [Preamble] [Start] [Address] [Start] [Control] [Length] [Data] [Checksum] [End]
 * 
 * Control Code: 0x1C (Remote control command)
 * Data Field Structure (16 bytes before scrambling):
 *   - PA (1 byte): Password authority level (default: 0x02)
 *   - P0-P2 (3 bytes): Password (default: 0x00 0x00 0x00)
 *   - C0-C3 (4 bytes): Operator code (default: 0x00 0x00 0x00 0x00)
 *   - N1 (1 byte): Command type (0x1A=trip/open, 0x1C=close)
 *   - N2 (1 byte): Command parameter (default: 0x00)
 *   - N3-N8 (6 bytes): Timestamp (BCD format: SS MM HH DD MM YY)
 * 
 * @param address Meter address (6 bytes, BCD format, LSB first)
 * @param close_relay true=close relay, false=trip/open relay
 * @return Complete DL/T 645-2007 relay control command frame ready for UART transmission
 * 
 * @note This command requires proper password authorization (default level 0x02)
 * @warning Timestamp must be current or future time, past time will be rejected by meter
 */
std::vector<uint8_t> DLT645Component::build_dlt645_relay_control_frame(const std::vector<uint8_t>& address,
                                                                       bool close_relay)
{
    std::vector<uint8_t> frame;

    // 1. Preamble (0xFE) - Wake up receiver
    frame.insert(frame.end(), {0xFE, 0xFE, 0xFE, 0xFE});

    // 2. Start delimiter (0x68)
    frame.push_back(0x68);

    // 3. Address field (6 bytes)
    for (size_t i = 0; i < 6 && i < address.size(); i++) {
        frame.push_back(address[i]);
    }

    // 4. Second start delimiter (0x68)
    frame.push_back(0x68);

    // 5. Control code: 0x1C (Remote control command)
    // 0x1C = 0001 1100
    // Bit7-6: 00 = Master station
    // Bit5: 0 = Normal response
    // Bit4: 1 = Has subsequent frame
    // Bit3-0: 1100 = Control command
    frame.push_back(0x1C);

    // 6. Data length: 16 bytes (0x10)
    frame.push_back(0x10);

    // 7. Data field (16 bytes, will be scrambled with +0x33)
    // PA: Password authority level (0x02)
    frame.push_back(0x02 + 0x33);  // = 0x35

    // P0-P2: Password (BCD format: 123456)
    // 123456 in BCD = 0x12 0x34 0x56
    frame.push_back(0x56 + 0x33);  // P0: Low 2 digits (56) = 0x89
    frame.push_back(0x34 + 0x33);  // P1: Middle 2 digits (34) = 0x67
    frame.push_back(0x12 + 0x33);  // P2: High 2 digits (12) = 0x45

    // C0-C3: Operator code (default: 00000000)
    for (int i = 0; i < 4; i++) {
        frame.push_back(0x00 + 0x33);  // = 0x33
    }

    // N1: Command type
    if (close_relay) {
        frame.push_back(0x1C + 0x33);  // Close relay = 0x1C + 0x33 = 0x4F
        ESP_LOGI(TAG, "🔌 Building CLOSE relay command");
    } else {
        frame.push_back(0x1A + 0x33);  // Trip/Open relay = 0x1A + 0x33 = 0x4D
        ESP_LOGI(TAG, "⚡ Building TRIP/OPEN relay command");
    }

    // N2: Command parameter (reserved, default: 0x00)
    frame.push_back(0x00 + 0x33);  // = 0x33

    // N3-N8: Timestamp (BCD format: SS MM HH DD MM YY)
    // Get current time
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Convert to BCD and add scrambling
    // N3: Second (SS)
    uint8_t sec_bcd = ((timeinfo.tm_sec / 10) << 4) | (timeinfo.tm_sec % 10);
    frame.push_back(sec_bcd + 0x33);

    // N4: Minute (MM)
    uint8_t min_bcd = ((timeinfo.tm_min / 10) << 4) | (timeinfo.tm_min % 10);
    frame.push_back(min_bcd + 0x33);

    // N5: Hour (HH)
    uint8_t hour_bcd = ((timeinfo.tm_hour / 10) << 4) | (timeinfo.tm_hour % 10);
    frame.push_back(hour_bcd + 0x33);

    // N6: Day (DD)
    uint8_t day_bcd = ((timeinfo.tm_mday / 10) << 4) | (timeinfo.tm_mday % 10);
    frame.push_back(day_bcd + 0x33);

    // N7: Month (MM)
    int month = timeinfo.tm_mon + 1;  // tm_mon is 0-11
    uint8_t mon_bcd = ((month / 10) << 4) | (month % 10);
    frame.push_back(mon_bcd + 0x33);

    // N8: Year (YY) - last 2 digits
    int year = (timeinfo.tm_year + 1900) % 100;
    uint8_t year_bcd = ((year / 10) << 4) | (year % 10);
    frame.push_back(year_bcd + 0x33);

    ESP_LOGI(TAG, "📅 Timestamp: 20%02d-%02d-%02d %02d:%02d:%02d", year, month, timeinfo.tm_mday, 
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
#else
    // Non-ESP32 platform: use dummy timestamp
    for (int i = 0; i < 6; i++) {
        frame.push_back(0x00 + 0x33);
    }
#endif

    // 8. Checksum CS (1 byte)
    // Calculate from first 0x68 (skip 4 preamble bytes)
    uint8_t checksum = 0;
    for (size_t i = 4; i < frame.size(); i++) {  // Start from first 0x68
        checksum += frame[i];
    }
    frame.push_back(checksum);

    // 9. End delimiter (0x16)
    frame.push_back(0x16);

    // Debug log
    ESP_LOGD(TAG, "🔧 Build DL/T 645 relay control frame: Address=%02X%02X%02X%02X%02X%02X, Command=%s",
             address[0], address[1], address[2], address[3], address[4], address[5],
             close_relay ? "CLOSE" : "TRIP/OPEN");

    return frame;
}

// Data field scrambling/unscrambling functions
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

// BCD to float conversion
float DLT645Component::bcd_to_float(const std::vector<uint8_t>& bcd_data, int decimal_places)
{
    uint32_t int_value = 0;
    uint32_t multiplier = 1;

    for (size_t i = 0; i < bcd_data.size(); i++) {
        uint8_t low_nibble = bcd_data[i] & 0x0F;
        uint8_t high_nibble = (bcd_data[i] >> 4) & 0x0F;

        // Invalid BCD check
        if (low_nibble > 9 || high_nibble > 9) {
            ESP_LOGW(TAG, "⚠️ Invalid BCD digit: 0x%02X", bcd_data[i]);
            return 0.0f;
        }

        int_value += low_nibble * multiplier;
        multiplier *= 10;
        int_value += high_nibble * multiplier;
        multiplier *= 10;
    }

    return (float)int_value / pow(10, decimal_places);
}

// DL/T 645-2007 BCD to float with sign bit support
float DLT645Component::bcd_to_float_with_sign(const std::vector<uint8_t>& bcd_data, int decimal_places)
{
    if (bcd_data.empty()) {
        ESP_LOGW(TAG, "⚠️ Empty BCD data");
        return 0.0f;
    }

    // Check sign bit (highest bit of last byte)
    bool is_negative = (bcd_data.back() & 0x80) != 0;

    std::vector<uint8_t> clean_bcd_data = bcd_data;
    clean_bcd_data.back() &= 0x7F; // Clear sign bit

    ESP_LOGD(TAG, "📊 BCD sign parsing: original=0x%02X, clean=0x%02X, negative=%s", 
             bcd_data.back(), clean_bcd_data.back(), is_negative ? "yes" : "no");

    // Convert BCD to float
    float result = bcd_to_float(clean_bcd_data, decimal_places);

    return is_negative ? -result : result;
}

// ============= DL/T 645-2007 Protocol Functions =============

bool DLT645Component::discover_meter_address()
{
    if (!this->uart_initialized_) {
        ESP_LOGE(TAG, "❌ UART not initialized, cannot discover address");
        return false;
    }

    ESP_LOGD(TAG, "🔍 Discovering DL/T 645 meter address...");

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

bool DLT645Component::relay_trip_action()
{
    if (!this->uart_initialized_) {
        ESP_LOGE(TAG, "❌ UART not initialized, cannot execute relay trip operation");
        return false;
    }

    if (this->meter_address_bytes_.empty() ||
        (this->meter_address_bytes_.size() == 6 && this->meter_address_bytes_[0] == 0x99)) {
        ESP_LOGE(TAG, "❌ Meter address not discovered, cannot execute relay trip operation");
        return false;
    }

    ESP_LOGW(TAG, "⚡ Executing relay TRIP/OPEN operation...");

    // Use current meter address
    std::vector<uint8_t> meter_address = this->meter_address_bytes_;

    ESP_LOGI(TAG, "📡 Sending TRIP command to meter address: %02X %02X %02X %02X %02X %02X",
             meter_address[0], meter_address[1], meter_address[2],
             meter_address[3], meter_address[4], meter_address[5]);

    // Build trip command frame (false = trip/open)
    std::vector<uint8_t> trip_frame = build_dlt645_relay_control_frame(meter_address, false);

    // Send command and wait for response
    bool success = send_dlt645_frame(trip_frame, this->frame_timeout_ms_);

    if (success) {
        ESP_LOGW(TAG, "✅ TRIP command sent, waiting for meter response...");
    } else {
        ESP_LOGE(TAG, "❌ TRIP command send failed");
    }

    return success;
}

bool DLT645Component::relay_close_action()
{
    if (!this->uart_initialized_) {
        ESP_LOGE(TAG, "❌ UART not initialized, cannot execute relay close operation");
        return false;
    }

    if (this->meter_address_bytes_.empty() ||
        (this->meter_address_bytes_.size() == 6 && this->meter_address_bytes_[0] == 0x99)) {
        ESP_LOGE(TAG, "❌ Meter address not discovered, cannot execute relay close operation");
        return false;
    }

    ESP_LOGI(TAG, "🔌 Executing relay CLOSE operation...");

    // Use current meter address
    std::vector<uint8_t> meter_address = this->meter_address_bytes_;

    ESP_LOGI(TAG, "📡 Sending CLOSE command to meter address: %02X %02X %02X %02X %02X %02X",
             meter_address[0], meter_address[1], meter_address[2],
             meter_address[3], meter_address[4], meter_address[5]);

    // Build close command frame (true = close)
    std::vector<uint8_t> close_frame = build_dlt645_relay_control_frame(meter_address, true);

    // Send command and wait for response
    bool success = send_dlt645_frame(close_frame, this->frame_timeout_ms_);

    if (success) {
        ESP_LOGI(TAG, "✅ CLOSE command sent, waiting for meter response...");
    } else {
        ESP_LOGE(TAG, "❌ CLOSE command send failed");
    }

    return success;
}

bool DLT645Component::set_datetime_action()
{
    if (!this->uart_initialized_) {
        ESP_LOGE(TAG, "❌ UART not initialized, cannot set meter datetime");
        return false;
    }

    if (this->meter_address_bytes_.empty() ||
        (this->meter_address_bytes_.size() == 6 && 
         (this->meter_address_bytes_[0] == 0x99 || this->meter_address_bytes_[0] == 0xAA))) {
        ESP_LOGE(TAG, "❌ Meter address not discovered or is broadcast address, cannot set datetime");
        ESP_LOGE(TAG, "   Write operations require specific meter address (broadcast not allowed)");
        return false;
    }

    ESP_LOGI(TAG, "🕐 Setting meter date and time from system time...");

    // Use current meter address
    std::vector<uint8_t> meter_address = this->meter_address_bytes_;

    ESP_LOGI(TAG, "📡 Sending SET DATETIME command to meter address: %02X %02X %02X %02X %02X %02X",
             meter_address[0], meter_address[1], meter_address[2],
             meter_address[3], meter_address[4], meter_address[5]);

    // Build write datetime command frame
    std::vector<uint8_t> datetime_frame = build_dlt645_write_datetime_frame(meter_address);

    // Send command and wait for response
    // Use standard frame timeout (1 second)
    bool success = send_dlt645_frame(datetime_frame, this->frame_timeout_ms_);

    if (success) {
        ESP_LOGI(TAG, "✅ SET DATETIME command sent, waiting for meter response...");
        ESP_LOGI(TAG, "   Expected response: Control code 0x94 (write data success)");
    } else {
        ESP_LOGE(TAG, "❌ SET DATETIME command send failed");
    }

    return success;
}

bool DLT645Component::set_time_action()
{
    if (!this->uart_initialized_) {
        ESP_LOGE(TAG, "❌ UART not initialized, cannot set meter time");
        return false;
    }

    if (this->meter_address_bytes_.empty() ||
        (this->meter_address_bytes_.size() == 6 && 
         (this->meter_address_bytes_[0] == 0x99 || this->meter_address_bytes_[0] == 0xAA))) {
        ESP_LOGE(TAG, "❌ Meter address not discovered or is broadcast address, cannot set time");
        ESP_LOGE(TAG, "   Write operations require specific meter address (broadcast not allowed)");
        return false;
    }

    ESP_LOGI(TAG, "🕐 Setting meter time from system time...");

    // Use current meter address
    std::vector<uint8_t> meter_address = this->meter_address_bytes_;

    ESP_LOGI(TAG, "📡 Sending SET TIME command to meter address: %02X %02X %02X %02X %02X %02X",
             meter_address[0], meter_address[1], meter_address[2],
             meter_address[3], meter_address[4], meter_address[5]);

    // Build write time command frame (3 bytes: HH mm SS)
    std::vector<uint8_t> time_frame = build_dlt645_write_time_frame(meter_address);

    // Send command and wait for response
    // Use standard frame timeout (1 second)
    bool success = send_dlt645_frame(time_frame, this->frame_timeout_ms_);

    if (success) {
        ESP_LOGI(TAG, "✅ SET TIME command sent (3-byte format: HH mm SS)");
        ESP_LOGI(TAG, "   Expected response: Control code 0x94 (write data success)");
    } else {
        ESP_LOGE(TAG, "❌ SET TIME command send failed");
    }

    return success;
}

/**
 * @brief Execute broadcast time synchronization (Control Code 0x08)
 * 
 * Sends a broadcast time synchronization command to all meters on the bus.
 * This is a one-shot command that sets year, month, day, hour, and minute.
 * 
 * Unlike DI-based write commands (set_datetime/set_time), broadcast time sync:
 * - Uses Control Code 0x08 (instead of 0x14 with data identifier)
 * - Targets broadcast address (99 99 99 99 99 99)
 * - Uses 5-byte format: YY MM DD HH mm (no seconds, no weekday)
 * - Does not require password authorization
 * - All meters on the bus receive and process the command simultaneously
 * 
 * Data Format (5 bytes, BCD, little-endian):
 * - Byte 0: Year (last 2 digits) - e.g., 2025 → 0x25
 * - Byte 1: Month (01-12)
 * - Byte 2: Day (01-31)
 * - Byte 3: Hour (00-23)
 * - Byte 4: Minute (00-59)
 * 
 * Reference: DL/T 645-2007 standard, GitHub implementation (600888/dlt645)
 * 
 * @return true if command sent successfully, false otherwise
 * 
 * @note This function automatically uses broadcast address (99 99 99 99 99 99)
 * @note Meter response expected: Control code 0x88 (broadcast time sync acknowledgment)
 * @warning Some meters may not respond to broadcast commands (fire-and-forget)
 */
bool DLT645Component::broadcast_time_sync()
{
    if (!this->uart_initialized_) {
        ESP_LOGE(TAG, "❌ UART not initialized, cannot broadcast time sync");
        return false;
    }

    ESP_LOGI(TAG, "📡 Broadcasting time synchronization to all meters...");
    ESP_LOGI(TAG, "   Using Control Code 0x08 (Broadcast Time Sync)");
    ESP_LOGI(TAG, "   Format: 5 bytes (YY MM DD HH mm) - No seconds, no weekday");

    // Use broadcast address (99 99 99 99 99 99)
    std::vector<uint8_t> broadcast_addr = {0x99, 0x99, 0x99, 0x99, 0x99, 0x99};

    ESP_LOGI(TAG, "📡 Broadcast address: %02X %02X %02X %02X %02X %02X",
             broadcast_addr[0], broadcast_addr[1], broadcast_addr[2],
             broadcast_addr[3], broadcast_addr[4], broadcast_addr[5]);

    // Build broadcast time sync frame
    std::vector<uint8_t> sync_frame = build_dlt645_broadcast_time_sync_frame(broadcast_addr);

    if (sync_frame.empty()) {
        ESP_LOGE(TAG, "❌ Failed to build broadcast time sync frame");
        return false;
    }

    // Send broadcast command
    // Note: Broadcast commands may not receive response from all meters
    bool success = send_dlt645_frame(sync_frame, this->frame_timeout_ms_);

    if (success) {
        ESP_LOGI(TAG, "✅ BROADCAST TIME SYNC command sent successfully");
        ESP_LOGI(TAG, "   Expected response: Control code 0x88 (or no response for fire-and-forget)");
    } else {
        ESP_LOGE(TAG, "❌ BROADCAST TIME SYNC command send failed");
    }

    return success;
}

// 解析DL/T 645数据标识符对应的数据
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
                ESP_LOGW(TAG, "⚠️ : %d", actual_data.size());
            }
            break;
        }

        default: {
            ESP_LOGW(TAG, "⚠️ : 0x%08X", data_identifier);
            break;
        }
    }
}

enum DLT645_REQUEST_TYPE DLT645Component::get_next_event_index()
{
    if (false == this->device_address_discovered_) {
        //stick to read device address until address is discovered
        this->current_request_type = DLT645_REQUEST_TYPE::READ_DEVICE_ADDRESS;
        return this->current_request_type;
    }
    //TODO create message queue during setup, and try to read current request type from it with timeout 4ms


    // Total power query with ratio control
    enum DLT645_REQUEST_TYPE next_request_type;
    this->total_power_query_count_++;
    if (this->total_power_query_count_ < this->power_ratio_) {
        ESP_LOGD(TAG, "🔋 Repeating total power query (%d/%d)", this->total_power_query_count_, this->power_ratio_);
        next_request_type = DLT645_REQUEST_TYPE::READ_ACTIVE_POWER_TOTAL;
    } else {
        ESP_LOGD(TAG, "🔄 Switching to non-power query after %d repeats", this->power_ratio_);
        this->total_power_query_count_ = 0;
        next_request_type = this->last_non_power_query_index_;
        // Advance non-power query index (cycle range: 2 to max_events_-1)
        uint32_t next_index = static_cast<uint32_t>(this->last_non_power_query_index_) + 1;
        if (next_index >= static_cast<uint32_t>(DLT645_REQUEST_TYPE::READ_POS_END)) {
            // Wrap around to voltage query (skip device address and power)
            next_index = static_cast<uint32_t>(DLT645_REQUEST_TYPE::READ_VOLTAGE_A_PHASE);
        }
        this->last_non_power_query_index_ = static_cast<enum DLT645_REQUEST_TYPE>(next_index);
    }
    this->current_request_type = next_request_type;
    return this->current_request_type;
}

#endif // defined(USE_ESP32) || defined(USE_ESP_IDF)

} // namespace dlt645_component
} // namespace esphome
