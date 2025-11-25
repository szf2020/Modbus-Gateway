// json_templates.c - JSON template implementation for different sensor types

#include "json_templates.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <inttypes.h>

static const char *TAG = "JSON_TEMPLATES";

// Map sensor type string to JSON template type
json_template_type_t get_json_type_from_sensor_type(const char* sensor_type)
{
    if (!sensor_type) return JSON_TYPE_UNKNOWN;
    
    if (strcasecmp(sensor_type, "FLOW") == 0 || strcasecmp(sensor_type, "Flow-Meter") == 0 || strcasecmp(sensor_type, "ZEST") == 0) {
        return JSON_TYPE_FLOW;
    } else if (strcasecmp(sensor_type, "LEVEL") == 0 || strcasecmp(sensor_type, "Level") == 0) {
        return JSON_TYPE_LEVEL;
    } else if (strcasecmp(sensor_type, "Radar Level") == 0) {
        return JSON_TYPE_LEVEL;  // Uses same JSON format as regular Level sensors
    } else if (strcasecmp(sensor_type, "RAINGAUGE") == 0) {
        return JSON_TYPE_RAINGAUGE;
    } else if (strcasecmp(sensor_type, "BOREWELL") == 0) {
        return JSON_TYPE_BOREWELL;
    } else if (strcasecmp(sensor_type, "ENERGY") == 0) {
        return JSON_TYPE_ENERGY;
    } else if (strcasecmp(sensor_type, "QUALITY") == 0) {
        return JSON_TYPE_QUALITY;
    }
    
    return JSON_TYPE_UNKNOWN;
}

// Get human-readable name for JSON template type
const char* get_json_template_name(json_template_type_t type)
{
    switch (type) {
        case JSON_TYPE_FLOW:      return "FLOW";
        case JSON_TYPE_LEVEL:     return "LEVEL";
        case JSON_TYPE_RAINGAUGE: return "RAINGAUGE";
        case JSON_TYPE_BOREWELL:  return "BOREWELL";
        case JSON_TYPE_ENERGY:    return "ENERGY";
        case JSON_TYPE_QUALITY:   return "QUALITY";
        default:                  return "UNKNOWN";
    }
}

// Format timestamp in ISO8601 format
void format_timestamp_iso8601(char* timestamp, size_t size)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    gmtime_r(&now, &timeinfo);
    strftime(timestamp, size, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
}

// Format timestamp as epoch time
void format_timestamp_epoch(uint32_t* epoch_time)
{
    time_t now;
    time(&now);
    *epoch_time = (uint32_t)now;
}

