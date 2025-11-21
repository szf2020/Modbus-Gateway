# Remote Sensor Configuration Format Guide
## Configure Modbus Sensors via Azure IoT Hub C2D Commands

---

## 📋 Overview

You can now **remotely add, update, and delete Modbus sensors** from Azure IoT Hub without accessing the web interface or reflashing firmware!

**Available Commands:**
1. **add_sensor** - Add a new Modbus sensor
2. **update_sensor** - Modify existing sensor settings
3. **delete_sensor** - Remove a sensor

---

## ✅ Command 1: Add New Sensor

### **Format:**

```json
{
  "command": "add_sensor",
  "sensor": {
    "name": "Flow Meter 1",
    "unit_id": "FM001",
    "slave_id": 1,
    "register_address": 8,
    "quantity": 4,
    "data_type": "FLOAT64_12345678",
    "sensor_type": "Panda_USM",
    "scale_factor": 1.0,
    "baud_rate": 9600
  }
}
```

###  **Field Descriptions:**

| Field | Type | Required | Description | Example |
|-------|------|----------|-------------|---------|
| `name` | string | ✅ Yes | Sensor display name (max 31 chars) | "Flow Meter 1" |
| `unit_id` | string | ✅ Yes | Unique identifier (max 15 chars) | "FM001", "TANK1_LEVEL" |
| `slave_id` | integer | ✅ Yes | Modbus slave ID (1-247) | 1, 5, 10 |
| `register_address` | integer | ✅ Yes | Modbus register address | 0, 8, 4121 |
| `quantity` | integer | ✅ Yes | Number of registers to read (1-10) | 1, 2, 4 |
| `data_type` | string | ✅ Yes | Data format (see table below) | "FLOAT32_ABCD", "UINT16" |
| `sensor_type` | string | ✅ Yes | Sensor category (see types below) | "Panda_USM", "Flow-Meter" |
| `scale_factor` | float | ✅ Yes | Multiplier for raw value | 1.0, 0.01, 100.0 |
| `baud_rate` | integer | ✅ Yes | RS485 baud rate (300-115200) | 9600, 19200 |

---

### **Sensor Types:**

| Type | Description | Auto-Configuration |
|------|-------------|-------------------|
| `Panda_USM` | Panda USM flow meter | Quantity=4, Data=DOUBLE64 |
| `Flow-Meter` | Generic flow meter | Quantity=4, UINT32+FLOAT32 |
| `ZEST` | ZEST AquaGen meter | Quantity=4, Custom format |
| `Level` | Water level sensor | Standard Modbus |
| `Radar Level` | Radar level sensor | Standard Modbus |
| `pH` | pH sensor | Standard Modbus |
| `TDS` | Total Dissolved Solids | Standard Modbus |
| `Temp` | Temperature sensor | Standard Modbus |
| `COD` | Chemical Oxygen Demand | Standard Modbus |
| `TSS` | Total Suspended Solids | Standard Modbus |
| `BOD` | Biochemical Oxygen Demand | Standard Modbus |
| `ENERGY` | Energy/Power meter | Standard Modbus |
| `Clampon` | Clamp-on flow meter | Quantity=2, UINT32_3412 |
| `Dailian` | Dailian ultrasonic meter | Quantity=2, UINT32_3412 |
| `Piezometer` | Piezometer (water level) | Quantity=1, UINT16 |

---

### **Data Types:**

#### **16-bit Integer (1 register):**
- `UINT16` - 16-bit unsigned (0 to 65,535)
- `INT16` - 16-bit signed (-32,768 to 32,767)

#### **32-bit Integer (2 registers):**
- `UINT32_ABCD` or `UINT32_1234` - Big-endian
- `UINT32_DCBA` or `UINT32_4321` - Little-endian
- `UINT32_BADC` or `UINT32_2143` - Mid-big-endian
- `UINT32_CDAB` or `UINT32_3412` - Mid-little-endian
- `INT32_ABCD` - Signed big-endian
- `INT32_DCBA` - Signed little-endian
- `INT32_BADC` - Signed mid-big-endian
- `INT32_CDAB` - Signed mid-little-endian

