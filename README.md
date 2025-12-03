# Fluxgen ESP32 Modbus IoT Gateway - Stable Release

[![Stable Release](https://img.shields.io/badge/Status-STABLE-brightgreen.svg)](PRODUCTION_GUIDE.md)
[![Version](https://img.shields.io/badge/Version-1.1.0-blue.svg)](VERSION.md)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.4-orange.svg)](https://docs.espressif.com/projects/esp-idf/en/v5.4/)
[![64-bit Support](https://img.shields.io/badge/64--bit-Complete-purple.svg)](#)
[![License](https://img.shields.io/badge/License-Industrial-yellow.svg)](#)

## Professional Industrial IoT Gateway

A production-ready ESP32-based Modbus IoT gateway designed for industrial environments. Features real-time RS485 Modbus communication, comprehensive ScadaCore data format support, SD card offline caching, and seamless Azure IoT Hub integration.

## Key Features

### Industrial Grade Communication
- **Real-time RS485 Modbus RTU** with professional error handling
- **20+ comprehensive data formats** including all ScadaCore interpretations
- **Robust diagnostics** with detailed troubleshooting guides
- **Support for up to 8 sensors** per gateway
- **Multiple flow meter types** with specialized data decoding

### Professional Web Interface
- **Responsive industrial design** optimized for field use
- **Individual sensor management** - Add, Edit, Delete, Test independently
- **Real-time testing** with comprehensive format display
- **Professional branding** with customizable company logos
- **Dark gradient theme** for better visibility

### Enterprise Cloud Integration
- **Azure IoT Hub connectivity** with secure MQTT communication
- **Configurable telemetry intervals** (30-3600 seconds)
- **Automatic reconnection** and error recovery
- **Persistent configuration** in flash memory
- **SD Card offline caching** with chronological replay

### Dual Connectivity Modes
- **WiFi Mode** - Connect to existing WiFi networks
- **SIM/4G Mode** - A7670C cellular modem with PPP support
- **Automatic failover** and recovery

## Supported Sensor Types

### Flow Meters (Totalizer/Cumulative)

| Sensor Type | Data Format | Registers | Description |
|-------------|-------------|-----------|-------------|
| **Flow-Meter** | UINT32_BADC + FLOAT32_BADC | 4 | Generic flow meter |
| **Panda_USM** | FLOAT64_BE (Double) | 4 | Panda Ultrasonic meter |
| **Clampon** | UINT32_BADC + FLOAT32_BADC | 4 | Clampon flow meter |
| **ZEST** | UINT16 + FLOAT32_BE | 4 | AquaGen ZEST meter |
| **Dailian** | UINT32_3412 (CDAB) | 2 | Dailian Ultrasonic |
| **Dailian_EMF** | UINT32 word-swapped | 2 | Dailian EMF meter |
| **Panda_EMF** | INT32_BE + FLOAT32_BE | 4 | Panda EMF meter |

### Level Sensors

| Sensor Type | Data Format | Calculation |
|-------------|-------------|-------------|
| **Level** | User-selectable | Level % = ((Sensor Height - Raw) / Tank Height) x 100 |
| **Radar Level** | User-selectable | Level % = (Raw / Max Water Level) x 100 |
| **Panda_Level** | UINT16 | Level % = ((Sensor Height - Raw) / Tank Height) x 100 |
| **Piezometer** | UINT16_HI (fixed) | Raw value x scale factor |

### Other Sensors

| Sensor Type | Description |
|-------------|-------------|
| **ENERGY** | Energy meter readings |
| **RAINGAUGE** | Rain gauge measurements |
| **BOREWELL** | Borewell monitoring |
| **QUALITY** | Water quality (pH, turbidity, DO, conductivity) |

## Supported Data Formats

### 8-bit Formats
- **INT8** - 8-bit Signed (-128 to 127)
- **UINT8** - 8-bit Unsigned (0 to 255)

### 16-bit Formats (1 register)
- **INT16_HI/LO** - 16-bit signed, high/low byte first
- **UINT16_HI/LO** - 16-bit unsigned, high/low byte first

### 32-bit Integer Formats (2 registers)
- **INT32_ABCD** - Big Endian
- **INT32_DCBA** - Little Endian
- **INT32_BADC** - Mid-Big Endian (Word swap)
- **INT32_CDAB** - Mid-Little Endian
- **UINT32_ABCD/DCBA/BADC/CDAB** - Unsigned variants

### 32-bit Float Formats (2 registers)
- **FLOAT32_ABCD/DCBA/BADC/CDAB** - IEEE 754 with all byte orders

### 64-bit Formats (4 registers)
- **INT64_12345678/87654321/21436587/78563412** - All byte orders
- **UINT64_12345678/87654321/21436587/78563412** - Unsigned variants
- **FLOAT64_12345678/87654321/21436587/78563412** - Double precision

### Special Formats
- **ASCII** - ASCII String
- **HEX** - Hexadecimal
- **BOOL** - Boolean
- **PDU** - Protocol Data Unit

## Quick Start

### 1. Hardware Setup
```
ESP32 Connections:
├── GPIO 16 (RX2)  → RS485 RO (Receive)
├── GPIO 17 (TX2)  → RS485 DI (Transmit)
├── GPIO 18 (RTS)  → RS485 DE/RE (Direction)
├── GPIO 23 (MOSI) → SD Card MOSI
├── GPIO 19 (MISO) → SD Card MISO
├── GPIO 5  (CLK)  → SD Card CLK
├── GPIO 15 (CS)   → SD Card CS
└── GND            → Common Ground
```

### 2. Build and Flash
```bash
git clone <repository-url>
cd modbus_iot_gateway
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 3. Configure via Web Interface
1. Connect to WiFi: `FluxGen-Gateway` (password: `fluxgen123`)
2. Open browser: `http://192.168.4.1`
3. Configure WiFi or SIM settings
4. Configure Azure IoT Hub credentials
5. Add sensors with appropriate type selection
6. Test each sensor using Test RS485 feature

## SD Card Offline Caching

### Features
- **Automatic caching** when network is unavailable
- **Chronological replay** - Cached data sent FIRST when network resumes
- **Message validation** - Skips invalid timestamps and placeholder data
- **Batch processing** - 20 messages per replay cycle
- **Auto-recovery** - Remounts SD card on filesystem errors

### Why Offline Data First?
For flow meters and totalizers, data must be sent in chronological order:
1. Cloud uses first received data as reference point
2. If live data sent first, cached data would be "out of order"
3. Historical data would be interpolated incorrectly or wasted

### Sequence When Network Resumes
```
1. Check SD card for pending messages
2. Send ALL cached messages first (oldest to newest)
3. Wait 500ms for processing
4. Send current live telemetry data
```

## Web Interface Features

### Overview Dashboard
- System status and uptime
- Azure IoT Hub connection status
- Modbus communication statistics
- Network signal strength

### Sensor Configuration
- Add/Edit/Delete sensors
- Test RS485 communication
- View all ScadaCore format interpretations
- Real-time value display

### Settings
- WiFi configuration with network scanning
- SIM/4G modem settings
- Azure IoT Hub credentials
- Telemetry interval configuration
- SD Card enable/disable

## Project Structure

```
modbus_iot_gateway/
├── main/
│   ├── main.c              # Main application and task management
│   ├── web_config.c        # Web interface (609KB - handle with care!)
│   ├── web_config.h        # Configuration structures
│   ├── sensor_manager.c    # Sensor data processing
│   ├── modbus.c            # RS485 Modbus RTU implementation
│   ├── sd_card_logger.c    # SD card offline caching
│   ├── a7670c_ppp.c        # SIM module PPP implementation
│   ├── json_templates.c    # Telemetry JSON formatting
│   ├── iot_configs.h       # IoT configuration constants
│   └── gpio_map.h          # GPIO pin definitions
├── CLAUDE.md               # Development guidelines
├── README.md               # This file
└── sdkconfig               # ESP-IDF configuration
```

## Recent Updates (v1.1.0)

### New Sensor Types
- **Panda EMF** - INT32_BE + FLOAT32_BE totalizer format
- **Panda Level** - UINT16 with percentage calculation
- **Clampon** - UINT32_BADC + FLOAT32_BADC format
- **Dailian EMF** - UINT32 word-swapped totalizer

### Bug Fixes
- Fixed Test RS485 display for all flow meter types (was showing incorrect values)
- Fixed ScadaCore table header visibility (missing white text color)
- Fixed success page styling to match main theme
- Fixed buffer overflow in success page

### Improvements
- **Offline data priority** - Cached data sent BEFORE live data when network resumes
- **Memory optimization** - Skip MQTT/telemetry tasks in setup mode
- **PPP recovery** - Better handling of modem reset on reboot
- **TLS certificates** - Added certificate bundle for SIM mode

## Production Deployment

### System Requirements
- **Hardware**: ESP32-WROOM-32 with 4MB flash
- **Power**: 3.3V regulated, 500mA minimum
- **Communication**: RS485 transceiver (MAX485/SP485)
- **Storage**: MicroSD card (FAT32, 2GB-16GB recommended)
- **Network**: WiFi 802.11b/g/n or 4G SIM card

### Deployment Checklist
- [ ] Hardware properly wired and tested
- [ ] RS485 network with proper termination
- [ ] SD card formatted as FAT32 and inserted
- [ ] WiFi or SIM credentials configured
- [ ] Azure IoT Hub device provisioned
- [ ] All sensors configured and tested
- [ ] Offline caching verified

## Troubleshooting

### RS485 Communication Issues
1. Verify baud rate matches sensor (default: 9600)
2. Check GPIO pins: RX=16, TX=17, RTS=18
3. Ensure proper RS485 termination
4. Verify slave ID and register addresses

### SD Card Issues
1. Format as FAT32 (not exFAT)
2. Use 2GB-16GB card (Class 4 or 10)
3. Check wiring: MOSI=23, MISO=19, CLK=5, CS=15
4. Try SanDisk, Samsung, or Kingston brand

### MQTT Connection Issues
1. Verify Azure IoT Hub credentials
2. Check network connectivity
3. Verify DNS resolution for azure-devices.net
4. Check SAS token expiration

## Support

### Technical Support Levels
- **Level 1**: Basic configuration and WiFi setup
- **Level 2**: RS485 communication and sensor integration
- **Level 3**: Custom firmware and enterprise integration

### Resources
- **Web Interface** - Built-in help and diagnostics
- **Serial Monitor** - Detailed debug logging
- **CLAUDE.md** - Development guidelines and known issues

## About Fluxgen

**Fluxgen Industrial IoT Solutions** - Professional industrial automation and IoT connectivity solutions for modern manufacturing and process control environments.

---

**Status: PRODUCTION READY v1.1.0**

*This system is ready for industrial deployment with comprehensive monitoring, offline caching, and professional support capabilities.*
