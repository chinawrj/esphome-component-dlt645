"""
ESPHome Hello World Component
一个用于演示ESPHome外部组件开发的Hello World示例，
每5秒触发一次hello_world事件，事件中包含可配置的magic_number数据。
"""
from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
)

# 组件配置常量
CONF_MAGIC_NUMBER = "magic_number"
CONF_POWER_RATIO = "power_ratio"
CONF_ON_HELLO_WORLD = "on_hello_world"

# DL/T 645-2007 数据标识符独立事件配置常量
CONF_ON_DEVICE_ADDRESS = "on_device_address"
CONF_ON_ACTIVE_POWER = "on_active_power"
CONF_ON_ENERGY_ACTIVE = "on_energy_active"
CONF_ON_VOLTAGE_A = "on_voltage_a"
CONF_ON_CURRENT_A = "on_current_a"
CONF_ON_POWER_FACTOR = "on_power_factor"
CONF_ON_FREQUENCY = "on_frequency"
CONF_ON_ENERGY_REVERSE = "on_energy_reverse"
CONF_ON_DATETIME = "on_datetime"
CONF_ON_TIME_HMS = "on_time_hms"

# 定义组件命名空间
hello_world_component_ns = cg.esphome_ns.namespace("hello_world_component")

# 声明主组件类
HelloWorldComponent = hello_world_component_ns.class_("HelloWorldComponent", cg.Component)

# 声明Trigger类，用于自动化
HelloWorldTrigger = hello_world_component_ns.class_(
    "HelloWorldTrigger", automation.Trigger.template(cg.uint32)
)

# DL/T 645-2007 数据标识符独立事件触发器类
DeviceAddressTrigger = hello_world_component_ns.class_(
    "DeviceAddressTrigger", automation.Trigger.template(cg.uint32)
)
ActivePowerTrigger = hello_world_component_ns.class_(
    "ActivePowerTrigger", automation.Trigger.template(cg.uint32, cg.float_)
)
EnergyActiveTrigger = hello_world_component_ns.class_(
    "EnergyActiveTrigger", automation.Trigger.template(cg.uint32, cg.float_)
)
VoltageATrigger = hello_world_component_ns.class_(
    "VoltageATrigger", automation.Trigger.template(cg.uint32, cg.float_)
)
CurrentATrigger = hello_world_component_ns.class_(
    "CurrentATrigger", automation.Trigger.template(cg.uint32, cg.float_)
)
PowerFactorTrigger = hello_world_component_ns.class_(
    "PowerFactorTrigger", automation.Trigger.template(cg.uint32, cg.float_)
)
FrequencyTrigger = hello_world_component_ns.class_(
    "FrequencyTrigger", automation.Trigger.template(cg.uint32, cg.float_)
)
EnergyReverseTrigger = hello_world_component_ns.class_(
    "EnergyReverseTrigger", automation.Trigger.template(cg.uint32, cg.float_)
)
DatetimeTrigger = hello_world_component_ns.class_(
    "DatetimeTrigger", automation.Trigger.template(cg.uint32, cg.uint32, cg.uint32, cg.uint32, cg.uint32)
)
TimeHmsTrigger = hello_world_component_ns.class_(
    "TimeHmsTrigger", automation.Trigger.template(cg.uint32, cg.uint32, cg.uint32, cg.uint32)
)

