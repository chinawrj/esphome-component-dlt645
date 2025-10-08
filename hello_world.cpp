#include "hello_world.h"
#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#if defined(USE_ESP32) || defined(USE_ESP_IDF)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include "esp_log.h"
#endif

namespace esphome {
namespace hello_world_component {

static const char *const TAG = "hello_world_component";

// 获取毫秒数的跨平台函数
uint32_t get_current_time_ms() {
#if defined(USE_ESP32) || defined(USE_ESP_IDF)
  return (uint32_t)(esp_timer_get_time() / 1000ULL);
#else
  // 非ESP32平台，返回一个简单的时间戳
  return 0;
#endif
}

void HelloWorldComponent::setup() {
  ESP_LOGCONFIG(TAG, "🚀 设置带FreeRTOS任务的Hello World组件...");
  ESP_LOGCONFIG(TAG, "Magic Number: %lu", (unsigned long)this->magic_number_);

#if defined(USE_ESP32) || defined(USE_ESP_IDF)
  // === 初始化DL/T 645-2007 UART通信相关变量 ===
  ESP_LOGI(TAG, "📡 初始化DL/T 645-2007 UART通信变量...");
  
  // 初始化地址管理变量（对应YAML中的globals）
  this->meter_address_bytes_ = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};     // 默认广播地址
  this->broadcast_address_bytes_ = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA}; // 广播地址
  this->device_address_discovered_ = false;
  
  // 初始化响应处理变量
  this->response_buffer_.clear();
  this->waiting_for_response_ = false;
  this->frame_timeout_ms_ = 500;  // 500ms超时
  this->last_data_receive_time_ = 0;
  this->last_sent_data_identifier_ = 0;
  
  // 性能测量变量初始化
  this->command_send_start_time_ = 0;
  this->first_response_byte_time_ = 0;
  
  ESP_LOGI(TAG, "✅ DL/T 645变量初始化完成");
  
  // === 初始化UART通信 ===
  if (!this->init_dlt645_uart()) {
    ESP_LOGE(TAG, "❌ DL/T 645 UART初始化失败");
    this->mark_failed();
    return;
  }
  
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
  
  // === 清理DL/T 645 UART资源 ===
  this->deinit_dlt645_uart();
  
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
  
  // === DL/T 645-2007 UART通信主循环 ===
  uint32_t last_dlt645_send_time = 0;
  const uint32_t DLT645_SEND_INTERVAL_MS = 50;  // 1秒发送间隔（对应YAML中的1s interval）
  uint32_t last_uart_read_time = 0;
  const uint32_t UART_READ_INTERVAL_MS = 5;
  
