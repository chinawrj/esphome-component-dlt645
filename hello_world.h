#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"

#if defined(USE_ESP32) || defined(USE_ESP_IDF)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#endif

namespace esphome {
namespace hello_world_component {

// FreeRTOS任务配置常量
constexpr uint32_t HELLO_WORLD_TASK_STACK_SIZE = 4096;  // 4KB堆栈
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
constexpr UBaseType_t HELLO_WORLD_TASK_PRIORITY = 5;    // 中等优先级
#endif
constexpr uint32_t HELLO_WORLD_TRIGGER_INTERVAL_MS = 5000;  // 5秒间隔

// Event Group事件位定义 - DL/T 645-2007数据标识符对应的事件位
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
const EventBits_t EVENT_GENERAL = BIT0;            // BIT0: 保持原有的一般事件
const EventBits_t EVENT_DI_DEVICE_ADDRESS = BIT1;  // BIT1: 设备地址查询 (0x04000401)
const EventBits_t EVENT_DI_ACTIVE_POWER_TOTAL = BIT2;  // BIT2: 总功率 (0x02030000)
const EventBits_t EVENT_DI_ENERGY_ACTIVE_TOTAL = BIT3; // BIT3: 总电能 (0x00010000)
const EventBits_t EVENT_DI_VOLTAGE_A_PHASE = BIT4;     // BIT4: A相电压 (0x02010100)
const EventBits_t EVENT_DI_CURRENT_A_PHASE = BIT5;     // BIT5: A相电流 (0x02020100)
const EventBits_t EVENT_DI_POWER_FACTOR_TOTAL = BIT6;  // BIT6: 功率因数 (0x02060000)
const EventBits_t EVENT_DI_FREQUENCY = BIT7;           // BIT7: 频率 (0x02800002)
const EventBits_t EVENT_DI_ENERGY_REVERSE_TOTAL = BIT8; // BIT8: 反向总电能 (0x00020000)
const EventBits_t EVENT_DI_DATETIME = BIT9;            // BIT9: 日期时间 (0x04000101)
const EventBits_t EVENT_DI_TIME_HMS = BIT10;           // BIT10: 时分秒 (0x04000102)

// 所有DL/T 645事件位的掩码 (BIT1-BIT10)
const EventBits_t ALL_DLT645_EVENTS = EVENT_DI_DEVICE_ADDRESS | EVENT_DI_ACTIVE_POWER_TOTAL | 
                                      EVENT_DI_ENERGY_ACTIVE_TOTAL | EVENT_DI_VOLTAGE_A_PHASE | 
                                      EVENT_DI_CURRENT_A_PHASE | EVENT_DI_POWER_FACTOR_TOTAL | 
                                      EVENT_DI_FREQUENCY | EVENT_DI_ENERGY_REVERSE_TOTAL | 
                                      EVENT_DI_DATETIME | EVENT_DI_TIME_HMS;

// 所有事件位的掩码 (包括原有的EVENT_GENERAL)
const EventBits_t ALL_EVENTS = EVENT_GENERAL | ALL_DLT645_EVENTS;
#endif

class HelloWorldComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  
  void set_magic_number(uint32_t magic_number) { this->magic_number_ = magic_number; }
  
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
  void add_on_energy_active_callback(std::function<void(uint32_t)> &&callback) {
    this->energy_active_callback_.add(std::move(callback));
  }
  void add_on_voltage_a_callback(std::function<void(uint32_t)> &&callback) {
    this->voltage_a_callback_.add(std::move(callback));
  }
  void add_on_current_a_callback(std::function<void(uint32_t)> &&callback) {
    this->current_a_callback_.add(std::move(callback));
  }
  void add_on_power_factor_callback(std::function<void(uint32_t)> &&callback) {
    this->power_factor_callback_.add(std::move(callback));
  }
  void add_on_frequency_callback(std::function<void(uint32_t)> &&callback) {
    this->frequency_callback_.add(std::move(callback));
  }
  void add_on_energy_reverse_callback(std::function<void(uint32_t)> &&callback) {
    this->energy_reverse_callback_.add(std::move(callback));
  }
  void add_on_datetime_callback(std::function<void(uint32_t)> &&callback) {
    this->datetime_callback_.add(std::move(callback));
  }
  void add_on_time_hms_callback(std::function<void(uint32_t)> &&callback) {
    this->time_hms_callback_.add(std::move(callback));
  }

 protected:
  void trigger_hello_world_event();
  
  // FreeRTOS任务相关方法
  bool create_hello_world_task();
  void destroy_hello_world_task();
  static void hello_world_task_func(void* parameter);
  void process_hello_world_events();
  
