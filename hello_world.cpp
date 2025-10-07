#include "hello_world.h"
#include "esphome/core/log.h"
#include "esphome/core/component.h"

#if defined(USE_ESP32) || defined(USE_ESP_IDF)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#endif

namespace esphome {
namespace hello_world_component {

static const char *const TAG = "hello_world_component";

// 静态变量：模拟的功率值（用于测试）
static float fake_active_power_value = 1234.5f;  // 默认fake功率值：1234.5W

// 获取毫秒数的跨平台函数
uint32_t get_current_time_ms() {
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
  return (uint32_t)(esp_timer_get_time() / 1000ULL);
#else
  return millis();
#endif
}

void HelloWorldComponent::setup() {
  ESP_LOGCONFIG(TAG, "🚀 设置带FreeRTOS任务的Hello World组件...");
  ESP_LOGCONFIG(TAG, "Magic Number: %lu", (unsigned long)this->magic_number_);

#if defined(USE_ESP32) || defined(USE_ESP_IDF)
  // 创建事件组用于任务间通信
  this->event_group_ = xEventGroupCreate();
  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "❌ 创建事件组失败");
    this->mark_failed();
    return;
  }
  
  // 创建FreeRTOS任务
  if (!this->create_hello_world_task()) {
    ESP_LOGE(TAG, "❌ 创建FreeRTOS任务失败");
    this->mark_failed();
    return;
  }
  
  ESP_LOGCONFIG(TAG, "✅ FreeRTOS任务已创建，将每 %lu 秒触发一次事件", 
                (unsigned long)(HELLO_WORLD_TRIGGER_INTERVAL_MS / 1000));
#else
  ESP_LOGW(TAG, "⚠️ 非ESP32平台，降级为loop模式");
#endif
  
  ESP_LOGCONFIG(TAG, "✅ Hello World组件设置完成");
}

void HelloWorldComponent::loop() {
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
  // 在ESP32上，主要逻辑在FreeRTOS任务中，loop只处理事件组中的事件
  this->process_hello_world_events();
#else
  // 非ESP32平台的备用实现
  static uint32_t last_trigger_time = 0;
  uint32_t now = get_current_time_ms();
  if (now - last_trigger_time >= HELLO_WORLD_TRIGGER_INTERVAL_MS) {
    this->trigger_hello_world_event();
    last_trigger_time = now;
  }
#endif
}

void HelloWorldComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Hello World Component (FreeRTOS Task版本):");
  ESP_LOGCONFIG(TAG, "  Magic Number: %lu", (unsigned long)this->magic_number_);
  ESP_LOGCONFIG(TAG, "  Trigger Interval: %lu 秒", 
                (unsigned long)(HELLO_WORLD_TRIGGER_INTERVAL_MS / 1000));
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
  ESP_LOGCONFIG(TAG, "  Task Status: %s", this->task_running_ ? "运行中" : "已停止");
  ESP_LOGCONFIG(TAG, "  Task Stack Size: %lu 字节", (unsigned long)HELLO_WORLD_TASK_STACK_SIZE);
  ESP_LOGCONFIG(TAG, "  Task Priority: %d", (int)HELLO_WORLD_TASK_PRIORITY);
  ESP_LOGCONFIG(TAG, "  Event Group: %s", this->event_group_ ? "已创建" : "未创建");
#endif
}

void HelloWorldComponent::trigger_hello_world_event() {
  ESP_LOGD(TAG, "🌍 Hello World 事件触发! Magic Number: %lu", 
           (unsigned long)this->magic_number_);
  this->hello_world_callback_.call(this->magic_number_);
}

#if defined(USE_ESP32) || defined(USE_ESP_IDF)

bool HelloWorldComponent::create_hello_world_task() {
  if (this->hello_world_task_handle_ != nullptr) {
    ESP_LOGW(TAG, "⚠️ FreeRTOS任务已存在");
    return true;
  }
  
  this->task_running_ = true;
  
  // 创建FreeRTOS任务 - 参考ESP32Camera的实现方式
  BaseType_t result = xTaskCreate(
    &HelloWorldComponent::hello_world_task_func,  // 任务函数
    "hello_world_task",                           // 任务名称
    HELLO_WORLD_TASK_STACK_SIZE,                  // 堆栈大小
    this,                                         // 传递给任务的参数(this指针)
    HELLO_WORLD_TASK_PRIORITY,                    // 任务优先级
    &this->hello_world_task_handle_               // 任务句柄
  );
  
  if (result != pdPASS) {
    ESP_LOGE(TAG, "❌ xTaskCreate失败，错误代码: %d", result);
    this->task_running_ = false;
    return false;
  }
  
  ESP_LOGI(TAG, "✅ FreeRTOS任务创建成功，句柄: %p", this->hello_world_task_handle_);
  return true;
}

