# DLT645 Component for ESPHome

This is an ESPHome external component for DL/T 645-2007 smart meter communication protocol, supporting UART communication and real-time monitoring of various electrical parameters.

## Features

- DL/T 645-2007 protocol implementation
- FreeRTOS task-based architecture
- UART communication support
- Multiple data identifier support
- Event-driven automation system
- Thread-safe implementation
- Real-time electrical parameter monitoring

### Data Identifiers Supported

- Device address query (0x04000401)
- Total active power (0x02030000)
- Forward active total energy (0x00010000)
- Phase A voltage (0x02010100)
- Phase A current (0x02020100)
- Total power factor (0x02060000)
- Grid frequency (0x02800002)
- Reverse active total energy (0x00020000)
- Date and time (0x04000101)
- Hours, minutes, seconds (0x04000102)
- Relay status (local state tracking - updated on control commands)

## Architecture

The component uses:
- FreeRTOS tasks for background processing
- Event groups for task synchronization  
- UART communication for DL/T 645 protocol
- ESPHome automation integration
- Thread-safe data caching

## Configuration Options

| Parameter | Type | Default | Description |
|--------|------|---------|-------------|
| `power_ratio` | `int` | `10` | Query ratio control for total power vs other parameters |
| `on_device_address` | `Automation` | - | Device address discovery event trigger |
| `on_active_power` | `Automation` | - | Active power data event trigger |
| `on_energy_active` | `Automation` | - | Active energy data event trigger |
| `on_voltage_a` | `Automation` | - | Phase A voltage data event trigger |
| `on_current_a` | `Automation` | - | Phase A current data event trigger |
| `on_power_factor` | `Automation` | - | Power factor data event trigger |
| `on_frequency` | `Automation` | - | Frequency data event trigger |
| `on_energy_reverse` | `Automation` | - | Reverse energy data event trigger |
| `on_datetime` | `Automation` | - | Date and time data event trigger |
| `on_time_hms` | `Automation` | - | Time (hours, minutes, seconds) data event trigger |
| `on_relay_status` | `Automation` | - | Relay status data event trigger (0=closed, 1=open, 2=fault) |

## Usage Example

### Basic Configuration

```yaml
external_components:
  - source: 
      type: local
      path: components

esphome:
  name: smart-meter-reader

esp32:
  board: esp32dev

logger:
  level: DEBUG

# UART configuration for DL/T 645
uart:
  tx_pin: GPIO1
  rx_pin: GPIO2
  baud_rate: 2400
  data_bits: 8
  parity: EVEN
  stop_bits: 1

dlt645_component:
  power_ratio: 10
  on_active_power:
    then:
      - logger.log: 
          format: "Active Power: %.2f W (DI: 0x%08X)"
          args: ['power_watts', 'data_identifier']
  on_voltage_a:
    then:
      - logger.log: 
          format: "Phase A Voltage: %.1f V"
          args: ['voltage_v']
  on_relay_status:
    then:
      - logger.log:
          format: "Relay Status: 0x%02X (DI: 0x%08X) - %s"
          args: ['status', 'data_identifier', 
                 'status == 0 ? "Closed" : (status == 1 ? "Open" : "Fault")']
```

## License

This component is provided as-is for educational and development purposes.