#### **32-bit Float (2 registers):**
- `FLOAT32_ABCD` or `FLOAT32_1234` - Big-endian
- `FLOAT32_DCBA` or `FLOAT32_4321` - Little-endian
- `FLOAT32_BADC` or `FLOAT32_2143` - Mid-big-endian
- `FLOAT32_CDAB` or `FLOAT32_3412` - Mid-little-endian

#### **64-bit Float/Integer (4 registers):**
- `FLOAT64_12345678` - 64-bit double big-endian
- `FLOAT64_87654321` - 64-bit double little-endian
- `INT64_12345678` - 64-bit signed big-endian
- `UINT64_12345678` - 64-bit unsigned big-endian

---

### **Complete Examples:**

#### **Example 1: Panda USM Flow Meter**

```json
{
  "command": "add_sensor",
  "sensor": {
    "name": "Main Line Flow",
    "unit_id": "FLOW_MAIN",
    "slave_id": 1,
    "register_address": 8,
    "quantity": 4,
    "data_type": "FLOAT64_12345678",
    "sensor_type": "Panda_USM",
    "scale_factor": 1.0,
    "baud_rate": 9600
  }
}
```

**Send via Azure CLI:**
```bash
az iot device c2d-message send \
  --hub-name your-hub-name \
  --device-id gateway-01 \
  --data '{
    "command":"add_sensor",
    "sensor":{
      "name":"Main Line Flow",
      "unit_id":"FLOW_MAIN",
      "slave_id":1,
      "register_address":8,
      "quantity":4,
      "data_type":"FLOAT64_12345678",
      "sensor_type":"Panda_USM",
      "scale_factor":1.0,
      "baud_rate":9600
    }
  }'
```

---

#### **Example 2: Simple Temperature Sensor (UINT16)**

```json
{
  "command": "add_sensor",
  "sensor": {
    "name": "Tank Temperature",
    "unit_id": "TEMP_01",
    "slave_id": 2,
    "register_address": 100,
    "quantity": 1,
    "data_type": "UINT16",
    "sensor_type": "Temp",
    "scale_factor": 0.1,
    "baud_rate": 9600
  }
}
```

**Explanation:**
- Reads 1 register from address 100
- Data type: 16-bit unsigned integer
- Scale: 0.1 (e.g., raw value 235 = 23.5°C)
- Baud rate: 9600

---

#### **Example 3: pH Sensor (FLOAT32)**

```json
{
  "command": "add_sensor",
  "sensor": {
    "name": "pH Sensor",
    "unit_id": "PH_TANK1",
    "slave_id": 3,
    "register_address": 50,
    "quantity": 2,
    "data_type": "FLOAT32_ABCD",
    "sensor_type": "pH",
    "scale_factor": 1.0,
    "baud_rate": 9600
  }
}
```

**Explanation:**
- Reads 2 registers (32-bit float)
- Big-endian byte order (ABCD)
- No scaling needed (already pH value)

---

#### **Example 4: Energy Meter (UINT32)**

```json
{
  "command": "add_sensor",
  "sensor": {
    "name": "Power Meter",
    "unit_id": "ENERGY_01",
    "slave_id": 5,
    "register_address": 200,
    "quantity": 2,
    "data_type": "UINT32_ABCD",
    "sensor_type": "ENERGY",
    "scale_factor": 0.01,
    "baud_rate": 19200
  }
}
```

**Explanation:**
- Higher baud rate (19200)
- 32-bit unsigned integer
- Scale: 0.01 (raw 12345 = 123.45 kWh)

---

## 🔄 Command 2: Update Existing Sensor

### **Format:**

```json
{
  "command": "update_sensor",
  "index": 0,
  "updates": {
    "enabled": true,
    "name": "Updated Name",
    "scale_factor": 0.5,
    "baud_rate": 19200
  }
}
```

### **Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `index` | integer | Sensor index (0-19, based on order added) |
| `updates` | object | Fields to update (only include what you want to change) |

### **Updatable Fields:**

- `enabled` - true/false (enable/disable sensor)
- `name` - Change display name
- `scale_factor` - Update scaling multiplier
- `baud_rate` - Change communication speed

**Note:** Cannot change slave_id, register_address, or data_type. Delete and re-add sensor instead.

---

### **Examples:**

#### **Example 1: Disable a Sensor**

