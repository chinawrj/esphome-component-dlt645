"""
ESPHome DLT645 Component
An ESPHome component for DL/T 645-2007 smart meter communication protocol,
supporting UART communication and real-time monitoring of various electrical parameters.
"""

from esphome import automation
from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
    CONF_RX_PIN,
    CONF_TX_PIN,
)

# 
CONF_POWER_RATIO = "power_ratio"
CONF_BAUD_RATE = "baud_rate"
CONF_RX_BUFFER_SIZE = "rx_buffer_size"
CONF_SIMULATE = "simulate"

# DL/T 645-2007 
CONF_ON_DEVICE_ADDRESS = "on_device_address"
CONF_ON_ACTIVE_POWER = "on_active_power"
CONF_ON_WARNING_REVERSE_POWER = "on_warning_reverse_power"
CONF_ON_ENERGY_ACTIVE = "on_energy_active"
CONF_ON_VOLTAGE_A = "on_voltage_a"
CONF_ON_CURRENT_A = "on_current_a"
CONF_ON_POWER_FACTOR = "on_power_factor"
CONF_ON_FREQUENCY = "on_frequency"
CONF_ON_ENERGY_REVERSE = "on_energy_reverse"
CONF_ON_DATETIME = "on_datetime"
CONF_ON_TIME_HMS = "on_time_hms"

# 
dlt645_component_ns = cg.esphome_ns.namespace("dlt645_component")

# 
DLT645Component = dlt645_component_ns.class_("DLT645Component", cg.Component)

# DL/T 645-2007 
DeviceAddressTrigger = dlt645_component_ns.class_(
    "DeviceAddressTrigger", automation.Trigger.template(cg.uint32)
)
ActivePowerTrigger = dlt645_component_ns.class_(
    "ActivePowerTrigger", automation.Trigger.template(cg.uint32, cg.float_)
)
WarningReversePowerTrigger = dlt645_component_ns.class_(
    "WarningReversePowerTrigger", automation.Trigger.template(cg.uint32, cg.float_)
)
EnergyActiveTrigger = dlt645_component_ns.class_(
    "EnergyActiveTrigger", automation.Trigger.template(cg.uint32, cg.float_)
)
VoltageATrigger = dlt645_component_ns.class_(
    "VoltageATrigger", automation.Trigger.template(cg.uint32, cg.float_)
)
CurrentATrigger = dlt645_component_ns.class_(
    "CurrentATrigger", automation.Trigger.template(cg.uint32, cg.float_)
)
PowerFactorTrigger = dlt645_component_ns.class_(
    "PowerFactorTrigger", automation.Trigger.template(cg.uint32, cg.float_)
)
FrequencyTrigger = dlt645_component_ns.class_(
    "FrequencyTrigger", automation.Trigger.template(cg.uint32, cg.float_)
)
EnergyReverseTrigger = dlt645_component_ns.class_(
    "EnergyReverseTrigger", automation.Trigger.template(cg.uint32, cg.float_)
)
DatetimeTrigger = dlt645_component_ns.class_(
    "DatetimeTrigger", automation.Trigger.template(cg.uint32, cg.uint32, cg.uint32, cg.uint32, cg.uint32)
)
TimeHmsTrigger = dlt645_component_ns.class_(
    "TimeHmsTrigger", automation.Trigger.template(cg.uint32, cg.uint32, cg.uint32, cg.uint32)
)

# DL/T 645-2007 Relay Control and DateTime Setting Actions
RelayTripAction = dlt645_component_ns.class_("RelayTripAction", automation.Action)
RelayCloseAction = dlt645_component_ns.class_("RelayCloseAction", automation.Action)
SetDatetimeAction = dlt645_component_ns.class_("SetDatetimeAction", automation.Action)
SetTimeAction = dlt645_component_ns.class_("SetTimeAction", automation.Action)
BroadcastTimeSyncAction = dlt645_component_ns.class_("BroadcastTimeSyncAction", automation.Action)