# 组件配置架构
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HelloWorldComponent),
        cv.Optional(CONF_MAGIC_NUMBER, default=42): cv.uint32_t,
        cv.Optional(CONF_POWER_RATIO, default=10): cv.int_range(min=1, max=100),
        
        # 原有的通用事件（保持向后兼容）
        cv.Optional(CONF_ON_HELLO_WORLD): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(HelloWorldTrigger),
            }
        ),
        
        # DL/T 645-2007 数据标识符独立事件
        cv.Optional(CONF_ON_DEVICE_ADDRESS): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DeviceAddressTrigger),
            }
        ),
        cv.Optional(CONF_ON_ACTIVE_POWER): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ActivePowerTrigger),
            }
        ),
        cv.Optional(CONF_ON_ENERGY_ACTIVE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(EnergyActiveTrigger),
            }
        ),
        cv.Optional(CONF_ON_VOLTAGE_A): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(VoltageATrigger),
            }
        ),
        cv.Optional(CONF_ON_CURRENT_A): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CurrentATrigger),
            }
        ),
        cv.Optional(CONF_ON_POWER_FACTOR): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(PowerFactorTrigger),
                cv.Optional("data_identifier"): cv.uint32_t,
                cv.Optional("power_factor"): cv.float_,
            }
        ),
        cv.Optional(CONF_ON_FREQUENCY): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FrequencyTrigger),
                cv.Optional("data_identifier"): cv.uint32_t,
                cv.Optional("frequency_hz"): cv.float_,
            }
        ),
        cv.Optional(CONF_ON_ENERGY_REVERSE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(EnergyReverseTrigger),
                cv.Optional("data_identifier"): cv.uint32_t,
                cv.Optional("energy_reverse_kwh"): cv.float_,
            }
        ),
        cv.Optional(CONF_ON_DATETIME): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DatetimeTrigger),
                cv.Optional("data_identifier"): cv.uint32_t,
                cv.Optional("year"): cv.uint32_t,
                cv.Optional("month"): cv.uint32_t,
                cv.Optional("day"): cv.uint32_t,
                cv.Optional("weekday"): cv.uint32_t,
            }
        ),
        cv.Optional(CONF_ON_TIME_HMS): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TimeHmsTrigger),
                cv.Optional("data_identifier"): cv.uint32_t,
                cv.Optional("hour"): cv.uint32_t,
                cv.Optional("minute"): cv.uint32_t,
                cv.Optional("second"): cv.uint32_t,
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """代码生成函数"""
    # 创建组件实例
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # 设置magic_number
    cg.add(var.set_magic_number(config[CONF_MAGIC_NUMBER]))
    
    # 设置总功率查询比例参数
    cg.add(var.set_power_ratio(config[CONF_POWER_RATIO]))
    
    # 注册原有的通用触发器（保持向后兼容）
    for conf in config.get(CONF_ON_HELLO_WORLD, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "magic_number")], conf)
    
    # 注册DL/T 645-2007数据标识符独立事件触发器
    
    # 设备地址查询事件 (DI: 0x04000401)
    for conf in config.get(CONF_ON_DEVICE_ADDRESS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier")], conf)
    
    # 总功率事件 (DI: 0x02030000) - 双参数：data_identifier + power_watts (单位: W)
    for conf in config.get(CONF_ON_ACTIVE_POWER, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.float_, "power_watts")], conf)
    
    # 总电能事件 (DI: 0x00010000) - 双参数：data_identifier + energy_kwh (单位: kWh)
    for conf in config.get(CONF_ON_ENERGY_ACTIVE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.float_, "energy_kwh")], conf)
    
    # A相电压事件 (DI: 0x02010100) - 双参数：data_identifier + voltage_v (单位: V)
    for conf in config.get(CONF_ON_VOLTAGE_A, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.float_, "voltage_v")], conf)
    
    # A相电流事件 (DI: 0x02020100) - 双参数：data_identifier + current_a (单位: A)
    for conf in config.get(CONF_ON_CURRENT_A, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.float_, "current_a")], conf)
    
    # 功率因数事件 (DI: 0x02060000)
    for conf in config.get(CONF_ON_POWER_FACTOR, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.float_, "power_factor")], conf)
    
    # 频率事件 (DI: 0x02800002)
    for conf in config.get(CONF_ON_FREQUENCY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.float_, "frequency_hz")], conf)
    
    # 反向总电能事件 (DI: 0x00020000)
    for conf in config.get(CONF_ON_ENERGY_REVERSE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.float_, "energy_reverse_kwh")], conf)
    
    # 日期时间事件 (DI: 0x04000101)
    for conf in config.get(CONF_ON_DATETIME, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.uint32, "year"), (cg.uint32, "month"), (cg.uint32, "day"), (cg.uint32, "weekday")], conf)
    
    # 时分秒事件 (DI: 0x04000102)
    for conf in config.get(CONF_ON_TIME_HMS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.uint32, "hour"), (cg.uint32, "minute"), (cg.uint32, "second")], conf)