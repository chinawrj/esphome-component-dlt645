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

// FreeRTOS任务配置常量
constexpr uint32_t DLT645_TASK_STACK_SIZE = 4096;  // 4KB堆栈
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
constexpr UBaseType_t DLT645_TASK_PRIORITY = 5;    // 中等优先级
#endif
constexpr uint32_t DLT645_TRIGGER_INTERVAL_MS = 5000;  // 5秒间隔

// DL/T 645-2007 数据标识符枚举定义
enum class DLT645_DATA_IDENTIFIER : uint32_t {
  DEVICE_ADDRESS      = 0x04000401,  // 设备地址查询
  ACTIVE_POWER_TOTAL  = 0x02030000,  // 总有功功率
  ENERGY_ACTIVE_TOTAL = 0x00010000,  // 正向有功总电能
  VOLTAGE_A_PHASE     = 0x02010100,  // A相电压
  CURRENT_A_PHASE     = 0x02020100,  // A相电流
  POWER_FACTOR_TOTAL  = 0x02060000,  // 总功率因数
  FREQUENCY           = 0x02800002,  // 电网频率
  ENERGY_REVERSE_TOTAL = 0x00020000, // 反向有功总电能
  DATETIME            = 0x04000101,  // 日期时间
  TIME_HMS            = 0x04000102   // 时分秒
};

// Event Group事件位定义 - DL/T 645-2007数据标识符对应的事件位
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
const EventBits_t EVENT_GENERAL = BIT0;            // BIT0: 保持原有的一般事件
const EventBits_t EVENT_DI_DEVICE_ADDRESS = BIT1;  // BIT1: 设备地址查询
const EventBits_t EVENT_DI_ACTIVE_POWER_TOTAL = BIT2;  // BIT2: 总功率
const EventBits_t EVENT_DI_ENERGY_ACTIVE_TOTAL = BIT3; // BIT3: 总电能
const EventBits_t EVENT_DI_VOLTAGE_A_PHASE = BIT4;     // BIT4: A相电压
const EventBits_t EVENT_DI_CURRENT_A_PHASE = BIT5;     // BIT5: A相电流
const EventBits_t EVENT_DI_POWER_FACTOR_TOTAL = BIT6;  // BIT6: 功率因数
const EventBits_t EVENT_DI_FREQUENCY = BIT7;           // BIT7: 频率
const EventBits_t EVENT_DI_ENERGY_REVERSE_TOTAL = BIT8; // BIT8: 反向总电能
const EventBits_t EVENT_DI_DATETIME = BIT9;            // BIT9: 日期时间
const EventBits_t EVENT_DI_TIME_HMS = BIT10;           // BIT10: 时分秒

// 所有DL/T 645事件位的掩码 (BIT1-BIT10)
const EventBits_t ALL_DLT645_EVENTS = EVENT_DI_DEVICE_ADDRESS | EVENT_DI_ACTIVE_POWER_TOTAL | 
                                      EVENT_DI_ENERGY_ACTIVE_TOTAL | EVENT_DI_VOLTAGE_A_PHASE | 
                                      EVENT_DI_CURRENT_A_PHASE | EVENT_DI_POWER_FACTOR_TOTAL | 
                                      EVENT_DI_FREQUENCY | EVENT_DI_ENERGY_REVERSE_TOTAL | 
                                      EVENT_DI_DATETIME | EVENT_DI_TIME_HMS;

// 所有事件位的掩码 (包括原有的EVENT_GENERAL)
const EventBits_t ALL_EVENTS = EVENT_GENERAL | ALL_DLT645_EVENTS;
#endif