  // 任务主循环 - DL/T 645通信 + 事件触发
  while (component->task_running_) {
    uint32_t now = get_current_time_ms();
    
    // === 1. DL/T 645 UART数据读取处理（10ms间隔）===
    if (now - last_uart_read_time >= UART_READ_INTERVAL_MS) {
      component->process_uart_data();
      last_uart_read_time = now;
    }

    // === 2. DL/T 645 数据查询发送（1s间隔）===
    if (now - last_dlt645_send_time >= DLT645_SEND_INTERVAL_MS && false == component->waiting_for_response_) {
        // 获取当前要查询的数据标识符
        uint32_t data_identifier = dlt645_data_identifiers[current_event_index];
        const char* event_name = dlt645_event_names[current_event_index];
        
        ESP_LOGD(TAG, "📡 [%d/%d] 发送DL/T 645查询: %s (DI: 0x%08X)", 
                 current_event_index + 1, num_dlt645_events, event_name, data_identifier);
        
        // 根据当前数据标识符选择相应的查询函数
        bool send_success = false;
        if (data_identifier == 0x04000401) {
          // 设备地址查询
          send_success = component->discover_meter_address();
        } else if (data_identifier == 0x02030000) {
          // 总有功功率查询
          send_success = component->query_active_power_total();
        } else {
          // 通用查询 - 使用当前已知地址或广播地址
          std::vector<uint8_t> query_address = component->meter_address_bytes_;
          if (query_address.empty()) {
            query_address = {0x99, 0x99, 0x99, 0x99, 0x99, 0x99};  // 广播地址
          }
          
          std::vector<uint8_t> query_frame = component->build_dlt645_read_frame(query_address, data_identifier);
          send_success = component->send_dlt645_frame(query_frame);
        }
        
        if (!send_success) {
          ESP_LOGW(TAG, "⚠️ DL/T 645查询发送失败: %s", event_name);
        }
        
        // 移动到下一个数据标识符（循环）
        current_event_index = (current_event_index + 1) % num_dlt645_events;
        
        // 如果电表地址已被发现，跳过设备地址查询（index 0）
        if (current_event_index == 0 && component->device_address_discovered_) {
          ESP_LOGD(TAG, "⏭️ 电表地址已发现，跳过设备地址查询");
          current_event_index = 1;  // 跳到下一个有用的查询（总功率）
        }
        
        if (current_event_index == 0) {
          ESP_LOGD(TAG, "🔄 DL/T 645查询循环完成，重新开始...");
        } else if (current_event_index == 1 && component->device_address_discovered_) {
          ESP_LOGD(TAG, "🔄 DL/T 645查询循环完成（跳过地址查询），重新开始数据查询...");
        }
      
      last_dlt645_send_time = now;
    }
    
    // 任务延迟 - 释放CPU给其他任务
    vTaskDelay(pdMS_TO_TICKS(10));  // 10ms延迟，提高响应性
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
      
      ESP_LOGD(TAG, "📥 检测到DL/T 645事件: %s (DI: 0x%08X, BIT%d)", 
               dlt645_events[i].name, dlt645_events[i].data_identifier, bit_num);
      
      // 根据事件位调用对应的独立回调函数
      switch (dlt645_events[i].bit) {
        case EVENT_DI_DEVICE_ADDRESS:
          this->device_address_callback_.call(this->cached_data_identifier_);
          break;
        case EVENT_DI_ACTIVE_POWER_TOTAL:
          ESP_LOGD(TAG, "📊 传递功率值: %.1f W", this->cached_active_power_w_);
          this->active_power_callback_.call(this->cached_data_identifier_, this->cached_active_power_w_);
          break;
        case EVENT_DI_ENERGY_ACTIVE_TOTAL:
          ESP_LOGD(TAG, "🔋 传递总电能值: %.2f kWh", this->cached_energy_active_kwh_);
          this->energy_active_callback_.call(this->cached_data_identifier_);
          break;
        case EVENT_DI_VOLTAGE_A_PHASE:
          ESP_LOGD(TAG, "🔌 传递A相电压值: %.1f V", this->cached_voltage_a_v_);
          this->voltage_a_callback_.call(this->cached_data_identifier_);
          break;
        case EVENT_DI_CURRENT_A_PHASE:
          ESP_LOGD(TAG, "🔄 传递A相电流值: %.3f A", this->cached_current_a_a_);
          this->current_a_callback_.call(this->cached_data_identifier_);
          break;
        case EVENT_DI_POWER_FACTOR_TOTAL:
          ESP_LOGD(TAG, "📈 传递功率因数值: %.3f", this->cached_power_factor_);
          this->power_factor_callback_.call(this->cached_data_identifier_);
          break;
        case EVENT_DI_FREQUENCY:
          ESP_LOGD(TAG, "🌊 传递频率值: %.2f Hz", this->cached_frequency_hz_);
          this->frequency_callback_.call(this->cached_data_identifier_);
          break;
        case EVENT_DI_ENERGY_REVERSE_TOTAL:
          ESP_LOGD(TAG, "🔄 传递反向电能值: %.2f kWh", this->cached_energy_reverse_kwh_);
          this->energy_reverse_callback_.call(this->cached_data_identifier_);
          break;
        case EVENT_DI_DATETIME:
          ESP_LOGD(TAG, "📅 传递日期时间: %s", this->cached_datetime_str_.c_str());
          this->datetime_callback_.call(this->cached_data_identifier_);
          break;
        case EVENT_DI_TIME_HMS:
          ESP_LOGD(TAG, "⏰ 传递时分秒: %s", this->cached_time_hms_str_.c_str());
          this->time_hms_callback_.call(this->cached_data_identifier_);
          break;
        default:
          ESP_LOGW(TAG, "⚠️ 未知事件位: 0x%08X", dlt645_events[i].bit);
          break;
      }
    }
  }
}

#endif  // defined(USE_ESP32) || defined(USE_ESP_IDF)

#if defined(USE_ESP32) || defined(USE_ESP_IDF)

// === DL/T 645-2007 UART通信实现 ===

bool HelloWorldComponent::init_dlt645_uart() {
  ESP_LOGI(TAG, "🔧 初始化DL/T 645-2007 UART通信...");
  
  // UART配置结构体
  uart_config_t uart_config = {
      .baud_rate = DLT645_BAUD_RATE,              // 2400 波特率
      .data_bits = UART_DATA_8_BITS,              // 8数据位  
      .parity = UART_PARITY_EVEN,                 // 偶校验
      .stop_bits = UART_STOP_BITS_1,              // 1停止位
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,      // 无硬件流控
      .rx_flow_ctrl_thresh = 122,                 // RX流控阈值
      .source_clk = UART_SCLK_DEFAULT,            // 默认时钟源
  };
  
  ESP_LOGI(TAG, "📋 UART配置: 波特率=%d, 数据位=8, 校验=偶校验, 停止位=1", DLT645_BAUD_RATE);
  
  // 配置UART参数
  esp_err_t ret = uart_param_config(this->uart_port_, &uart_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ UART参数配置失败: %s", esp_err_to_name(ret));
    return false;
  }
  
  // 设置UART引脚
  ret = uart_set_pin(this->uart_port_, DLT645_TX_PIN, DLT645_RX_PIN, 
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ UART引脚设置失败: %s", esp_err_to_name(ret));
    return false;
  }
  
  ESP_LOGI(TAG, "📌 UART引脚: TX=GPIO%d, RX=GPIO%d", DLT645_TX_PIN, DLT645_RX_PIN);
  
  // 安装UART驱动程序
  ret = uart_driver_install(this->uart_port_, DLT645_RX_BUFFER_SIZE, 0, 0, nullptr, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ UART驱动安装失败: %s", esp_err_to_name(ret));
    return false;
  }
  
  this->uart_initialized_ = true;
  
  ESP_LOGI(TAG, "✅ DL/T 645 UART初始化成功");
  ESP_LOGI(TAG, "   - UART端口: %d", this->uart_port_);
  ESP_LOGI(TAG, "   - 接收缓冲区: %d 字节", DLT645_RX_BUFFER_SIZE);
  
  return true;
}

