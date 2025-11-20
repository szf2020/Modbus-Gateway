// web_config.h - Web-based configuration interface for multi-sensor Modbus system

#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include "esp_err.h"
#include "esp_http_server.h"
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/uart.h"

// Configuration states
typedef enum {
    CONFIG_STATE_SETUP,      // Initial setup mode (STA for configuration)
    CONFIG_STATE_OPERATION   // Normal operation mode (AP mode)
} config_state_t;

// Network mode selection
typedef enum {
    NETWORK_MODE_WIFI,       // Use WiFi connectivity (default)
    NETWORK_MODE_SIM         // Use SIM module (A7670C) connectivity
} network_mode_t;

// Sub-sensor for water quality parameters
typedef struct {
    bool enabled;
    char parameter_name[32];   // pH, Temperature, TDS, etc.
    char json_key[16];         // JSON field name: "pH", "temp", "tds", etc.
    int slave_id;
    int register_address;
    int quantity;
    char data_type[32];        // INT32, UINT16, FLOAT32, FLOAT64_78563412, etc. - increased for 64-bit formats
    char register_type[16];    // HOLDING, INPUT
    float scale_factor;
    char byte_order[16];       // BIG_ENDIAN, LITTLE_ENDIAN, etc.
    char units[16];            // pH, degC, ppm, etc.
} sub_sensor_t;

// Sensor configuration structure
typedef struct {
    bool enabled;
    char name[32];
    char unit_id[16];
    int slave_id;
    int baud_rate;
    char parity[8];            // none, even, odd
    int register_address;
    int quantity;
    char data_type[32];        // INT32, UINT16, FLOAT32, FLOAT64_78563412, etc. - increased for 64-bit formats
    char register_type[16];    // HOLDING, INPUT
    float scale_factor;
    char byte_order[16];       // BIG_ENDIAN, LITTLE_ENDIAN, etc.
    char description[64];
    
    // Sensor type and Level-specific fields
    char sensor_type[16];      // "Flow-Meter", "Level", "ENERGY", "QUALITY", etc.
    float sensor_height;       // For Level sensors: physical sensor height
    float max_water_level;     // For Level sensors: maximum water level for calculation
    char meter_type[32];       // For ENERGY sensors: meter type identifier
    
    // Sub-sensors for water quality sensors only
    sub_sensor_t sub_sensors[8]; // Up to 8 sub-sensors per water quality sensor
    int sub_sensor_count;
} sensor_config_t;

// SIM module configuration (A7670C)
typedef struct {
    bool enabled;              // Enable/disable SIM module
    char apn[64];             // APN for cellular carrier (e.g., "airteliot")
    char apn_user[32];        // APN username (usually empty)
    char apn_pass[32];        // APN password (usually empty)
    gpio_num_t uart_tx_pin;   // UART TX pin - ESP32 TX -> A7670C RX (default: GPIO 33)
    gpio_num_t uart_rx_pin;   // UART RX pin - ESP32 RX <- A7670C TX (default: GPIO 32)
    gpio_num_t pwr_pin;       // Power control pin (default: GPIO 4)
    gpio_num_t reset_pin;     // Reset pin (default: GPIO 15)
    uart_port_t uart_num;     // UART port number (default: UART_NUM_1)
    int uart_baud_rate;       // Baud rate (default: 115200)
} sim_module_config_t;

// SD card configuration
typedef struct {
    bool enabled;              // Enable/disable SD card logging
    bool cache_on_failure;     // Cache data to SD when network fails
    gpio_num_t mosi_pin;       // SPI MOSI pin (default: GPIO 13)
    gpio_num_t miso_pin;       // SPI MISO pin (default: GPIO 12)
    gpio_num_t clk_pin;        // SPI CLK pin (default: GPIO 14)
    gpio_num_t cs_pin;         // SPI CS pin (default: GPIO 5)
    int spi_host;              // SPI host (default: SPI2_HOST)
    int max_message_size;      // Maximum message size (default: 512)
    int min_free_space_mb;     // Minimum free space in MB (default: 1)
} sd_card_config_t;

// RTC configuration
typedef struct {
    bool enabled;              // Enable/disable RTC
    gpio_num_t sda_pin;       // I2C SDA pin (default: GPIO 21)
    gpio_num_t scl_pin;       // I2C SCL pin (default: GPIO 22)
    int i2c_num;              // I2C port number (default: I2C_NUM_0)
    bool sync_on_boot;        // Sync RTC from NTP on boot (if network available)
    bool update_from_ntp;     // Periodically update RTC from NTP
} rtc_config_t;

// System configuration
typedef struct {
    // Network configuration
    network_mode_t network_mode;  // WiFi or SIM mode
    char wifi_ssid[32];
    char wifi_password[64];
    sim_module_config_t sim_config;

    // Cloud configuration
    char azure_hub_fqdn[128];
    char azure_device_id[32];
    char azure_device_key[128];
    int telemetry_interval;    // in seconds

    // Sensor configuration
    sensor_config_t sensors[20]; // Support up to 20 individual sensors
    int sensor_count;

    // Optional features
    sd_card_config_t sd_config;
    rtc_config_t rtc_config;

    // System flags
    bool config_complete;
    bool modem_reset_enabled;  // Enable/disable modem reset on MQTT disconnect
    int modem_boot_delay;      // Delay in seconds to wait for modem boot after reset
    int modem_reset_gpio_pin;  // GPIO pin for modem reset control
    int trigger_gpio_pin;      // GPIO pin for configuration mode trigger (default: 34)
} system_config_t;

// Function prototypes
esp_err_t web_config_init(void);
esp_err_t web_config_start_sta_mode(void);
esp_err_t web_config_start_ap_mode(void);
esp_err_t web_config_stop(void);
esp_err_t web_config_start_server_only(void);

// Configuration management
esp_err_t config_load_from_nvs(system_config_t *config);
esp_err_t config_save_to_nvs(const system_config_t *config);
esp_err_t config_reset_to_defaults(void);

// Sensor testing
esp_err_t test_sensor_connection(const sensor_config_t *sensor, char *result_buffer, size_t buffer_size);

// Sub-sensor management for water quality sensors
esp_err_t add_sub_sensor_to_quality_sensor(int sensor_index, const sub_sensor_t *sub_sensor);
esp_err_t delete_sub_sensor_from_quality_sensor(int sensor_index, int sub_sensor_index);

// Get current configuration
system_config_t* get_system_config(void);
config_state_t get_config_state(void);
void set_config_state(config_state_t state);
bool web_config_needs_auto_start(void);

// Modem GPIO control
esp_err_t update_modem_gpio_pin(int new_gpio_pin);

// WiFi network connection
esp_err_t connect_to_wifi_network(void);

#endif // WEB_CONFIG_H