void HelloWorldComponent::destroy_hello_world_task() {
  if (this->hello_world_task_handle_ == nullptr) {
    return;
  }
  
  ESP_LOGI(TAG, "🧹 销毁FreeRTOS任务...");
  this->task_running_ = false;
  
  // 等待任务自然结束
  vTaskDelay(pdMS_TO_TICKS(100));
  
  // 删除任务
  if (this->hello_world_task_handle_ != nullptr) {
    vTaskDelete(this->hello_world_task_handle_);
    this->hello_world_task_handle_ = nullptr;
  }
  
  // 删除事件组
  if (this->event_group_ != nullptr) {
    vEventGroupDelete(this->event_group_);
    this->event_group_ = nullptr;
  }
  
  ESP_LOGI(TAG, "✅ FreeRTOS任务已销毁");
}

// 静态任务函数 - 在独立的FreeRTOS任务中运行
void HelloWorldComponent::hello_world_task_func(void* parameter) {
  HelloWorldComponent* component = static_cast<HelloWorldComponent*>(parameter);
  
  ESP_LOGI(TAG, "🚀 FreeRTOS任务启动，任务句柄: %p", xTaskGetCurrentTaskHandle());
  ESP_LOGI(TAG, "📊 任务堆栈高水位: %lu 字节", 
           (unsigned long)uxTaskGetStackHighWaterMark(nullptr));
  
  uint32_t last_trigger_time = get_current_time_ms();
  
  // DL/T 645-2007数据标识符数组和对应的事件位（按用户要求重新排序）
  const EventBits_t dlt645_event_bits[] = {
    EVENT_DI_DEVICE_ADDRESS,        // BIT1: 设备地址查询 (0x04000401)
    EVENT_DI_ACTIVE_POWER_TOTAL,    // BIT2: 总功率 (0x02030000)
    EVENT_DI_ENERGY_ACTIVE_TOTAL,   // BIT3: 总电能 (0x00010000)
    EVENT_DI_VOLTAGE_A_PHASE,       // BIT4: A相电压 (0x02010100)
    EVENT_DI_CURRENT_A_PHASE,       // BIT5: A相电流 (0x02020100)
    EVENT_DI_POWER_FACTOR_TOTAL,    // BIT6: 功率因数 (0x02060000)
    EVENT_DI_FREQUENCY,             // BIT7: 频率 (0x02800002)
    EVENT_DI_ENERGY_REVERSE_TOTAL,  // BIT8: 反向总电能 (0x00020000)
    EVENT_DI_DATETIME,              // BIT9: 日期时间 (0x04000101)
    EVENT_DI_TIME_HMS               // BIT10: 时分秒 (0x04000102)
  };
  
  const char* dlt645_event_names[] = {
    "设备地址查询",
    "总功率",
    "总电能", 
    "A相电压",
    "A相电流",
    "功率因数",
    "频率",
    "反向总电能",
    "日期时间",
    "时分秒"
  };
  
  const uint32_t dlt645_data_identifiers[] = {
    0x04000401,  // 设备地址查询
    0x02030000,  // 总功率
    0x00010000,  // 总电能
    0x02010100,  // A相电压
    0x02020100,  // A相电流
    0x02060000,  // 功率因数
    0x02800002,  // 频率
    0x00020000,  // 反向总电能
    0x04000101,  // 日期时间
    0x04000102   // 时分秒
  };
  
  const size_t num_dlt645_events = sizeof(dlt645_event_bits) / sizeof(dlt645_event_bits[0]);
  size_t current_event_index = 0;
  
  ESP_LOGI(TAG, "📋 DL/T 645事件循环已配置，共 %d 个数据标识符", num_dlt645_events);
  
  // 任务主循环 - 按顺序循环触发DL/T 645事件位
  while (component->task_running_) {
    uint32_t now = get_current_time_ms();
    
    // 检查是否到了触发时间
    if (now - last_trigger_time >= HELLO_WORLD_TRIGGER_INTERVAL_MS) {
      // 获取当前要触发的事件位和相关信息
      EventBits_t current_event_bit = dlt645_event_bits[current_event_index];
      const char* event_name = dlt645_event_names[current_event_index];
      uint32_t data_identifier = dlt645_data_identifiers[current_event_index];
      
      ESP_LOGI(TAG, "🎯 [%d/%d] 触发DL/T 645事件: %s (DI: 0x%08X, BIT: %d)", 
               current_event_index + 1, num_dlt645_events, event_name, 
               data_identifier, __builtin_ctzl(current_event_bit));
      
      // 设置事件组中的对应事件位
      EventBits_t result = xEventGroupSetBits(component->event_group_, current_event_bit);
      if (result & current_event_bit) {
        ESP_LOGD(TAG, "📤 事件位已设置: %s, time=%lu", event_name, (unsigned long)now);
      } else {
        ESP_LOGW(TAG, "⚠️ 设置事件位失败: %s", event_name);
      }
      
      // 移动到下一个事件位（循环）
      current_event_index = (current_event_index + 1) % num_dlt645_events;
      
      // 如果循环了一轮，在日志中标记
      if (current_event_index == 0) {
        ESP_LOGI(TAG, "🔄 DL/T 645事件位循环完成，重新开始...");
      }
      
      last_trigger_time = now;
    }
    
    // 任务延迟 - 释放CPU给其他任务
    vTaskDelay(pdMS_TO_TICKS(100));  // 100ms延迟
  }
  
  ESP_LOGI(TAG, "🛑 FreeRTOS任务即将退出");
  
  // 任务清理并自我删除
  component->hello_world_task_handle_ = nullptr;
  vTaskDelete(nullptr);
}

