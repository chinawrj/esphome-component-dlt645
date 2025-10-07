# Hello World Component for ESPHome

这是一个用于演示ESPHome外部组件开发的Hello World示例组件。

## 功能特性

- 每5秒触发一次`hello_world`事件
- 支持可配置的magic_number参数
- 支持ESPHome自动化系统，可以使用`on_hello_world`触发器
- 事件数据包含magic_number值

## 配置选项

| 选项 | 类型 | 默认值 | 描述 |
|------|------|--------|------|
| `magic_number` | `uint32` | `42` | 在hello_world事件中传递的魔法数字 |
| `on_hello_world` | `Automation` | - | 当hello_world事件触发时执行的自动化动作 |

## 使用示例

### 基本配置

```yaml
external_components:
  - source:
      type: local
      path: hello_world_component

hello_world_component:
  id: my_hello_world
  magic_number: 123
  on_hello_world:
    then:
      - logger.log: 
          format: "收到Hello World事件! Magic Number: %d"
          args: ['magic_number']
```

### 完整示例

```yaml
esphome:
  name: hello-world-test
  platform: esp32
  board: esp32dev

logger:

wifi:
  ssid: "your_wifi_name"
  password: "your_wifi_password"

external_components:
  - source:
      type: local
      path: hello_world_component

hello_world_component:
  id: my_hello_world
  magic_number: 42
  on_hello_world:
    then:
      - logger.log: 
          format: "🌍 Hello World! Magic Number: %d"
          args: ['magic_number']
      - switch.toggle: my_led

switch:
  - platform: gpio
    pin: GPIO2
    name: "LED"
    id: my_led
```

## 安装说明

1. 在你的ESPHome配置目录中创建`hello_world_component`文件夹
2. 将此组件的所有文件复制到该文件夹中
3. 在你的ESPHome YAML文件中添加`external_components`配置
4. 添加`hello_world_component`配置

## 开发说明

此组件演示了以下ESPHome组件开发概念：

- **Python配置验证**: 使用`CONFIG_SCHEMA`验证YAML配置
- **C++组件实现**: 继承自`Component`类，实现生命周期方法
- **事件系统**: 使用`CallbackManager`实现事件回调
- **自动化集成**: 使用`Trigger`类支持ESPHome自动化系统
- **代码生成**: 使用`to_code`函数生成C++代码

## API参考

### HelloWorldComponent类

#### 方法

- `void set_magic_number(uint32_t magic_number)`: 设置魔法数字
- `void add_on_hello_world_callback(callback)`: 添加hello_world事件回调

#### 事件

- `on_hello_world(magic_number)`: 每5秒触发，携带magic_number参数