void HelloWorldComponent::deinit_dlt645_uart() {
  if (this->uart_initialized_) {
    ESP_LOGI(TAG, "🧹 反初始化DL/T 645 UART...");
    uart_driver_delete(this->uart_port_);
    this->uart_initialized_ = false;
    ESP_LOGI(TAG, "✅ UART已反初始化");
  }
}

bool HelloWorldComponent::send_dlt645_frame(const std::vector<uint8_t>& frame_data) {
  if (!this->uart_initialized_) {
    ESP_LOGE(TAG, "❌ UART未初始化，无法发送数据帧");
    return false;
  }
  
  // 输出调试信息 - 显示发送的帧数据
  std::string hex_frame = "";
  for (size_t i = 0; i < frame_data.size(); i++) {
    char hex_str[4];
    sprintf(hex_str, "%02X ", frame_data[i]);
    hex_frame += hex_str;
  }
  ESP_LOGD(TAG, "📤 发送DL/T 645帧 (%d字节): %s", frame_data.size(), hex_frame.c_str());
  
  // 清空接收缓冲区
  uart_flush_input(this->uart_port_);
  this->response_buffer_.clear();
  
  // 发送帧数据
  int bytes_written = uart_write_bytes(this->uart_port_, 
                                      frame_data.data(), 
                                      frame_data.size());
  
  if (bytes_written != frame_data.size()) {
    ESP_LOGE(TAG, "❌ UART发送失败，期望 %d 字节，实际发送 %d 字节", 
             frame_data.size(), bytes_written);
    return false;
  }
  
  // 等待发送完成
  uart_wait_tx_done(this->uart_port_, pdMS_TO_TICKS(500));
  
  // 标记等待响应状态
  this->waiting_for_response_ = true;
  this->last_data_receive_time_ = get_current_time_ms();
  
  ESP_LOGD(TAG, "✅ 成功发送 %d 字节 DL/T 645帧", frame_data.size());
  return true;
}

void HelloWorldComponent::process_uart_data() {
  if (!this->uart_initialized_) {
    return;
  }
  
  uint32_t current_time = get_current_time_ms();
  
  // 检查响应超时
  if (this->waiting_for_response_ && 
      current_time - this->last_data_receive_time_ > this->frame_timeout_ms_) {
    ESP_LOGW(TAG, "⏰ DL/T 645响应超时，清空缓冲区");
    this->response_buffer_.clear();
    this->waiting_for_response_ = false;
    return;
  }
  
  // 检查是否有可用数据
  size_t available_bytes = 0;
  uart_get_buffered_data_len(this->uart_port_, &available_bytes);
  
  if (available_bytes == 0) {
    return;  // 无数据可读
  }
  
  ESP_LOGD(TAG, "📨 检测到UART数据: %d 字节", available_bytes);
  
  // 连续读取所有可用数据
  uint8_t data[256];
  int total_bytes_read = 0;
  
  while (available_bytes > 0) {
    int bytes_to_read = std::min(available_bytes, sizeof(data));
    int bytes_read = uart_read_bytes(this->uart_port_, data, bytes_to_read, pdMS_TO_TICKS(20));
    
    if (bytes_read > 0) {
      // 添加到响应缓冲区
      for (int i = 0; i < bytes_read; i++) {
        this->response_buffer_.push_back(data[i]);
      }
      total_bytes_read += bytes_read;
      this->last_data_receive_time_ = current_time;
      
      // 如果读取到数据，再多等待10ms以确保接收完整
      vTaskDelay(pdMS_TO_TICKS(10));
      
      // 检查是否还有更多数据
      uart_get_buffered_data_len(this->uart_port_, &available_bytes);
    } else {
      break;  // 无法读取更多数据
    }
  }
  
  if (total_bytes_read > 0) {
    ESP_LOGD(TAG, "📥 总共读取 %d 字节，缓冲区总长度: %d", 
             total_bytes_read, this->response_buffer_.size());
    
    // 检查帧完整性和解析
    this->check_and_parse_dlt645_frame();
  }
}