class DLT645Component : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  
  void set_magic_number(uint32_t magic_number) { this->magic_number_ = magic_number; }
  
  // 设置总功率查询比例控制参数
  void set_power_ratio(int ratio) { this->power_ratio_ = ratio; }
  
  // 原有的通用回调函数（保持向后兼容）
  void add_on_hello_world_callback(std::function<void(uint32_t)> &&callback) {
    this->hello_world_callback_.add(std::move(callback));
  }
  
  // DL/T 645-2007 数据标识符独立事件回调函数
  void add_on_device_address_callback(std::function<void(uint32_t)> &&callback) {
    this->device_address_callback_.add(std::move(callback));
  }
  void add_on_active_power_callback(std::function<void(uint32_t, float)> &&callback) {
    this->active_power_callback_.add(std::move(callback));
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
  
  // FreeRTOS任务相关方法
  bool create_dlt645_task();
  void destroy_dlt645_task();
  static void dlt645_task_func(void* parameter);
  void process_dlt645_events();
  
  // === DL/T 645-2007 UART通信相关方法 ===
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
  bool init_dlt645_uart();                           // UART初始化
  void deinit_dlt645_uart();                         // UART反初始化
  bool send_dlt645_frame(const std::vector<uint8_t>& frame, uint32_t timeout_ms = 1000);  // 发送帧（可指定超时时间）
  void process_uart_data();                          // 处理UART数据
  void check_and_parse_dlt645_frame();              // 检查和解析DL/T 645帧
  
  // 动态波特率切换功能（仅针对设备发现命令超时）
  bool change_uart_baud_rate(int new_baud_rate);     // 切换UART波特率
  void cycle_to_next_baud_rate();                    // 循环切换到下一个波特率
  
  // DL/T 645-2007 帧构建和数据处理辅助函数
  std::vector<uint8_t> build_dlt645_read_frame(const std::vector<uint8_t>& address, uint32_t data_identifier);
  void scramble_dlt645_data(std::vector<uint8_t>& data);     // 数据加扰 (+0x33)
  void unscramble_dlt645_data(std::vector<uint8_t>& data);   // 数据解扰 (-0x33)
  float bcd_to_float(const std::vector<uint8_t>& bcd_data, int decimal_places);  // BCD转浮点
  float bcd_to_float_with_sign(const std::vector<uint8_t>& bcd_data, int decimal_places);  // BCD转浮点（支持符号位）
  
  // DL/T 645-2007 设备地址发现和数据查询函数
  bool discover_meter_address();                             // 电表地址发现
  bool query_active_power_total();                           // 查询总有功功率
  void parse_dlt645_data_by_identifier(uint32_t data_identifier, const std::vector<uint8_t>& data_field);  // 根据DI解析数据
  
  // 事件索引管理函数
  size_t get_next_event_index(size_t current_index, size_t max_events);  // 获取下一个事件索引
#endif
  
  uint32_t magic_number_{42};
  
  // 查询比例控制参数
  int power_ratio_{10};                        // 默认10:1比例，每10次总功率查询执行1次其他查询
  int total_power_query_count_{0};            // 总功率查询计数器
  size_t last_non_power_query_index_{2};      // 保存上次"非总功率"查询的索引（从2开始，即总电能）
  
  // 原有的通用回调管理器（保持向后兼容）
  CallbackManager<void(uint32_t)> hello_world_callback_;
  
  // DL/T 645-2007 数据标识符独立事件回调管理器
  CallbackManager<void(uint32_t)> device_address_callback_;    // 设备地址查询
  CallbackManager<void(uint32_t, float)> active_power_callback_;      // 总功率
  CallbackManager<void(uint32_t, float)> energy_active_callback_;     // 总电能
  CallbackManager<void(uint32_t, float)> voltage_a_callback_;         // A相电压
  CallbackManager<void(uint32_t, float)> current_a_callback_;         // A相电流
  CallbackManager<void(uint32_t, float)> power_factor_callback_;      // 功率因数
  CallbackManager<void(uint32_t, float)> frequency_callback_;         // 频率
  CallbackManager<void(uint32_t, float)> energy_reverse_callback_;    // 反向总电能
  CallbackManager<void(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)> datetime_callback_;          // 日期时间 (DI, year, month, day, weekday)
  CallbackManager<void(uint32_t, uint32_t, uint32_t, uint32_t)> time_hms_callback_;          // 时分秒 (DI, hour, minute, second)
  
  // FreeRTOS任务和事件组
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
  TaskHandle_t dlt645_task_handle_{nullptr};
  EventGroupHandle_t event_group_{nullptr};
  bool task_running_{false};
#endif

  // === DL/T 645-2007 UART通信相关成员变量 ===
  
  // UART配置参数（来自YAML配置）
  static constexpr int DLT645_TX_PIN = 1;        // GPIO1 - 发送引脚
  static constexpr int DLT645_RX_PIN = 2;        // GPIO2 - 接收引脚
  static constexpr int DLT645_BAUD_RATE = 2400;  // 波特率
  static constexpr int DLT645_RX_BUFFER_SIZE = 256;  // 接收缓冲区大小
  
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
  uart_port_t uart_port_{UART_NUM_1};           // 使用UART1
  bool uart_initialized_{false};                // UART初始化标志
#endif
  
  // 电表地址管理（对应YAML中的globals）
  std::vector<uint8_t> meter_address_bytes_;     // 电表实际地址
  std::vector<uint8_t> broadcast_address_bytes_; // 广播地址
  bool device_address_discovered_{false};       // 地址发现标志
  
  // UART响应处理（对应YAML中的响应缓冲逻辑）
  std::vector<uint8_t> response_buffer_;         // 响应缓冲区
  uint32_t last_data_receive_time_{0};           // 最后接收数据时间
  uint32_t current_command_timeout_ms_{1000};    // 当前命令的超时时间(与waiting_for_response_配对使用)
  uint32_t frame_timeout_ms_{1000};              // 一般命令帧超时时间(1秒)
  uint32_t device_discovery_timeout_ms_{2000};   // 设备发现命令专用超时时间(2秒)
  
  // 请求-响应匹配
  uint32_t last_sent_data_identifier_{0};        // 最后发送的数据标识符
  uint32_t switch_baud_rate_when_failed_{false}; // 失败时切换波特率标志
  
  // 波特率自动切换管理（仅针对设备发现命令超时）
  std::vector<int> baud_rate_list_{9600, 4800, 2400, 1200};  // 波特率列表，从高到低
  size_t current_baud_rate_index_{0};            // 当前波特率索引

  
  // 性能测量变量
  uint32_t command_send_start_time_{0};
  uint32_t first_response_byte_time_{0};
  
  // === DL/T 645解析数据缓存变量（用于线程安全的数据传递）===
  // 这些变量在FreeRTOS任务中更新，在ESPHome主循环中读取
  float cached_active_power_w_{0.0f};            // 总有功功率 (W)
  float cached_energy_active_kwh_{0.0f};         // 正向有功总电能 (kWh)
  float cached_voltage_a_v_{0.0f};               // A相电压 (V)
  float cached_current_a_a_{0.0f};               // A相电流 (A)
  float cached_power_factor_{0.0f};              // 总功率因数
  float cached_frequency_hz_{0.0f};              // 电网频率 (Hz)
  float cached_energy_reverse_kwh_{0.0f};        // 反向有功总电能 (kWh)
  std::string cached_datetime_str_{""};          // 日期时间字符串
  std::string cached_time_hms_str_{""};          // 时分秒字符串
  
  // 日期时间数值缓存
  uint32_t cached_year_{0};                      // 年
  uint32_t cached_month_{0};                     // 月
  uint32_t cached_day_{0};                       // 日
  uint32_t cached_weekday_{0};                   // 星期 (1-7)
  
  // 时分秒数值缓存
  uint32_t cached_hour_{0};                      // 时
  uint32_t cached_minute_{0};                    // 分
  uint32_t cached_second_{0};                    // 秒
};

// 原有的通用触发器（保持向后兼容）
class HelloWorldTrigger : public Trigger<uint32_t> {
 public:
  explicit HelloWorldTrigger(DLT645Component *parent) {
    parent->add_on_hello_world_callback([this](uint32_t magic_number) {
      this->trigger(magic_number);
    });
  }
};

// DL/T 645-2007 数据标识符独立事件触发器类
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
      this->trigger(data_identifier, power_watts);  // power_watts 以W为单位传递
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