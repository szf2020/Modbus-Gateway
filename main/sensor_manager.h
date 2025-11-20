// sensor_manager.h - Multi-sensor management and testing

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "web_config.h"
#include "esp_err.h"

// Sensor test result
typedef struct {
    bool success;
    uint32_t raw_value;
    double scaled_value;
    char error_message[128];
    char raw_hex[32];
    uint32_t response_time_ms;
} sensor_test_result_t;

// Water quality parameters structure
typedef struct {
    double ph_value;        // pH level
    double tds_value;       // Total Dissolved Solids (ppm)
    double temp_value;      // Temperature (degC)
    double humidity_value;  // Humidity (%)
    double tss_value;       // Total Suspended Solids (mg/L)
    double bod_value;       // Biochemical Oxygen Demand (mg/L)
    double cod_value;       // Chemical Oxygen Demand (mg/L)
} quality_params_t;

// Multi-sensor reading result
typedef struct {
    char unit_id[16];
    char sensor_name[32];
    double value;
    bool valid;
    char timestamp[32];
    char data_source[16];
    char raw_hex[32];       // Raw hex string from modbus read
    uint32_t raw_value;     // Raw numeric value
    quality_params_t quality_params; // Water quality parameters (for QUALITY sensors)
} sensor_reading_t;

// Function prototypes
esp_err_t sensor_manager_init(void);
esp_err_t sensor_test_live(const sensor_config_t *sensor, sensor_test_result_t *result);
esp_err_t sensor_read_all_configured(sensor_reading_t *readings, int max_readings, int *actual_count);
esp_err_t sensor_read_single(const sensor_config_t *sensor, sensor_reading_t *reading);
esp_err_t sensor_read_quality(const sensor_config_t *sensor, sensor_reading_t *reading);

// Utility functions
const char* get_register_type_description(const char* reg_type);
const char* get_data_type_description(const char* data_type);
const char* get_byte_order_description(const char* byte_order);

// Data format conversion
esp_err_t convert_modbus_data(uint16_t *registers, int reg_count, 
                             const char* data_type, const char* byte_order,
                             double scale_factor, double *result, uint32_t *raw_value);

#endif // SENSOR_MANAGER_H