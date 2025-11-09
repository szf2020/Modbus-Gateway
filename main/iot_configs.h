#ifndef IOT_CONFIGS_H
#define IOT_CONFIGS_H

// WiFi Configuration (Replace with your credentials)
#define IOT_CONFIG_WIFI_SSID "YOUR_WIFI_SSID"
#define IOT_CONFIG_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Azure IoT Hub Configuration (Replace with your Azure IoT Hub credentials)
#define IOT_CONFIG_IOTHUB_FQDN "fluxgen-testhub.azure-devices.net"
#define IOT_CONFIG_DEVICE_ID "your-device-id"
#define IOT_CONFIG_DEVICE_KEY "YOUR_DEVICE_KEY_HERE"

// Telemetry Configuration
#define TELEMETRY_FREQUENCY_MILLISECS 300000  // 5 minutes (300 seconds)

// Production Configuration
#define MAX_MQTT_RECONNECT_ATTEMPTS 5
#define MAX_MODBUS_READ_FAILURES 10
#define SYSTEM_RESTART_ON_CRITICAL_ERROR true


#endif // IOT_CONFIGS_H