void HelloWorldComponent::check_and_parse_dlt645_frame() {
  // 检查最小帧长度
  if (this->response_buffer_.size() < 12) {
    return;  // 数据不够构成最小帧
  }
  
  ESP_LOGD(TAG, "📦 开始解析DL/T 645响应帧 (%d字节)", this->response_buffer_.size());
  
  // 输出完整的缓冲区数据用于调试
  std::string hex_data = "";
  for (size_t i = 0; i < this->response_buffer_.size(); i++) {
    char hex_str[4];
    sprintf(hex_str, "%02X ", this->response_buffer_[i]);
    hex_data += hex_str;
  }
  ESP_LOGD(TAG, "� 完整响应数据: %s", hex_data.c_str());
  
  // 寻找帧开始标志 (跳过前导符 0xFE)
  size_t frame_start = 0;
  while (frame_start < this->response_buffer_.size() && 
         this->response_buffer_[frame_start] == 0xFE) {
    frame_start++;
  }
  
  // 检查起始符
  if (frame_start >= this->response_buffer_.size() || 
      this->response_buffer_[frame_start] != 0x68) {
    ESP_LOGW(TAG, "⚠️ 未找到有效的帧起始符 (0x68)");
    this->response_buffer_.clear();
    this->waiting_for_response_ = false;
    return;
  }
  
  ESP_LOGD(TAG, "🔍 找到帧起始符，偏移量: %d", frame_start);
  
  // 检查帧长度是否足够 (至少需要: 起始符 + 地址6字节 + 起始符 + 控制码 + 长度 + 校验 + 结束符)
  size_t required_length = frame_start + 12;  // 最小帧长度
  if (this->response_buffer_.size() < required_length) {
    ESP_LOGD(TAG, "📦 帧数据不完整，等待更多数据...");
    return;
  }
  
  // 提取帧结构
  size_t idx = frame_start;
  
  // 验证第二个起始符 (索引 frame_start + 7)
  if (idx + 7 >= this->response_buffer_.size() || 
      this->response_buffer_[idx + 7] != 0x68) {
    ESP_LOGW(TAG, "⚠️ 第二个起始符 (0x68) 验证失败");
    this->response_buffer_.clear();
    this->waiting_for_response_ = false;
    return;
  }
  
  // 提取地址域 (6字节)
  std::vector<uint8_t> address(6);
  for (int i = 0; i < 6; i++) {
    address[i] = this->response_buffer_[idx + 1 + i];
  }
  
  // 提取控制码和数据长度
  uint8_t control_code = this->response_buffer_[idx + 8];
  uint8_t data_length = this->response_buffer_[idx + 9];
  
  ESP_LOGD(TAG, "📋 地址: %02X %02X %02X %02X %02X %02X, 控制码: 0x%02X, 数据长度: %d", 
           address[0], address[1], address[2], address[3], address[4], address[5], 
           control_code, data_length);
  
  // 检查是否是正常响应 (0x91) 或错误响应 (0xD1/0xB1)
  if (control_code == 0xD1 || control_code == 0xB1) {
    ESP_LOGW(TAG, "⚠️ 电表响应错误，控制码: 0x%02X", control_code);
    this->response_buffer_.clear();
    this->waiting_for_response_ = false;
    return;
  }
  
  if (control_code != 0x91) {
    ESP_LOGW(TAG, "⚠️ 未知的控制码: 0x%02X", control_code);
    this->response_buffer_.clear();
    this->waiting_for_response_ = false;
    return;
  }
  
  // 检查数据区长度
  size_t frame_total_length = idx + 10 + data_length + 2;  // +2 for checksum and end delimiter
  if (this->response_buffer_.size() < frame_total_length) {
    ESP_LOGD(TAG, "📦 等待完整帧数据 (期望 %d 字节，当前 %d 字节)", 
             frame_total_length, this->response_buffer_.size());
    return;
  }
  
  // 验证结束符
  if (this->response_buffer_[frame_total_length - 1] != 0x16) {
    ESP_LOGW(TAG, "⚠️ 帧结束符 (0x16) 验证失败: 0x%02X", 
             this->response_buffer_[frame_total_length - 1]);
    this->response_buffer_.clear();
    this->waiting_for_response_ = false;
    return;
  }
  
  // 验证校验和
  uint8_t calculated_checksum = 0;
  for (size_t i = idx; i < idx + 10 + data_length; i++) {
    calculated_checksum += this->response_buffer_[i];
  }
  uint8_t received_checksum = this->response_buffer_[idx + 10 + data_length];
  
  if (calculated_checksum != received_checksum) {
    ESP_LOGW(TAG, "⚠️ 校验和验证失败 (计算: 0x%02X, 接收: 0x%02X)", 
             calculated_checksum, received_checksum);
    this->response_buffer_.clear();
    this->waiting_for_response_ = false;
    return;
  }
  
  ESP_LOGD(TAG, "✅ DL/T 645帧验证成功，开始解析数据域");
  
  // 提取并解扰数据域
  std::vector<uint8_t> data_field(data_length);
  for (int i = 0; i < data_length; i++) {
    data_field[i] = this->response_buffer_[idx + 10 + i];
  }
  
  // 解扰数据域 (减去 0x33)
  unscramble_dlt645_data(data_field);
  
  // 解析数据标识符 (前4字节，LSB优先)
  if (data_length >= 4) {
    uint32_t data_identifier = data_field[0] | (data_field[1] << 8) | 
                              (data_field[2] << 16) | (data_field[3] << 24);
    
    ESP_LOGD(TAG, "🎯 数据标识符: 0x%08X", data_identifier);
    
    // 根据数据标识符解析相应的数据
    parse_dlt645_data_by_identifier(data_identifier, data_field);
  }
  
  // 更新电表地址
  if (address[0] != 0x99 || address[1] != 0x99) {  // 非广播地址
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
      this->meter_address_bytes_ = address;
      ESP_LOGI(TAG, "📍 更新电表地址: %02X %02X %02X %02X %02X %02X", 
           address[0], address[1], address[2], address[3], address[4], address[5]);
      this->device_address_discovered_ = true;
    }
  }
  
  // 清空缓冲区并重置等待状态
  this->response_buffer_.clear();
  this->waiting_for_response_ = false;
  
  ESP_LOGD(TAG, "📦 DL/T 645帧解析完成");
}