# Component configuration
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DLT645Component),
        cv.Optional(CONF_POWER_RATIO, default=10): cv.int_range(min=1, max=100),
        cv.Optional(CONF_TX_PIN, default=1): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_RX_PIN, default=2): pins.internal_gpio_input_pin_number,
        cv.Optional(CONF_BAUD_RATE, default=2400): cv.int_range(min=1200, max=9600),
        cv.Optional(CONF_RX_BUFFER_SIZE, default=256): cv.int_range(min=128, max=1024),
        cv.Optional(CONF_SIMULATE, default=False): cv.boolean,
        
        # DL/T 645-2007 
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
        cv.Optional(CONF_ON_WARNING_REVERSE_POWER): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(WarningReversePowerTrigger),
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
    """"""
    # 
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # 
    cg.add(var.set_power_ratio(config[CONF_POWER_RATIO]))
    cg.add(var.set_tx_pin(config[CONF_TX_PIN]))
    cg.add(var.set_rx_pin(config[CONF_RX_PIN]))
    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))
    cg.add(var.set_rx_buffer_size(config[CONF_RX_BUFFER_SIZE]))
    cg.add(var.set_simulate(config[CONF_SIMULATE]))
    
    # DL/T 645-2007
    
    # Device address query (DI: 0x04000401)
    for conf in config.get(CONF_ON_DEVICE_ADDRESS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier")], conf)
    
    # Total active power (DI: 0x02030000) - Parameters: data_identifier + power_watts (Unit: W)
    for conf in config.get(CONF_ON_ACTIVE_POWER, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.float_, "power_watts")], conf)
    
    # Reverse power warning (DI: 0x02030000) - Triggers only on >=0 to <0 transition
    for conf in config.get(CONF_ON_WARNING_REVERSE_POWER, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.float_, "power_watts")], conf)
    
    # Total active energy (DI: 0x00010000) - Parameters: data_identifier + energy_kwh (Unit: kWh)
    for conf in config.get(CONF_ON_ENERGY_ACTIVE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.float_, "energy_kwh")], conf)
    
    # A (DI: 0x02010100) - ：data_identifier + voltage_v (: V)
    for conf in config.get(CONF_ON_VOLTAGE_A, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.float_, "voltage_v")], conf)
    
    # A (DI: 0x02020100) - ：data_identifier + current_a (: A)
    for conf in config.get(CONF_ON_CURRENT_A, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.float_, "current_a")], conf)
    
    #  (DI: 0x02060000)
    for conf in config.get(CONF_ON_POWER_FACTOR, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.float_, "power_factor")], conf)
    
    #  (DI: 0x02800002)
    for conf in config.get(CONF_ON_FREQUENCY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.float_, "frequency_hz")], conf)
    
    #  (DI: 0x00020000)
    for conf in config.get(CONF_ON_ENERGY_REVERSE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.float_, "energy_reverse_kwh")], conf)
    
    #  (DI: 0x04000101)
    for conf in config.get(CONF_ON_DATETIME, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.uint32, "year"), (cg.uint32, "month"), (cg.uint32, "day"), (cg.uint32, "weekday")], conf)
    
    #  (DI: 0x04000102)
    for conf in config.get(CONF_ON_TIME_HMS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "data_identifier"), (cg.uint32, "hour"), (cg.uint32, "minute"), (cg.uint32, "second")], conf)


# DL/T 645-2007 继电器控制 Actions
@automation.register_action(
    "dlt645_component.relay_trip",
    RelayTripAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DLT645Component),
        }
    ),
)
async def relay_trip_action_to_code(config, action_id, template_arg, args):
    """拉闸动作 (Trip/Open relay) - 断开电表继电器"""
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action(
    "dlt645_component.relay_close",
    RelayCloseAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DLT645Component),
        }
    ),
)
async def relay_close_action_to_code(config, action_id, template_arg, args):
    """合闸动作 (Close relay) - 闭合电表继电器"""
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action(
    "dlt645_component.set_datetime",
    SetDatetimeAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DLT645Component),
        }
    ),
)
async def set_datetime_action_to_code(config, action_id, template_arg, args):
    """Set meter date - Automatically gets and sets meter date from system time (WW DD MM YY - 4 bytes)"""
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action(
    "dlt645_component.set_time",
    SetTimeAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DLT645Component),
        }
    ),
)
async def set_time_action_to_code(config, action_id, template_arg, args):
    """Set meter time - Automatically gets and sets meter time from system time (HH mm SS - 3 bytes)"""
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action(
    "dlt645_component.broadcast_time_sync",
    BroadcastTimeSyncAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DLT645Component),
        }
    ),
)
async def broadcast_time_sync_action_to_code(config, action_id, template_arg, args):
    """Broadcast time synchronization - Uses control code 0x08 to sync time to all meters (YY MM DD HH mm - 5 bytes)"""
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)