// Validate JSON parameters
esp_err_t validate_json_params(const json_params_t* params)
{
    if (!params) {
        ESP_LOGE(TAG, "JSON parameters are NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(params->unit_id) == 0) {
        ESP_LOGE(TAG, "Unit ID is empty");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (params->type == JSON_TYPE_UNKNOWN) {
        ESP_LOGE(TAG, "Unknown JSON template type");
        return ESP_ERR_INVALID_ARG;
    }
    
    return ESP_OK;
}

// Create JSON payload based on template type and parameters
esp_err_t create_json_payload(const json_params_t* params, char* json_buffer, size_t buffer_size)
{
    if (!params || !json_buffer || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = validate_json_params(params);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Clear buffer
    memset(json_buffer, 0, buffer_size);
    
    ESP_LOGI(TAG, "Creating JSON for type: %s, Unit: %s, Value: %.6f", 
             get_json_template_name(params->type), params->unit_id, params->scaled_value);
    
    switch (params->type) {
        case JSON_TYPE_FLOW: {
            // {"unit_id":"FG24708F","type":"FLOW","consumption":"265.23","created_on":"2025-11-24T12:05:05Z"}
            snprintf(json_buffer, buffer_size,
                "{"
                "\"unit_id\":\"%s\","
                "\"type\":\"FLOW\","
                "\"consumption\":\"%.2f\","
                "\"created_on\":\"%s\""
                "}",
                params->unit_id,
                params->scaled_value,
                params->timestamp);
            break;
        }
        
        case JSON_TYPE_LEVEL: {
            // {"unit_id":"FG24769L","created_on":"2025-11-24T12:04:16Z","type":"LEVEL","level_filled":49}
            snprintf(json_buffer, buffer_size,
                "{"
                "\"unit_id\":\"%s\","
                "\"created_on\":\"%s\","
                "\"type\":\"LEVEL\","
                "\"level_filled\":%.0f"
                "}",
                params->unit_id,
                params->timestamp,
                params->scaled_value);
            break;
        }
        
        case JSON_TYPE_RAINGAUGE: {
            // {"unit_id":"FG24769R","created_on":"2025-11-24T12:04:16Z","type":"RAINGAUGE","raingauge":"123.45"}
            snprintf(json_buffer, buffer_size,
                "{"
                "\"unit_id\":\"%s\","
                "\"created_on\":\"%s\","
                "\"type\":\"RAINGAUGE\","
                "\"raingauge\":\"%.2f\""
                "}",
                params->unit_id,
                params->timestamp,
                params->scaled_value);
            break;
        }
        
        case JSON_TYPE_BOREWELL: {
            // {"borewell":24.196835,"type":"BOREWELL","created_on_epoch":1763986189,"slave_id":1,"meter":"piezo"}
            uint32_t epoch_time;
            format_timestamp_epoch(&epoch_time);

            snprintf(json_buffer, buffer_size,
                "{"
                "\"borewell\":%.6f,"
                "\"type\":\"BOREWELL\","
                "\"created_on_epoch\":%" PRIu32 ","
                "\"slave_id\":%d,"
                "\"meter\":\"%s\""
                "}",
                params->scaled_value,
                epoch_time,
                params->slave_id,
                strlen(params->extra_params.meter_id) > 0 ? params->extra_params.meter_id : "piezo");
            break;
        }
        
        case JSON_TYPE_ENERGY: {
            // {"ene_con_hex":"00004351","type":"ENERGY","created_on_epoch":1702213256,"slave_id":1,"meter":"abcdlong"}
            uint32_t epoch_time;
            format_timestamp_epoch(&epoch_time);

            // Use hex string from Test RS485 if available, otherwise format raw value
            char hex_value[32];
            if (strlen(params->extra_params.hex_string) > 0) {
                // Remove spaces from hex string if present
                const char* src = params->extra_params.hex_string;
                char* dst = hex_value;
                while (*src && dst < hex_value + sizeof(hex_value) - 1) {
                    if (*src != ' ') {
                        *dst++ = *src;
                    }
                    src++;
                }
                *dst = '\0';
            } else {
                // Fallback: format raw value as hex string
                snprintf(hex_value, sizeof(hex_value), "%08" PRIX32, params->raw_value);
            }

            snprintf(json_buffer, buffer_size,
                "{"
                "\"ene_con_hex\":\"%s\","
                "\"type\":\"ENERGY\","
                "\"created_on_epoch\":%" PRIu32 ","
                "\"slave_id\":%d,"
                "\"meter\":\"%s\""
                "}",
                hex_value,
                epoch_time,
                params->slave_id,
                strlen(params->extra_params.meter_id) > 0 ? params->extra_params.meter_id : params->unit_id);
            break;
        }
        
        case JSON_TYPE_QUALITY: {
            // {"params_data":{"pH":7,"TDS":100,"Temp":32,"HUMIDITY":65,"TSS":15,"BOD":8,"COD":12},"type":"QUALITY","created_on":"2023-12-10T12:58:57Z","unit_id":"TFG2235Q"}
            double ph_value = (params->extra_params.ph_value > 0) ? params->extra_params.ph_value : params->scaled_value;
            double tds_value = (params->extra_params.tds_value > 0) ? params->extra_params.tds_value : (params->scaled_value * 10);
            double temp_value = (params->extra_params.temp_value > 0) ? params->extra_params.temp_value : 25.0; // Default temp
            double humidity_value = (params->extra_params.humidity_value > 0) ? params->extra_params.humidity_value : 60.0; // Default humidity
            double tss_value = (params->extra_params.tss_value > 0) ? params->extra_params.tss_value : 10.0; // Default TSS
            double bod_value = (params->extra_params.bod_value > 0) ? params->extra_params.bod_value : 5.0; // Default BOD
            double cod_value = (params->extra_params.cod_value > 0) ? params->extra_params.cod_value : 8.0; // Default COD

            snprintf(json_buffer, buffer_size,
                "{"
                "\"params_data\":{"
                "\"pH\":%.2f,"
                "\"TDS\":%.2f,"
                "\"Temp\":%.2f,"
                "\"HUMIDITY\":%.2f,"
                "\"TSS\":%.2f,"
                "\"BOD\":%.2f,"
                "\"COD\":%.2f"
                "},"
                "\"type\":\"QUALITY\","
                "\"created_on\":\"%s\","
                "\"unit_id\":\"%s\""
                "}",
                ph_value,
                tds_value,
                temp_value,
                humidity_value,
                tss_value,
                bod_value,
                cod_value,
                params->timestamp,
                params->unit_id);
            break;
        }
        
        default: {
            ESP_LOGE(TAG, "Unsupported JSON template type: %d", params->type);
            return ESP_ERR_NOT_SUPPORTED;
        }
    }
    
    ESP_LOGI(TAG, "JSON created successfully (%d bytes): %s", strlen(json_buffer), json_buffer);
    return ESP_OK;
}

// Generate JSON for a sensor configuration with real data
esp_err_t generate_sensor_json(const sensor_config_t* sensor, double scaled_value,
                              uint32_t raw_value, const network_stats_t* net_stats,
                              char* json_buffer, size_t buffer_size)
{
    if (!sensor || !json_buffer || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Generating JSON for sensor: %s (Type: %s, Unit: %s)",
             sensor->name, sensor->sensor_type, sensor->unit_id);

    // Prepare JSON parameters
    json_params_t params = {0};

    // Determine JSON template type from sensor type
    params.type = get_json_type_from_sensor_type(sensor->sensor_type);
    if (params.type == JSON_TYPE_UNKNOWN) {
        ESP_LOGE(TAG, "Unknown sensor type: %s", sensor->sensor_type);
        return ESP_ERR_INVALID_ARG;
    }

    // Copy basic parameters
    strncpy(params.unit_id, sensor->unit_id, sizeof(params.unit_id) - 1);
    params.scaled_value = scaled_value;
    params.raw_value = raw_value;
    params.slave_id = sensor->slave_id;

    // Format timestamp
    format_timestamp_iso8601(params.timestamp, sizeof(params.timestamp));

    // Add network telemetry data
    if (net_stats) {
        params.signal_strength = net_stats->signal_strength;
        strncpy(params.network_type, net_stats->network_type, sizeof(params.network_type) - 1);

        // Determine quality based on signal strength
        if (net_stats->signal_strength >= -60) {
            strncpy(params.network_quality, "Excellent", sizeof(params.network_quality) - 1);
        } else if (net_stats->signal_strength >= -70) {
            strncpy(params.network_quality, "Good", sizeof(params.network_quality) - 1);
        } else if (net_stats->signal_strength >= -80) {
            strncpy(params.network_quality, "Fair", sizeof(params.network_quality) - 1);
        } else {
            strncpy(params.network_quality, "Poor", sizeof(params.network_quality) - 1);
        }
    } else {
        // Default values when network stats unavailable
        params.signal_strength = 0;
        strncpy(params.network_type, "Unknown", sizeof(params.network_type) - 1);
        strncpy(params.network_quality, "Unknown", sizeof(params.network_quality) - 1);
    }

    // Set additional parameters for specific types
    if (params.type == JSON_TYPE_ENERGY) {
        // For ENERGY type, use meter_type if available, otherwise fallback to sensor name
        if (strlen(sensor->meter_type) > 0) {
            strncpy(params.extra_params.meter_id, sensor->meter_type, sizeof(params.extra_params.meter_id) - 1);
        } else if (strlen(sensor->name) > 0) {
            strncpy(params.extra_params.meter_id, sensor->name, sizeof(params.extra_params.meter_id) - 1);
        }
    } else if (params.type == JSON_TYPE_QUALITY) {
        // For QUALITY type, use scaled_value as primary parameter and set reasonable defaults
        params.extra_params.ph_value = scaled_value;
        params.extra_params.tds_value = scaled_value * 10; // Example conversion
        params.extra_params.temp_value = 25.0; // Default temperature
        params.extra_params.humidity_value = 60.0; // Default humidity
        params.extra_params.tss_value = 10.0; // Default TSS
        params.extra_params.bod_value = 5.0; // Default BOD
        params.extra_params.cod_value = 8.0; // Default COD
    }

    // Generate the JSON
    esp_err_t ret = create_json_payload(&params, json_buffer, buffer_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create JSON payload for sensor %s", sensor->name);
        return ret;
    }

    ESP_LOGI(TAG, "JSON generated for sensor %s: %s", sensor->name, json_buffer);
    return ESP_OK;
}

// Generate JSON for a sensor configuration with real data and hex string
// Generate JSON for a sensor configuration with real data and hex string
esp_err_t generate_sensor_json_with_hex(const sensor_config_t* sensor, double scaled_value,
                              uint32_t raw_value, const char* hex_string,
                              const network_stats_t* net_stats,
                              char* json_buffer, size_t buffer_size)
{
    if (!sensor || !json_buffer || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Generating JSON for sensor: %s (Type: %s, Unit: %s, Hex: %s)",
             sensor->name, sensor->sensor_type, sensor->unit_id, hex_string ? hex_string : "NULL");

    // Prepare JSON parameters
    json_params_t params = {0};

    // Determine JSON template type from sensor type
    params.type = get_json_type_from_sensor_type(sensor->sensor_type);
    if (params.type == JSON_TYPE_UNKNOWN) {
        ESP_LOGE(TAG, "Unknown sensor type: %s", sensor->sensor_type);
        return ESP_ERR_INVALID_ARG;
    }

    // Copy basic parameters
    strncpy(params.unit_id, sensor->unit_id, sizeof(params.unit_id) - 1);
    params.scaled_value = scaled_value;
    params.raw_value = raw_value;
    params.slave_id = sensor->slave_id;

    // Format timestamp
    format_timestamp_iso8601(params.timestamp, sizeof(params.timestamp));

    // Add network telemetry data
    if (net_stats) {
        params.signal_strength = net_stats->signal_strength;
        strncpy(params.network_type, net_stats->network_type, sizeof(params.network_type) - 1);

        // Determine quality based on signal strength
        if (net_stats->signal_strength >= -60) {
            strncpy(params.network_quality, "Excellent", sizeof(params.network_quality) - 1);
        } else if (net_stats->signal_strength >= -70) {
            strncpy(params.network_quality, "Good", sizeof(params.network_quality) - 1);
        } else if (net_stats->signal_strength >= -80) {
            strncpy(params.network_quality, "Fair", sizeof(params.network_quality) - 1);
        } else {
            strncpy(params.network_quality, "Poor", sizeof(params.network_quality) - 1);
        }
    } else {
        // Default values when network stats unavailable
        params.signal_strength = 0;
        strncpy(params.network_type, "Unknown", sizeof(params.network_type) - 1);
        strncpy(params.network_quality, "Unknown", sizeof(params.network_quality) - 1);
    }

    // Set additional parameters for specific types
    if (params.type == JSON_TYPE_ENERGY) {
        // Copy hex string from Test RS485
        if (hex_string && strlen(hex_string) > 0) {
            strncpy(params.extra_params.hex_string, hex_string, sizeof(params.extra_params.hex_string) - 1);
        }
        // Use meter_type if available, otherwise fallback to sensor name
        if (strlen(sensor->meter_type) > 0) {
            strncpy(params.extra_params.meter_id, sensor->meter_type, sizeof(params.extra_params.meter_id) - 1);
        } else if (strlen(sensor->name) > 0) {
            strncpy(params.extra_params.meter_id, sensor->name, sizeof(params.extra_params.meter_id) - 1);
        }
    } else if (params.type == JSON_TYPE_QUALITY) {
        // For QUALITY type, use scaled_value as primary parameter and set reasonable defaults
        params.extra_params.ph_value = scaled_value;
        params.extra_params.tds_value = scaled_value * 10; // Example conversion
        params.extra_params.temp_value = 25.0; // Default temperature
        params.extra_params.humidity_value = 60.0; // Default humidity
        params.extra_params.tss_value = 10.0; // Default TSS
        params.extra_params.bod_value = 5.0; // Default BOD
        params.extra_params.cod_value = 8.0; // Default COD
    }

    // Generate the JSON
    esp_err_t ret = create_json_payload(&params, json_buffer, buffer_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create JSON payload for sensor %s", sensor->name);
        return ret;
    }

    ESP_LOGI(TAG, "JSON generated for sensor %s: %s", sensor->name, json_buffer);
    return ESP_OK;
}

// Generate JSON for a sensor reading with quality parameters (for QUALITY sensors)
esp_err_t generate_quality_sensor_json(const sensor_reading_t* reading, char* json_buffer, size_t buffer_size)
{
    if (!reading || !json_buffer || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Generating JSON for quality sensor reading: %s", reading->unit_id);
    
    // Clear buffer
    memset(json_buffer, 0, buffer_size);
    
    // Create JSON for water quality sensor with actual parameter values
    snprintf(json_buffer, buffer_size,
        "{"
        "\"params_data\":{"
        "\"pH\":%.2f,"
        "\"TDS\":%.2f,"
        "\"Temp\":%.2f,"
        "\"HUMIDITY\":%.2f,"
        "\"TSS\":%.2f,"
        "\"BOD\":%.2f,"
        "\"COD\":%.2f"
        "},"
        "\"type\":\"QUALITY\","
        "\"created_on\":\"%s\","
        "\"unit_id\":\"%s\""
        "}",
        reading->quality_params.ph_value,
        reading->quality_params.tds_value,
        reading->quality_params.temp_value,
        reading->quality_params.humidity_value,
        reading->quality_params.tss_value,
        reading->quality_params.bod_value,
        reading->quality_params.cod_value,
        reading->timestamp,
        reading->unit_id);
    
    ESP_LOGI(TAG, "Quality JSON generated (%d bytes): %s", strlen(json_buffer), json_buffer);
    return ESP_OK;
}
