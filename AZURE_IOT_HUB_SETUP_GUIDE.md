# Azure IoT Hub Setup Guide
## Complete Guide for ESP32 Modbus IoT Gateway

---

## 📋 Table of Contents
1. [What is Azure IoT Hub?](#what-is-azure-iot-hub)
2. [Why Use Azure IoT Hub?](#why-use-azure-iot-hub)
3. [Architecture Overview](#architecture-overview)
4. [Step-by-Step Setup](#step-by-step-setup)
5. [Gateway Configuration](#gateway-configuration)
6. [Monitoring & Visualization](#monitoring--visualization)
7. [Advanced Features](#advanced-features)
8. [Troubleshooting](#troubleshooting)

---

## 🌐 What is Azure IoT Hub?

**Azure IoT Hub** is Microsoft's cloud platform for IoT (Internet of Things) device management and data collection. It acts as a central message hub for bi-directional communication between your IoT devices (ESP32 gateway) and the cloud.

### Key Capabilities:
- **Device-to-Cloud (D2C) Telemetry**: Send sensor data from ESP32 → Azure Cloud
- **Cloud-to-Device (C2D) Messages**: Send commands from Cloud → ESP32
- **Device Management**: Monitor device health, update firmware remotely
- **Security**: Industry-standard encryption (TLS 1.2, SAS tokens)
- **Scalability**: Handle millions of devices and billions of messages

---

## 🎯 Why Use Azure IoT Hub?

### Benefits for Your Modbus Gateway:

1. **Centralized Data Collection**
   - All sensor data from multiple gateways → One cloud location
   - Historical data storage for years
   - Real-time monitoring from anywhere

2. **Advanced Analytics**
   - Azure Stream Analytics: Real-time data processing
   - Azure Time Series Insights: Visualize trends
   - Power BI: Create professional dashboards
   - Machine Learning: Predictive maintenance

3. **Enterprise Features**
   - Multi-user access control
   - API integration with other systems
   - Mobile app development (iOS/Android)
   - Email/SMS alerts

4. **Reliability**
   - 99.9% uptime SLA
   - Automatic scaling
   - Built-in redundancy
   - Professional support

### Use Cases:
- **Industrial Monitoring**: Water treatment plants, factories
- **Smart Cities**: Water distribution, energy management
- **Agriculture**: Irrigation, soil monitoring
- **Building Management**: HVAC, energy consumption

---

## 🏗️ Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     YOUR ESP32 GATEWAY                      │
│  ┌────────────┐    ┌──────────────┐    ┌───────────────┐  │
│  │ Modbus     │───>│ ESP32        │───>│ WiFi/4G       │  │
│  │ Sensors    │    │ (Your Code)  │    │ Connection    │  │
│  │ (RS485)    │    │              │    │               │  │
│  └────────────┘    └──────────────┘    └───────┬───────┘  │
└────────────────────────────────────────────────┼───────────┘
                                                  │
                                      MQTT over TLS (Secure)
                                                  │
                                                  ▼
┌─────────────────────────────────────────────────────────────┐
│                    AZURE IOT HUB                            │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Device Registry: Stores device credentials         │   │
│  │  Message Routing: Routes data to storage/analytics  │   │
│  │  Security: Authentication, encryption               │   │
│  └─────────────────────────────────────────────────────┘   │
└────────────┬──────────────────┬─────────────────┬──────────┘
             │                  │                 │
             ▼                  ▼                 ▼
    ┌────────────────┐  ┌──────────────┐  ┌─────────────┐
    │ Azure Storage  │  │ Stream       │  │ Power BI    │
    │ (Historical)   │  │ Analytics    │  │ (Dashboard) │
    └────────────────┘  └──────────────┘  └─────────────┘
```

---

## 🚀 Step-by-Step Setup

### Phase 1: Create Azure Account (If you don't have one)

1. **Go to Azure Portal**
   - Visit: https://portal.azure.com
   - Click "Start Free" (₹13,300 free credit for 30 days)

2. **Sign Up**
   - Use your email (personal or work)
   - Provide payment details (required, but won't charge during free trial)
   - Verify phone number

3. **Free Tier Benefits**
   - 8,000 messages/day FREE forever
   - Additional messages: Very low cost (₹0.33 per 1000 messages)

---

### Phase 2: Create IoT Hub

1. **Login to Azure Portal**
   - https://portal.azure.com

2. **Create IoT Hub**
   ```
   1. Search "IoT Hub" in search bar
   2. Click "Create IoT Hub"
   3. Fill in details:
      - Subscription: Your subscription name
      - Resource Group: Create new → "ModbusGatewayGroup"
      - Region: "Central India" (or closest to you)
      - IoT Hub Name: "fluxgen-modbus-hub" (must be globally unique)
      - Pricing Tier: "F1 - Free" (for testing) or "S1 - Standard"
   4. Click "Review + Create"
   5. Click "Create"
   6. Wait 2-3 minutes for deployment
   ```

3. **Note Your IoT Hub Details**
   - IoT Hub Name: `fluxgen-modbus-hub`
   - IoT Hub Hostname: `fluxgen-modbus-hub.azure-devices.net`

---

### Phase 3: Register Your ESP32 Device

1. **Open Your IoT Hub**
   - Go to portal.azure.com
   - Navigate to your IoT Hub

2. **Add New Device**
   ```
   1. In left menu, click "Devices"
   2. Click "+ Add Device"
   3. Fill in:
      - Device ID: "gateway-01" (or any unique name)
      - Authentication type: "Symmetric key"
      - Auto-generate keys: YES (checked)
   4. Click "Save"
   ```

3. **Get Device Credentials**
   ```
   1. Click on your device name "gateway-01"
   2. You'll see two important values:
      - Primary Key: (long base64 string, 44 characters)
      - Connection String: (contains everything)

   3. Copy and save:
      ✅ Device ID: gateway-01
      ✅ Primary Key: Example: wR8dGHnJk5+abcdefghijklmnopqrstuvwxyz1234567890=
   ```

**IMPORTANT**: Keep Primary Key secret! Anyone with this key can send data as your device.

---

### Phase 4: Update Your Code (Already Done!)

Your ESP32 code already has Azure IoT Hub support! Just need to update one file:

**File**: `main/iot_configs.h`

```c
// Current setting (line 9):
#define IOT_CONFIG_IOTHUB_FQDN "fluxgen-testhub.azure-devices.net"

// Change to YOUR IoT Hub hostname:
#define IOT_CONFIG_IOTHUB_FQDN "fluxgen-modbus-hub.azure-devices.net"
//                              ^^^^^^^^^^^^^^^^
//                              Your IoT Hub name from Azure portal
```

**Then rebuild and flash:**
```bash
idf.py build
idf.py -p COM3 flash
```

---

### Phase 5: Configure Via Web Interface

1. **Connect to ESP32 WiFi**
   - SSID: `ModbusIoT_Config` (or your configured AP name)
   - Password: (your configured password)
   - Open browser: http://192.168.4.1

2. **Configure Network**
   - Go to WiFi/SIM configuration
   - Enter your WiFi credentials
   - Save and connect

3. **Configure Azure Settings**
   ```
   1. Click "Azure IoT Hub" in menu
   2. Enter password: "admin" (default, can be changed in code)
   3. Fill in:
      - IoT Hub FQDN: fluxgen-modbus-hub.azure-devices.net (read-only)
      - Device ID: gateway-01
      - Device Key: (paste your Primary Key from Azure portal)
      - Telemetry Interval: 60 (send data every 60 seconds)
   4. Click "Save Azure Configuration"
   5. Gateway will restart and connect to Azure
   ```

4. **Add Sensors**
   - Go to "Sensor Configuration"
   - Add your Modbus sensors
   - Configure slave IDs, registers, data types

---

## 📊 Monitoring & Visualization

### Method 1: Azure Portal - Real-time Monitoring

**Using Built-in Monitoring Tool:**

1. **Azure Portal → Your IoT Hub → Overview**
   - See message count graph
   - Device connection status

2. **Monitor Messages in Real-Time:**
   ```bash
   # Install Azure CLI (one-time)
   # Windows: Download from https://aka.ms/installazurecliwindows
   # Linux: curl -sL https://aka.ms/InstallAzureCLIDeb | sudo bash

   # Login
   az login

   # Monitor device messages
   az iot hub monitor-events --hub-name fluxgen-modbus-hub --device-id gateway-01

   # You'll see real-time JSON data like:
   {
     "event": {
       "origin": "gateway-01",
       "payload": {
         "sensors": [
           {
             "unit": "PANDA_USM_01",
             "name": "Flow Meter 1",
             "value": 1513.533474,
             "timestamp": "2025-01-21T10:30:45Z"
           }
         ]
       }
     }
   }
   ```

---

### Method 2: Azure Storage - Historical Data

**Store Data for Analysis:**

1. **Create Storage Account**
   ```
   Azure Portal → Storage Accounts → Create
   - Name: modbusgatewaydata
   - Region: Same as IoT Hub
   - Create
   ```

2. **Route Messages to Storage**
   ```
   IoT Hub → Message Routing → Add Route
   - Name: StorageSensorData
   - Endpoint: Create new → Azure Storage
   - Container: sensordata
   - Save
   ```

3. **Access Historical Data**
   - Download as CSV/JSON from Azure Storage Explorer
   - Query with SQL-like syntax

---

### Method 3: Power BI - Professional Dashboards

**Create Beautiful Dashboards:**

1. **Setup Stream Analytics**
   ```
   Azure Portal → Stream Analytics Jobs → Create
   - Job name: SensorDataProcessor
   - Input: Your IoT Hub
   - Output: Power BI
   - Query:
     SELECT
         udf.GetArrayElement(sensors, 0).unit as SensorName,
         udf.GetArrayElement(sensors, 0).value as SensorValue,
         System.Timestamp as EventTime
     INTO PowerBIOutput
     FROM IoTHubInput
   ```

2. **Power BI Dashboard**
   - Login to powerbi.com
   - Create dataset from Stream Analytics output
   - Create visualizations:
     - Line chart: Sensor value over time
     - Gauge: Current value
     - Card: Latest reading
     - Map: Device locations

---

### Method 4: Mobile App - Azure IoT Central

**No-Code Option:**

1. **Create IoT Central App**
   - Visit: https://apps.azureiotcentral.com
   - Create new app (free for 2 devices)
   - Choose "Custom app" template

2. **Connect Your Device**
   - Device Templates → Create new
   - Define your sensor telemetry schema
   - Connect your ESP32 gateway

3. **Benefits**:
   - Pre-built mobile app (iOS/Android)
   - Automatic dashboards
   - Rules & alerts
   - No coding required!

---

## 🔥 Advanced Features

### 1. Cloud-to-Device (C2D) Commands

**Already implemented in your code!** Send commands from cloud to ESP32:

```bash
# Example: Send command to restart gateway
az iot device c2d-message send \
  --hub-name fluxgen-modbus-hub \
  --device-id gateway-01 \
  --data '{"command":"restart"}'
```

Your ESP32 will receive it and can act on it.

---

### 2. Device Twin - Configuration Management

**Store device configuration in cloud:**

```json
{
  "desired": {
    "telemetryInterval": 60,
    "sensorPollingRate": 5,
    "loggingLevel": "INFO"
  }
}
```

ESP32 can read these and update behavior without reflashing!

---

### 3. Firmware Over-The-Air (OTA) Updates

**Update ESP32 firmware remotely:**

1. Upload new firmware to Azure Blob Storage
2. Trigger update via Device Twin
3. ESP32 downloads and installs
4. No physical access needed!

---

### 4. Alerts & Notifications

**Setup Azure Logic Apps:**

```
If sensor value > threshold:
  → Send email to admin@company.com
  → Send SMS to +91-9876543210
  → Create ticket in ServiceNow
  → Post to Slack/Teams
```

---

## 🛠️ Troubleshooting

### Common Issues:

#### Issue 1: "Cannot connect to Azure IoT Hub"

**Check:**
1. ✅ Device ID matches exactly (case-sensitive)
2. ✅ Primary Key copied correctly (no extra spaces)
3. ✅ IoT Hub FQDN correct in `iot_configs.h`
4. ✅ ESP32 has internet connection (ping google.com)
5. ✅ Firewall allows MQTT port 8883

**Logs to check:**
```
I (12345) AZURE_IOT: [DYNAMIC CONFIG] Device ID: gateway-01
I (12346) AZURE_IOT: [DYNAMIC CONFIG] Device Key Length: 44
I (12347) AZURE_IOT: [OK] Azure IoT Hub DNS resolution successful
```

---

#### Issue 2: "Connected but no data visible"

**Verify:**
```bash
# Terminal 1: Monitor Azure
az iot hub monitor-events --hub-name fluxgen-modbus-hub

# Terminal 2: Check ESP32 logs
idf.py monitor

# Look for:
I (45678) AZURE_IOT: [SEND] Published to Azure IoT Hub
```

---

#### Issue 3: "Too many messages - quota exceeded"

**Free tier = 8,000 messages/day**

If you have:
- 10 sensors
- Sending every 60 seconds
- = 10 sensors × 1,440 readings/day = 14,400 messages/day ❌ (exceeds free tier)

**Solution:**
- Increase interval to 120 seconds (7,200 messages/day ✅)
- Or upgrade to S1 tier (400,000 messages/day)

---

## 💰 Pricing (India Region)

### Free Tier (F1)
- **Cost**: ₹0/month
- **Messages**: 8,000/day (240,000/month)
- **Devices**: Unlimited
- **Best for**: Testing, small deployments

### Standard Tier (S1)
- **Cost**: ₹3,279/month + ₹0.33/1000 messages
- **Messages**: 400,000/day included, then pay-as-you-go
- **Devices**: Unlimited
- **Best for**: Production, multiple gateways

**Example calculation:**
- 5 gateways × 10 sensors × 60 sec interval
- = 50 readings/minute × 1,440 min/day = 72,000 messages/day
- Free tier: ❌ Exceeds limit
- S1 tier: ₹3,279/month ✅ (within 400K limit)

---

## 📚 Additional Resources

### Official Documentation:
- Azure IoT Hub Docs: https://docs.microsoft.com/azure/iot-hub/
- ESP32 Azure SDK: https://github.com/Azure/azure-iot-sdk-c
- MQTT Protocol: https://mqtt.org/

### Learning:
- Microsoft Learn (Free courses): https://learn.microsoft.com/training/azure/
- YouTube: "Azure IoT Hub Tutorial" by Microsoft

### Tools:
- Azure IoT Explorer: Desktop app for testing
- VS Code Azure IoT Tools: Extension for development
- Postman: Test REST APIs

---

## 🎓 Quick Start Checklist

- [ ] Create Azure account
- [ ] Create IoT Hub
- [ ] Register device and get credentials
- [ ] Update `iot_configs.h` with your hub name
- [ ] Flash ESP32
- [ ] Configure via web interface (Device ID + Key)
- [ ] Add Modbus sensors
- [ ] Monitor messages in Azure portal
- [ ] Setup Power BI dashboard (optional)
- [ ] Configure alerts (optional)

---

## 📞 Support

**Need help?**
- Azure Support: https://azure.microsoft.com/support/
- Community Forum: https://learn.microsoft.com/answers/
- Your Gateway Docs: `CLAUDE.md` in project root

---

**Last Updated**: January 2025
**Gateway Version**: v1.1.0
**Tested with**: Azure IoT Hub (Free & S1 tiers)