// ============= DL/T 645-2007 帧构建辅助函数 =============

std::vector<uint8_t> HelloWorldComponent::build_dlt645_read_frame(
    const std::vector<uint8_t>& address, uint32_t data_identifier) {
  
  std::vector<uint8_t> frame;
  
  // 1. 前导符 (可选，用于唤醒电表)
  frame.push_back(0xFE);
  frame.push_back(0xFE);
  
  // 2. 起始符
  frame.push_back(0x68);
  
  // 3. 地址域 (6字节，LSB优先)
  for (size_t i = 0; i < 6 && i < address.size(); i++) {
    frame.push_back(address[i]);
  }
  
  // 4. 第二个起始符
  frame.push_back(0x68);
  
  // 5. 控制码 (0x11 = 读数据)
  frame.push_back(0x11);
  
  // 6. 数据长度 (4字节数据标识符)
  frame.push_back(0x04);
  
  // 7. 数据域：数据标识符 (4字节，LSB优先，需要加0x33加扰)
  uint8_t di_bytes[4];
  di_bytes[0] = (data_identifier & 0xFF) + 0x33;
  di_bytes[1] = ((data_identifier >> 8) & 0xFF) + 0x33;
  di_bytes[2] = ((data_identifier >> 16) & 0xFF) + 0x33;
  di_bytes[3] = ((data_identifier >> 24) & 0xFF) + 0x33;
  
  for (int i = 0; i < 4; i++) {
    frame.push_back(di_bytes[i]);
  }
  
  // 8. 校验和 (从第一个0x68到数据域最后一个字节的模256和)
  uint8_t checksum = 0;
  for (size_t i = 2; i < frame.size(); i++) {  // 跳过前导符
    checksum += frame[i];
  }
  frame.push_back(checksum);
  
  // 9. 结束符
  frame.push_back(0x16);
  
  ESP_LOGD(TAG, "🔧 构建DL/T 645读帧: 地址=%02X%02X%02X%02X%02X%02X, DI=0x%08X", 
           address[0], address[1], address[2], address[3], address[4], address[5], 
           data_identifier);
  
  return frame;
}

// 数据加扰/解扰函数
void HelloWorldComponent::scramble_dlt645_data(std::vector<uint8_t>& data) {
  for (size_t i = 0; i < data.size(); i++) {
    data[i] += 0x33;
  }
}

void HelloWorldComponent::unscramble_dlt645_data(std::vector<uint8_t>& data) {
  for (size_t i = 0; i < data.size(); i++) {
    data[i] -= 0x33;
  }
}

// BCD到浮点转换函数
float HelloWorldComponent::bcd_to_float(const std::vector<uint8_t>& bcd_data, int decimal_places) {
  uint32_t int_value = 0;
  uint32_t multiplier = 1;
  
  for (size_t i = 0; i < bcd_data.size(); i++) {
    uint8_t low_nibble = bcd_data[i] & 0x0F;
    uint8_t high_nibble = (bcd_data[i] >> 4) & 0x0F;
    
    // 检查BCD有效性
    if (low_nibble > 9 || high_nibble > 9) {
      ESP_LOGW(TAG, "⚠️ 无效BCD数据: 0x%02X", bcd_data[i]);
      return 0.0f;
    }
    
    int_value += low_nibble * multiplier;
    multiplier *= 10;
    int_value += high_nibble * multiplier;
    multiplier *= 10;
  }
  
  return (float)int_value / pow(10, decimal_places);
}

// ============= DL/T 645-2007 设备地址发现和数据查询函数 =============