  uint32_t magic_number_{42};
  
  // 原有的通用回调管理器（保持向后兼容）
  CallbackManager<void(uint32_t)> hello_world_callback_;
  
  // DL/T 645-2007 数据标识符独立事件回调管理器
  CallbackManager<void(uint32_t)> device_address_callback_;    // 设备地址查询
  CallbackManager<void(uint32_t, float)> active_power_callback_;      // 总功率
  CallbackManager<void(uint32_t)> energy_active_callback_;     // 总电能
  CallbackManager<void(uint32_t)> voltage_a_callback_;         // A相电压
  CallbackManager<void(uint32_t)> current_a_callback_;         // A相电流
  CallbackManager<void(uint32_t)> power_factor_callback_;      // 功率因数
  CallbackManager<void(uint32_t)> frequency_callback_;         // 频率
  CallbackManager<void(uint32_t)> energy_reverse_callback_;    // 反向总电能
  CallbackManager<void(uint32_t)> datetime_callback_;          // 日期时间
  CallbackManager<void(uint32_t)> time_hms_callback_;          // 时分秒
  
  // FreeRTOS任务和事件组
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
  TaskHandle_t hello_world_task_handle_{nullptr};
  EventGroupHandle_t event_group_{nullptr};
  bool task_running_{false};
#endif
};

// 原有的通用触发器（保持向后兼容）
class HelloWorldTrigger : public Trigger<uint32_t> {
 public:
  explicit HelloWorldTrigger(HelloWorldComponent *parent) {
    parent->add_on_hello_world_callback([this](uint32_t magic_number) {
      this->trigger(magic_number);
    });
  }
};

// DL/T 645-2007 数据标识符独立事件触发器类
class DeviceAddressTrigger : public Trigger<uint32_t> {
 public:
  explicit DeviceAddressTrigger(HelloWorldComponent *parent) {
    parent->add_on_device_address_callback([this](uint32_t data_identifier) {
      this->trigger(data_identifier);
    });
  }
};

class ActivePowerTrigger : public Trigger<uint32_t, float> {
 public:
  explicit ActivePowerTrigger(HelloWorldComponent *parent) {
    parent->add_on_active_power_callback([this](uint32_t data_identifier, float fake_power) {
      this->trigger(data_identifier, fake_power);
    });
  }
};

class EnergyActiveTrigger : public Trigger<uint32_t> {
 public:
  explicit EnergyActiveTrigger(HelloWorldComponent *parent) {
    parent->add_on_energy_active_callback([this](uint32_t data_identifier) {
      this->trigger(data_identifier);
    });
  }
};

class VoltageATrigger : public Trigger<uint32_t> {
 public:
  explicit VoltageATrigger(HelloWorldComponent *parent) {
    parent->add_on_voltage_a_callback([this](uint32_t data_identifier) {
      this->trigger(data_identifier);
    });
  }
};

class CurrentATrigger : public Trigger<uint32_t> {
 public:
  explicit CurrentATrigger(HelloWorldComponent *parent) {
    parent->add_on_current_a_callback([this](uint32_t data_identifier) {
      this->trigger(data_identifier);
    });
  }
};

class PowerFactorTrigger : public Trigger<uint32_t> {
 public:
  explicit PowerFactorTrigger(HelloWorldComponent *parent) {
    parent->add_on_power_factor_callback([this](uint32_t data_identifier) {
      this->trigger(data_identifier);
    });
  }
};

class FrequencyTrigger : public Trigger<uint32_t> {
 public:
  explicit FrequencyTrigger(HelloWorldComponent *parent) {
    parent->add_on_frequency_callback([this](uint32_t data_identifier) {
      this->trigger(data_identifier);
    });
  }
};

class EnergyReverseTrigger : public Trigger<uint32_t> {
 public:
  explicit EnergyReverseTrigger(HelloWorldComponent *parent) {
    parent->add_on_energy_reverse_callback([this](uint32_t data_identifier) {
      this->trigger(data_identifier);
    });
  }
};

class DatetimeTrigger : public Trigger<uint32_t> {
 public:
  explicit DatetimeTrigger(HelloWorldComponent *parent) {
    parent->add_on_datetime_callback([this](uint32_t data_identifier) {
      this->trigger(data_identifier);
    });
  }
};

class TimeHmsTrigger : public Trigger<uint32_t> {
 public:
  explicit TimeHmsTrigger(HelloWorldComponent *parent) {
    parent->add_on_time_hms_callback([this](uint32_t data_identifier) {
      this->trigger(data_identifier);
    });
  }
};

}  // namespace hello_world_component
}  // namespace esphome