```json
{
  "command": "update_sensor",
  "index": 0,
  "updates": {
    "enabled": false
  }
}
```

**CLI:**
```bash
az iot device c2d-message send \
  --hub-name your-hub \
  --device-id gateway-01 \
  --data '{"command":"update_sensor","index":0,"updates":{"enabled":false}}'
```

---

#### **Example 2: Change Scale Factor**

```json
{
  "command": "update_sensor",
  "index": 1,
  "updates": {
    "scale_factor": 0.01
  }
}
```

**Use case:** Sensor readings are 100x too large, apply 0.01 scale factor

---

#### **Example 3: Rename and Change Baud Rate**

```json
{
  "command": "update_sensor",
  "index": 2,
  "updates": {
    "name": "Primary Flow Meter",
    "baud_rate": 19200
  }
}
```

---

## ❌ Command 3: Delete Sensor

### **Format:**

```json
{
  "command": "delete_sensor",
  "index": 2
}
```

### **Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `index` | integer | Sensor index to delete (0-19) |

**Warning:** This permanently removes the sensor configuration. All sensors after this index will shift down by 1.

---

### **Example:**

```bash
# Delete sensor at index 1 (second sensor)
az iot device c2d-message send \
  --hub-name your-hub \
  --device-id gateway-01 \
  --data '{"command":"delete_sensor","index":1}'
```

**ESP32 Log:**
```
I (12345) AZURE_IOT: [C2D] Sensor 1 deleted (remaining: 3)
```

---

## 📊 Finding Sensor Index

Sensors are indexed in the order they were added, starting from 0.

### **Method 1: Check ESP32 Logs**

```
I (12345) AZURE_IOT: [C2D] Sensor added: Flow Meter 1 (total: 1)
```
This is sensor index 0.

### **Method 2: Request Status**

```bash
az iot device c2d-message send \
  --hub-name your-hub \
  --device-id gateway-01 \
  --data '{"command":"get_status"}'
```

Check telemetry JSON - sensors appear in array order:
```json
{
  "sensors": [
    {"unit":"FLOW_01", "value":123.45},  // Index 0
    {"unit":"TEMP_01", "value":25.3},    // Index 1
    {"unit":"PH_01", "value":7.2}        // Index 2
  ]
}
```

---

## 🎯 Common Scenarios

### **Scenario 1: Deploy Multiple Gateways with Same Config**

**Step 1:** Configure one gateway via web interface
**Step 2:** Document the configuration
**Step 3:** Send C2D commands to all other gateways

```python
# Python script to configure multiple gateways
gateways = ["gateway-01", "gateway-02", "gateway-03"]

sensor_config = {
    "command": "add_sensor",
    "sensor": {
        "name": "Flow Meter",
        "unit_id": "FLOW_01",
        "slave_id": 1,
        "register_address": 8,
        "quantity": 4,
        "data_type": "FLOAT64_12345678",
        "sensor_type": "Panda_USM",
        "scale_factor": 1.0,
        "baud_rate": 9600
    }
}

for gateway in gateways:
    send_c2d_message(gateway, sensor_config)
    print(f"✅ Configured {gateway}")
```

---

### **Scenario 2: Quick Testing - Add Sensor Remotely**

You're at the client site with a new sensor. Instead of opening web interface:

```bash
# Add sensor via phone/laptop from anywhere
az iot device c2d-message send \
  --hub-name your-hub \
  --device-id gateway-01 \
  --data '{
    "command":"add_sensor",
    "sensor":{
      "name":"Test Sensor",
      "unit_id":"TEST_01",
      "slave_id":10,
      "register_address":100,
      "quantity":2,
      "data_type":"FLOAT32_ABCD",
      "sensor_type":"Flow-Meter",
      "scale_factor":1.0,
      "baud_rate":9600
    }
  }'

# Verify immediately
az iot device c2d-message send \
  --hub-name your-hub \
  --device-id gateway-01 \
  --data '{"command":"get_status"}'
```

---

### **Scenario 3: Maintenance - Temporarily Disable Faulty Sensor**