bool HelloWorldComponent::discover_meter_address() {
  if (!this->uart_initialized_) {
    ESP_LOGE(TAG, "❌ UART未初始化，无法执行地址发现");
    return false;
  }
  
  ESP_LOGI(TAG, "🔍 开始DL/T 645电表地址发现...");
  
  // 使用广播地址 99 99 99 99 99 99
  std::vector<uint8_t> broadcast_address = {0x99, 0x99, 0x99, 0x99, 0x99, 0x99};
  
  // 数据标识符：设备地址查询 (0x04000401)
  uint32_t device_address_di = 0x04000401;
  
  // 构建地址发现帧
  std::vector<uint8_t> discover_frame = build_dlt645_read_frame(broadcast_address, device_address_di);
  
  ESP_LOGD(TAG, "📡 发送地址发现命令，使用广播地址和DI=0x04000401");
  
  // 发送地址发现帧
  bool success = send_dlt645_frame(discover_frame);
  
  if (success) {
    ESP_LOGD(TAG, "✅ 地址发现命令已发送，等待电表响应...");
    
    // 触发设备地址查询事件
    // 注意：这里使用EVENT_DI_DEVICE_ADDRESS事件位
    if (this->event_group_ != nullptr) {
      xEventGroupSetBits(this->event_group_, EVENT_DI_DEVICE_ADDRESS);
    }
  } else {
    ESP_LOGE(TAG, "❌ 地址发现命令发送失败");
  }
  
  return success;
}

bool HelloWorldComponent::query_active_power_total() {
  if (!this->uart_initialized_) {
    ESP_LOGE(TAG, "❌ UART未初始化，无法查询总有功功率");
    return false;
  }
  
  // 检查是否已经获得电表地址
  if (this->meter_address_bytes_.empty() || 
      (this->meter_address_bytes_.size() == 6 && 
       this->meter_address_bytes_[0] == 0x99)) {
    ESP_LOGW(TAG, "⚠️ 电表地址未知，使用广播地址查询总功率");
    // 继续使用广播地址，某些电表支持
  }
  
  ESP_LOGI(TAG, "⚡ 查询DL/T 645电表总有功功率...");
  
  // 使用当前已知地址（如果有的话），否则使用广播地址
  std::vector<uint8_t> meter_address = this->meter_address_bytes_;
  if (meter_address.empty()) {
    meter_address = {0x99, 0x99, 0x99, 0x99, 0x99, 0x99};  // 广播地址
    ESP_LOGD(TAG, "📡 使用广播地址查询总功率");
  } else {
    ESP_LOGD(TAG, "📡 使用电表地址: %02X %02X %02X %02X %02X %02X", 
             meter_address[0], meter_address[1], meter_address[2], 
             meter_address[3], meter_address[4], meter_address[5]);
  }
  
  // 数据标识符：总有功功率 (0x02030000)
  uint32_t active_power_total_di = 0x02030000;
  
  // 构建功率查询帧
  std::vector<uint8_t> power_query_frame = build_dlt645_read_frame(meter_address, active_power_total_di);
  
  ESP_LOGD(TAG, "📊 发送总有功功率查询命令，DI=0x02030000");
  
  // 发送功率查询帧
  bool success = send_dlt645_frame(power_query_frame);
  
  if (success) {
    ESP_LOGD(TAG, "✅ 总有功功率查询命令已发送，等待电表响应...");
  } else {
    ESP_LOGE(TAG, "❌ 总有功功率查询命令发送失败");
  }
  
  return success;
}