void HelloWorldComponent::process_hello_world_events() {
  if (this->event_group_ == nullptr) {
    return;
  }
  
  // 非阻塞地检查事件组中的事件位
  EventBits_t event_bits = xEventGroupWaitBits(
    this->event_group_,     // 事件组句柄
    ALL_EVENTS,             // 等待的事件位掩码 (包括原有的和新的DL/T 645事件位)
    pdTRUE,                 // 清除事件位
    pdFALSE,                // 不需要所有位都设置
    0                       // 非阻塞（超时时间为0）
  );
  
  // 处理原有的一般事件（保持向后兼容性）
  if (event_bits & EVENT_GENERAL) {
    ESP_LOGD(TAG, "📥 检测到EVENT_GENERAL事件位");
    // 触发ESPHome事件回调
    this->trigger_hello_world_event();
  }
  
  // 处理DL/T 645-2007数据标识符事件位
  struct DLT645EventInfo {
    EventBits_t bit;
    uint32_t data_identifier;
    const char* name;
  };
  
  const DLT645EventInfo dlt645_events[] = {
    {EVENT_DI_DEVICE_ADDRESS, 0x04000401, "设备地址查询"},
    {EVENT_DI_ACTIVE_POWER_TOTAL, 0x02030000, "总功率"},
    {EVENT_DI_ENERGY_ACTIVE_TOTAL, 0x00010000, "总电能"},
    {EVENT_DI_VOLTAGE_A_PHASE, 0x02010100, "A相电压"},
    {EVENT_DI_CURRENT_A_PHASE, 0x02020100, "A相电流"},
    {EVENT_DI_POWER_FACTOR_TOTAL, 0x02060000, "功率因数"},
    {EVENT_DI_FREQUENCY, 0x02800002, "频率"},
    {EVENT_DI_ENERGY_REVERSE_TOTAL, 0x00020000, "反向总电能"},
    {EVENT_DI_DATETIME, 0x04000101, "日期时间"},
    {EVENT_DI_TIME_HMS, 0x04000102, "时分秒"}
  };
  
  const size_t num_dlt645_events = sizeof(dlt645_events) / sizeof(dlt645_events[0]);
  
  // 遍历检查所有DL/T 645事件位，为每个事件位调用对应的独立回调
  for (size_t i = 0; i < num_dlt645_events; i++) {
    if (event_bits & dlt645_events[i].bit) {
      int bit_num = __builtin_ctzl(dlt645_events[i].bit);  // 计算位号
      
      ESP_LOGI(TAG, "📥 检测到DL/T 645事件: %s (DI: 0x%08X, BIT%d)", 
               dlt645_events[i].name, dlt645_events[i].data_identifier, bit_num);
      
      // 根据事件位调用对应的独立回调函数
      switch (dlt645_events[i].bit) {
        case EVENT_DI_DEVICE_ADDRESS:
          this->device_address_callback_.call(dlt645_events[i].data_identifier);
          break;
        case EVENT_DI_ACTIVE_POWER_TOTAL:
          // 每次触发时更新fake功率值（模拟变化的功率读数）
          fake_active_power_value += 10.5f;  // 每次增加10.5W
          if (fake_active_power_value > 3000.0f) {
            fake_active_power_value = 800.0f;  // 重置到800W
          }
          ESP_LOGI(TAG, "📊 传递fake功率值: %.1f W", fake_active_power_value);
          this->active_power_callback_.call(dlt645_events[i].data_identifier, fake_active_power_value);
          break;
        case EVENT_DI_ENERGY_ACTIVE_TOTAL:
          this->energy_active_callback_.call(dlt645_events[i].data_identifier);
          break;
        case EVENT_DI_VOLTAGE_A_PHASE:
          this->voltage_a_callback_.call(dlt645_events[i].data_identifier);
          break;
        case EVENT_DI_CURRENT_A_PHASE:
          this->current_a_callback_.call(dlt645_events[i].data_identifier);
          break;
        case EVENT_DI_POWER_FACTOR_TOTAL:
          this->power_factor_callback_.call(dlt645_events[i].data_identifier);
          break;
        case EVENT_DI_FREQUENCY:
          this->frequency_callback_.call(dlt645_events[i].data_identifier);
          break;
        case EVENT_DI_ENERGY_REVERSE_TOTAL:
          this->energy_reverse_callback_.call(dlt645_events[i].data_identifier);
          break;
        case EVENT_DI_DATETIME:
          this->datetime_callback_.call(dlt645_events[i].data_identifier);
          break;
        case EVENT_DI_TIME_HMS:
          this->time_hms_callback_.call(dlt645_events[i].data_identifier);
          break;
        default:
          ESP_LOGW(TAG, "⚠️ 未知事件位: 0x%08X", dlt645_events[i].bit);
          break;
      }
    }
  }
}

#endif  // defined(USE_ESP32) || defined(USE_ESP_IDF)

}  // namespace hello_world_component
}  // namespace esphome