```bash
# Disable sensor without deleting configuration
az iot device c2d-message send \
  --hub-name your-hub \
  --device-id gateway-01 \
  --data '{"command":"update_sensor","index":2,"updates":{"enabled":false}}'

# After repair, re-enable
az iot device c2d-message send \
  --hub-name your-hub \
  --device-id gateway-01 \
  --data '{"command":"update_sensor","index":2,"updates":{"enabled":true}}'
```

---

## ⚠️ Important Notes

### **Limitations:**

1. **Maximum 20 sensors** per gateway
2. **Configuration persists** - Saved to flash, survives reboots
3. **Index changes** - When you delete a sensor, all sensors after it shift down by 1
4. **Restart recommended** - After adding/updating sensors, consider sending restart command for clean initialization

### **Best Practices:**

1. ✅ **Document sensor indices** - Keep track of which index corresponds to which sensor
2. ✅ **Test on one gateway first** - Verify configuration before deploying to multiple gateways
3. ✅ **Use descriptive names** - Makes troubleshooting easier
4. ✅ **Use unique unit_ids** - Helps identify sensors in telemetry
5. ✅ **Verify after changes** - Send `get_status` command to confirm readings

### **Validation:**

The gateway validates:
- ✅ Slave ID (1-247)
- ✅ Register address (0-65535)
- ✅ Quantity (1-10)
- ✅ Scale factor (any float)
- ✅ Baud rate (standard values)
- ✅ Maximum sensor limit (20)

Invalid values will be logged and command rejected.

---

## 🔧 Troubleshooting

### **Issue 1: Sensor Not Added**

**Check ESP32 Logs:**
```
W (12345) AZURE_IOT: [C2D] Cannot add sensor - limit reached (20 max)
```

**Solution:** Delete unused sensors first

---

### **Issue 2: Wrong Index**

**Log:**
```
W (12345) AZURE_IOT: [C2D] Invalid sensor index: 5
```

**Solution:** Check current sensor count with `get_status`

---

### **Issue 3: Sensor Readings Wrong**

**Possible causes:**
- Wrong data_type (try different byte orders)
- Wrong scale_factor
- Wrong register_address
- Wrong quantity

**Solution:** Use web interface Modbus Explorer to test different formats first

---

## 📚 Quick Reference

### **Add Sensor Template:**

```json
{
  "command": "add_sensor",
  "sensor": {
    "name": "SENSOR_NAME",
    "unit_id": "UNIT_ID",
    "slave_id": 1,
    "register_address": 0,
    "quantity": 2,
    "data_type": "FLOAT32_ABCD",
    "sensor_type": "Flow-Meter",
    "scale_factor": 1.0,
    "baud_rate": 9600
  }
}
```

### **Update Sensor Template:**

```json
{
  "command": "update_sensor",
  "index": 0,
  "updates": {
    "enabled": true
  }
}
```

### **Delete Sensor Template:**

```json
{
  "command": "delete_sensor",
  "index": 0
}
```

---

## 🎓 Complete Workflow Example

```bash
# 1. Add first sensor
az iot device c2d-message send --hub-name your-hub --device-id gateway-01 \
  --data '{"command":"add_sensor","sensor":{"name":"Flow 1","unit_id":"FLOW_01","slave_id":1,"register_address":8,"quantity":4,"data_type":"FLOAT64_12345678","sensor_type":"Panda_USM","scale_factor":1.0,"baud_rate":9600}}'

# 2. Add second sensor
az iot device c2d-message send --hub-name your-hub --device-id gateway-01 \
  --data '{"command":"add_sensor","sensor":{"name":"Temp 1","unit_id":"TEMP_01","slave_id":2,"register_address":100,"quantity":1,"data_type":"UINT16","sensor_type":"Temp","scale_factor":0.1,"baud_rate":9600}}'

# 3. Test readings
az iot device c2d-message send --hub-name your-hub --device-id gateway-01 \
  --data '{"command":"get_status"}'

# 4. Update scale factor if needed
az iot device c2d-message send --hub-name your-hub --device-id gateway-01 \
  --data '{"command":"update_sensor","index":1,"updates":{"scale_factor":0.01}}'

# 5. Restart for clean initialization
az iot device c2d-message send --hub-name your-hub --device-id gateway-01 \
  --data '{"command":"restart"}'
```

---

**Last Updated**: January 2025
**Gateway Version**: v1.1.0
**Max Sensors**: 20 per gateway