// 根据数据标识符解析DL/T 645数据
void HelloWorldComponent::parse_dlt645_data_by_identifier(uint32_t data_identifier, const std::vector<uint8_t>& data_field) {
  ESP_LOGD(TAG, "🔍 解析DL/T 645数据 - DI: 0x%08X, 数据长度: %d", data_identifier, data_field.size());
  
  // 跳过数据标识符 (前4字节)，获取实际数据
  if (data_field.size() <= 4) {
    ESP_LOGW(TAG, "⚠️ 数据长度不足，无法解析");
    return;
  }
  
  std::vector<uint8_t> actual_data(data_field.begin() + 4, data_field.end());
  
  // 输出实际数据用于调试
  std::string hex_str = "";
  for (size_t i = 0; i < actual_data.size(); i++) {
    char hex[4];
    sprintf(hex, "%02X ", actual_data[i]);
    hex_str += hex;
  }
  ESP_LOGD(TAG, "📊 实际数据 (%d字节): %s", actual_data.size(), hex_str.c_str());
  
  switch (data_identifier) {
    case 0x04000401: {  // 设备地址查询
      ESP_LOGI(TAG, "🔍 [设备地址查询] 响应已接收");
      // 设备地址信息通常在帧的地址域中，这里主要确认查询成功
      
      // 保存数据标识符并设置事件位（线程安全）
      this->cached_data_identifier_ = data_identifier;
      if (this->event_group_ != nullptr) {
        xEventGroupSetBits(this->event_group_, EVENT_DI_DEVICE_ADDRESS);
      }
      break;
    }
    
    case 0x02030000: {  // 总有功功率
      if (actual_data.size() >= 3) {
        // DL/T 645功率格式：3字节BCD，XX.XXXX kW (4位小数)
        float power_kw = bcd_to_float(actual_data, 4);
        
        // 检查符号位 (最高字节的最高位)
        if (actual_data[2] & 0x80) {
          power_kw = -power_kw;
        }
        
        // 转换为W单位存储
        float power_w = power_kw * 1000.0f;
        
        ESP_LOGI(TAG, "⚡ [总有功功率] %.1f W (%.4f kW)", power_w, power_kw);
        
        // 保存数据到缓存变量并设置事件位（线程安全）
        this->cached_active_power_w_ = power_w;
        this->cached_data_identifier_ = data_identifier;
        if (this->event_group_ != nullptr) {
          xEventGroupSetBits(this->event_group_, EVENT_DI_ACTIVE_POWER_TOTAL);
        }
      } else {
        ESP_LOGW(TAG, "⚠️ 总有功功率数据长度不足");
      }
      break;
    }
    
    case 0x00010000: {  // 正向有功总电能
      if (actual_data.size() >= 4) {
        // DL/T 645电能格式：4字节BCD，XXXXXX.XX kWh (2位小数)
        float energy_kwh = bcd_to_float(actual_data, 2);
        
        ESP_LOGI(TAG, "🔋 [正向有功总电能] %.2f kWh", energy_kwh);
        
        // 保存数据到缓存变量并设置事件位（线程安全）
        this->cached_energy_active_kwh_ = energy_kwh;
        this->cached_data_identifier_ = data_identifier;
        if (this->event_group_ != nullptr) {
          xEventGroupSetBits(this->event_group_, EVENT_DI_ENERGY_ACTIVE_TOTAL);
        }
      } else {
        ESP_LOGW(TAG, "⚠️ 正向有功总电能数据长度不足");
      }
      break;
    }
    
    case 0x02010100: {  // A相电压
      if (actual_data.size() >= 2) {
        // DL/T 645电压格式：2字节BCD，XXX.X V (1位小数)
        float voltage_v = bcd_to_float(actual_data, 1);
        
        ESP_LOGI(TAG, "🔌 [A相电压] %.1f V", voltage_v);
        
        // 保存数据到缓存变量并设置事件位（线程安全）
        this->cached_voltage_a_v_ = voltage_v;
        this->cached_data_identifier_ = data_identifier;
        if (this->event_group_ != nullptr) {
          xEventGroupSetBits(this->event_group_, EVENT_DI_VOLTAGE_A_PHASE);
        }
      } else {
        ESP_LOGW(TAG, "⚠️ A相电压数据长度不足");
      }
      break;
    }
    
    case 0x02020100: {  // A相电流
      if (actual_data.size() >= 3) {
        // DL/T 645电流格式：3字节BCD，XXX.XXX A (3位小数)
        float current_a = bcd_to_float(actual_data, 3);
        
        ESP_LOGI(TAG, "🔄 [A相电流] %.3f A", current_a);
        
        // 保存数据到缓存变量并设置事件位（线程安全）
        this->cached_current_a_a_ = current_a;
        this->cached_data_identifier_ = data_identifier;
        if (this->event_group_ != nullptr) {
          xEventGroupSetBits(this->event_group_, EVENT_DI_CURRENT_A_PHASE);
        }
      } else {
        ESP_LOGW(TAG, "⚠️ A相电流数据长度不足");
      }
      break;
    }
    
    case 0x02060000: {  // 总功率因数
      if (actual_data.size() >= 2) {
        // DL/T 645功率因数格式：2字节BCD，X.XXX (3位小数)
        float power_factor = bcd_to_float(actual_data, 3);
        
        ESP_LOGI(TAG, "📈 [总功率因数] %.3f", power_factor);
        
        // 保存数据到缓存变量并设置事件位（线程安全）
        this->cached_power_factor_ = power_factor;
        this->cached_data_identifier_ = data_identifier;
        if (this->event_group_ != nullptr) {
          xEventGroupSetBits(this->event_group_, EVENT_DI_POWER_FACTOR_TOTAL);
        }
      } else {
        ESP_LOGW(TAG, "⚠️ 总功率因数数据长度不足");
      }
      break;
    }
    
    case 0x02800002: {  // 电网频率
      if (actual_data.size() >= 2) {
        // DL/T 645频率格式：2字节BCD，XX.XX Hz (2位小数)
        float frequency_hz = bcd_to_float(actual_data, 2);
        
        ESP_LOGI(TAG, "🌊 [电网频率] %.2f Hz", frequency_hz);
        
        // 保存数据到缓存变量并设置事件位（线程安全）
        this->cached_frequency_hz_ = frequency_hz;
        this->cached_data_identifier_ = data_identifier;
        if (this->event_group_ != nullptr) {
          xEventGroupSetBits(this->event_group_, EVENT_DI_FREQUENCY);
        }
      } else {
        ESP_LOGW(TAG, "⚠️ 电网频率数据长度不足");
      }
      break;
    }
    
    case 0x00020000: {  // 反向有功总电能
      if (actual_data.size() >= 4) {
        // DL/T 645电能格式：4字节BCD，XXXXXX.XX kWh (2位小数)
        float energy_kwh = bcd_to_float(actual_data, 2);
        
        ESP_LOGI(TAG, "🔄 [反向有功总电能] %.2f kWh", energy_kwh);
        
        // 保存数据到缓存变量并设置事件位（线程安全）
        this->cached_energy_reverse_kwh_ = energy_kwh;
        this->cached_data_identifier_ = data_identifier;
        if (this->event_group_ != nullptr) {
          xEventGroupSetBits(this->event_group_, EVENT_DI_ENERGY_REVERSE_TOTAL);
        }
      } else {
        ESP_LOGW(TAG, "⚠️ 反向有功总电能数据长度不足");
      }
      break;
    }
    
    case 0x04000101: {  // 日期时间
      // 输出原始数据用于调试
      std::string hex_str = "";
      for (size_t i = 0; i < actual_data.size(); i++) {
        char hex[4];
        sprintf(hex, "%02X ", actual_data[i]);
        hex_str += hex;
      }
      ESP_LOGI(TAG, "📊 日期时间原始数据 (%d字节): %s", actual_data.size(), hex_str.c_str());
      
      if (actual_data.size() == 4) {
        // 4字节格式 WDMY - 根据用户分析：星期-日-月-年
        auto bcd_to_byte = [](uint8_t bcd) -> int {
            return ((bcd >> 4) & 0x0F) * 10 + (bcd & 0x0F);
        };
        
        // 按WDMY格式解析：00 05 10 25 → 星期天 5日 10月 2025年
        int week_day = bcd_to_byte(actual_data[0]);    // W: 星期 (0=星期天, 1=星期一, ...)
        int day = bcd_to_byte(actual_data[1]);         // D: 日 (1-31)
        int month = bcd_to_byte(actual_data[2]);       // M: 月 (1-12)
        int year = bcd_to_byte(actual_data[3]);        // Y: 年 (0-99, 通常表示20xx)
        
        // 验证数据合理性
        bool is_valid = (week_day <= 6) && (day >= 1 && day <= 31) && 
                       (month >= 1 && month <= 12) && (year <= 99);
        
        char datetime_str[64];
        
        if (is_valid) {
            // 转换年份：25 -> 2025
            int full_year = (year < 50) ? 2000 + year : 1900 + year;
            
            // 中文星期名称
            const char* weekdays[] = {"日", "一", "二", "三", "四", "五", "六"};
            
            snprintf(datetime_str, sizeof(datetime_str), 
                     "%04d-%02d-%02d (星期%s)", 
                     full_year, month, day, weekdays[week_day]);
            
            ESP_LOGI(TAG, "📅 [日期时间-4字节WDMY] %s", datetime_str);
        } else {
            // 数据无效，显示原始值
            snprintf(datetime_str, sizeof(datetime_str), 
                     "INVALID_WDMY: W%02d-D%02d-M%02d-Y%02d", 
                     week_day, day, month, year);
            ESP_LOGW(TAG, "❌ 日期数据无效: %s", datetime_str);
        }
        
        // 保存数据到缓存变量并设置事件位（线程安全）
        this->cached_datetime_str_ = std::string(datetime_str);
        this->cached_data_identifier_ = data_identifier;
        if (this->event_group_ != nullptr) {
          xEventGroupSetBits(this->event_group_, EVENT_DI_DATETIME);
        }
      } else if (actual_data.size() >= 6) {
        // 6字节或更多字节格式：DL/T 645-2007 标准格式
        // 格式化日期时间字符串
        char datetime_str[32];
        snprintf(datetime_str, sizeof(datetime_str), "%02X%02X年%02X月%02X日%02X时%02X分", 
                 actual_data[1], actual_data[0], actual_data[2], actual_data[3], 
                 actual_data[4], actual_data[5]);
        
        ESP_LOGI(TAG, "📅 [日期时间-6+字节] %s", datetime_str);
        
        // 保存数据到缓存变量并设置事件位（线程安全）
        this->cached_datetime_str_ = std::string(datetime_str);
        this->cached_data_identifier_ = data_identifier;
        if (this->event_group_ != nullptr) {
          xEventGroupSetBits(this->event_group_, EVENT_DI_DATETIME);
        }
      } else {
        ESP_LOGW(TAG, "❌ 日期时间数据长度异常: %d 字节 - 原始数据: %s", actual_data.size(), hex_str.c_str());
      }
      break;
    }
    
    case 0x04000102: {  // 时分秒
      if (actual_data.size() >= 3) {
        // 格式化时分秒字符串
        char time_hms_str[16];
        snprintf(time_hms_str, sizeof(time_hms_str), "%02X时%02X分%02X秒", 
                 actual_data[0], actual_data[1], actual_data[2]);
        
        ESP_LOGI(TAG, "⏰ [时分秒] %s", time_hms_str);
        
        // 保存数据到缓存变量并设置事件位（线程安全）
        this->cached_time_hms_str_ = std::string(time_hms_str);
        this->cached_data_identifier_ = data_identifier;
        if (this->event_group_ != nullptr) {
          xEventGroupSetBits(this->event_group_, EVENT_DI_TIME_HMS);
        }
      } else {
        ESP_LOGW(TAG, "⚠️ 时分秒数据长度不足");
      }
      break;
    }
    
    default: {
      ESP_LOGW(TAG, "⚠️ 未知的数据标识符: 0x%08X", data_identifier);
      break;
    }
  }
}

#endif  // defined(USE_ESP32) || defined(USE_ESP_IDF)

}  // namespace hello_world_component
}  // namespace esphome
