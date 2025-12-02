// sensor_manager.c - Multi-sensor management implementation

#include "sensor_manager.h"
#include "modbus.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <time.h>
#include <math.h>
#include <inttypes.h>

static const char *TAG = "SENSOR_MGR";

esp_err_t sensor_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing sensor manager");
    return ESP_OK;
}

esp_err_t convert_modbus_data(uint16_t *registers, int reg_count, 
                             const char* data_type, const char* byte_order,
                             double scale_factor, double *result, uint32_t *raw_value)
{
    if (!registers || !data_type || !byte_order || !result || !raw_value) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Converting data: Type=%s, Order=%s, Scale=%.6f", data_type, byte_order, scale_factor);

    // Handle specific format names from web interface
    const char* actual_data_type = data_type;
    const char* actual_byte_order = byte_order;
    
    // 32-bit Integer formats
    if (strstr(data_type, "INT32_1234") || strstr(data_type, "UINT32_1234")) {
        actual_data_type = strstr(data_type, "UINT32") ? "UINT32" : "INT32";
        actual_byte_order = "BIG_ENDIAN";     // 1234 = ABCD = BIG_ENDIAN
    } else if (strstr(data_type, "INT32_4321") || strstr(data_type, "UINT32_4321")) {
        actual_data_type = strstr(data_type, "UINT32") ? "UINT32" : "INT32";
        actual_byte_order = "LITTLE_ENDIAN";  // 4321 = DCBA = LITTLE_ENDIAN
    } else if (strstr(data_type, "INT32_3412") || strstr(data_type, "UINT32_3412")) {
        actual_data_type = strstr(data_type, "UINT32") ? "UINT32" : "INT32";
        actual_byte_order = "BIG_ENDIAN";     // 3412 = CDAB = BIG_ENDIAN  
    } else if (strstr(data_type, "INT32_2143") || strstr(data_type, "UINT32_2143")) {
        actual_data_type = strstr(data_type, "UINT32") ? "UINT32" : "INT32";
        actual_byte_order = "MIXED_BADC";     // 2143 = BADC = MIXED_BADC
    }
    
    // 32-bit Float formats
    else if (strstr(data_type, "FLOAT32_1234")) {
        actual_data_type = "FLOAT32";
        actual_byte_order = "BIG_ENDIAN";     // 1234 = ABCD
    } else if (strstr(data_type, "FLOAT32_4321")) {
        actual_data_type = "FLOAT32";
        actual_byte_order = "LITTLE_ENDIAN";  // 4321 = DCBA
    } else if (strstr(data_type, "FLOAT32_3412")) {
        actual_data_type = "FLOAT32";
        actual_byte_order = "BIG_ENDIAN";     // 3412 = CDAB
    } else if (strstr(data_type, "FLOAT32_2143")) {
        actual_data_type = "FLOAT32";
        actual_byte_order = "MIXED_BADC";     // 2143 = BADC
    }
    
    // 64-bit formats - Handle both full and truncated format names
    else if (strstr(data_type, "INT64_12345678") || strstr(data_type, "UINT64_12345678") || strstr(data_type, "FLOAT64_12345678") ||
             strstr(data_type, "INT64_1234567") || strstr(data_type, "UINT64_1234567") || strstr(data_type, "FLOAT64_1234567")) {
        actual_data_type = strstr(data_type, "UINT64") ? "UINT64" : strstr(data_type, "FLOAT64") ? "FLOAT64" : "INT64";
        actual_byte_order = "BIG_ENDIAN";     // 12345678/1234567 = standard order
    } else if (strstr(data_type, "INT64_87654321") || strstr(data_type, "UINT64_87654321") || strstr(data_type, "FLOAT64_87654321") ||
               strstr(data_type, "INT64_8765432") || strstr(data_type, "UINT64_8765432") || strstr(data_type, "FLOAT64_8765432")) {
        actual_data_type = strstr(data_type, "UINT64") ? "UINT64" : strstr(data_type, "FLOAT64") ? "FLOAT64" : "INT64";
        actual_byte_order = "LITTLE_ENDIAN";  // 87654321/8765432 = reversed
    } else if (strstr(data_type, "INT64_78563412") || strstr(data_type, "UINT64_78563412") || strstr(data_type, "FLOAT64_78563412") ||
               strstr(data_type, "INT64_7856341") || strstr(data_type, "UINT64_7856341") || strstr(data_type, "FLOAT64_7856341")) {
        actual_data_type = strstr(data_type, "UINT64") ? "UINT64" : strstr(data_type, "FLOAT64") ? "FLOAT64" : "INT64";
        actual_byte_order = "MIXED_BADC";     // 78563412/7856341 = mixed order
    }
    
    ESP_LOGI(TAG, "Mapped to: Type=%s, Order=%s", actual_data_type, actual_byte_order);

    if (strcmp(actual_data_type, "UINT16") == 0 && reg_count >= 1) {
        *raw_value = registers[0];
        *result = (double)(*raw_value) * scale_factor;
        ESP_LOGI(TAG, "UINT16: Raw=0x%04" PRIX32 " (%" PRIu32 ") -> %.6f", *raw_value, *raw_value, *result);
        
    } else if (strcmp(actual_data_type, "INT16") == 0 && reg_count >= 1) {
        int16_t signed_val = (int16_t)registers[0];
        *raw_value = registers[0];
        *result = (double)signed_val * scale_factor;
        ESP_LOGI(TAG, "INT16: Raw=0x%04" PRIX32 " (%d) -> %.6f", *raw_value, signed_val, *result);
        
    } else if ((strcmp(actual_data_type, "UINT32") == 0 || strcmp(actual_data_type, "INT32") == 0) && reg_count >= 2) {
        uint32_t combined_value;
        
        if (strcmp(actual_byte_order, "BIG_ENDIAN") == 0) {
            // ABCD - reg[0] is high word, reg[1] is low word
            combined_value = ((uint32_t)registers[0] << 16) | registers[1];
        } else if (strcmp(actual_byte_order, "LITTLE_ENDIAN") == 0) {
            // CDAB - reg[1] is high word, reg[0] is low word
            combined_value = ((uint32_t)registers[1] << 16) | registers[0];
        } else if (strcmp(actual_byte_order, "MIXED_BADC") == 0) {
            // BADC - byte swap within each register
            uint16_t reg0_swapped = ((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF);
            uint16_t reg1_swapped = ((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF);
            combined_value = ((uint32_t)reg0_swapped << 16) | reg1_swapped;
        } else if (strcmp(actual_byte_order, "MIXED_DCBA") == 0) {
            // DCBA - completely reversed
            uint16_t reg0_swapped = ((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF);
            uint16_t reg1_swapped = ((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF);
            combined_value = ((uint32_t)reg1_swapped << 16) | reg0_swapped;
        } else {
            ESP_LOGE(TAG, "Unknown byte order: %s", actual_byte_order);
            return ESP_ERR_INVALID_ARG;
        }

        *raw_value = combined_value;
        
        if (strcmp(actual_data_type, "INT32") == 0) {
            int32_t signed_val = (int32_t)combined_value;
            *result = (double)signed_val * scale_factor;
            ESP_LOGI(TAG, "INT32: Raw=0x%08" PRIX32 " (%" PRId32 ") -> %.6f", combined_value, signed_val, *result);
        } else {
            *result = (double)combined_value * scale_factor;
            ESP_LOGI(TAG, "UINT32: Raw=0x%08" PRIX32 " (%" PRIu32 ") -> %.6f", combined_value, combined_value, *result);
        }
        
    } else if (strcmp(actual_data_type, "HEX") == 0) {
        // HEX type - concatenate all registers as hex value
        *raw_value = 0;
        for (int i = 0; i < reg_count && i < 2; i++) {
            *raw_value = (*raw_value << 16) | registers[i];
        }
        *result = (double)(*raw_value) * scale_factor;
        ESP_LOGI(TAG, "HEX: Raw=0x%08" PRIX32 " -> %.6f", *raw_value, *result);
        
    } else if (strcmp(actual_data_type, "FLOAT32") == 0 && reg_count >= 2) {
        uint32_t combined_value;
        
        if (strcmp(actual_byte_order, "BIG_ENDIAN") == 0) {
            combined_value = ((uint32_t)registers[0] << 16) | registers[1];
        } else if (strcmp(actual_byte_order, "LITTLE_ENDIAN") == 0) {
            combined_value = ((uint32_t)registers[1] << 16) | registers[0];
        } else {
            ESP_LOGE(TAG, "FLOAT32 only supports BIG_ENDIAN and LITTLE_ENDIAN");
            return ESP_ERR_INVALID_ARG;
        }
        
        // Convert to IEEE 754 float
        union {
            uint32_t i;
            float f;
        } converter;
        converter.i = combined_value;
        
        *raw_value = combined_value;
        *result = (double)converter.f * scale_factor;
        ESP_LOGI(TAG, "FLOAT32: Raw=0x%08" PRIX32 " (%.6f) -> %.6f", combined_value, converter.f, *result);
        
    } else if (strcmp(actual_data_type, "FLOAT64") == 0 && reg_count >= 4) {
        // FLOAT64 handling - 4 registers (64-bit double precision)
        uint64_t combined_value64 = 0;
        
        if (strcmp(actual_byte_order, "BIG_ENDIAN") == 0) {
            // FLOAT64_12345678 (ABCDEFGH) - Standard big endian
            combined_value64 = ((uint64_t)registers[0] << 48) | ((uint64_t)registers[1] << 32) | 
                              ((uint64_t)registers[2] << 16) | registers[3];
        } else if (strcmp(actual_byte_order, "LITTLE_ENDIAN") == 0) {
            // FLOAT64_87654321 (HGFEDCBA) - Full little endian
            combined_value64 = ((uint64_t)registers[3] << 48) | ((uint64_t)registers[2] << 32) | 
                              ((uint64_t)registers[1] << 16) | registers[0];
        } else if (strcmp(actual_byte_order, "MIXED_BADC") == 0) {
            // FLOAT64_78563412 (GHEFCDAB) - Mixed byte order
            uint64_t val64_78563412 = ((uint64_t)(((registers[3] & 0xFF) << 8) | ((registers[3] >> 8) & 0xFF)) << 48) |
                                     ((uint64_t)(((registers[2] & 0xFF) << 8) | ((registers[2] >> 8) & 0xFF)) << 32) |
                                     ((uint64_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) |
                                     (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
            combined_value64 = val64_78563412;
        } else {
            ESP_LOGE(TAG, "FLOAT64 unsupported byte order: %s", actual_byte_order);
            return ESP_ERR_INVALID_ARG;
        }
        
        // Convert to IEEE 754 double precision
        union {
            uint64_t i;
            double d;
        } converter64;
        converter64.i = combined_value64;
        
        *raw_value = (uint32_t)(combined_value64 & 0xFFFFFFFF);  // Store lower 32 bits for compatibility
        *result = converter64.d * scale_factor;
        ESP_LOGI(TAG, "FLOAT64: Raw=0x%016" PRIX64 " (%.6f) -> %.6f", combined_value64, converter64.d, *result);
        
    } else {
        // Calculate expected register count for better error message
        int expected_regs = 1;  // Default for INT16/UINT16
        if (strcmp(actual_data_type, "UINT32") == 0 || strcmp(actual_data_type, "INT32") == 0 || strcmp(actual_data_type, "FLOAT32") == 0) {
            expected_regs = 2;
        } else if (strcmp(actual_data_type, "FLOAT64") == 0 || strcmp(actual_data_type, "INT64") == 0 || strcmp(actual_data_type, "UINT64") == 0) {
            expected_regs = 4;
        }
        
        ESP_LOGE(TAG, "Unsupported data type or insufficient registers: %s -> %s (need %d, have %d)", 
                 data_type, actual_data_type, expected_regs, reg_count);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t sensor_test_live(const sensor_config_t *sensor, sensor_test_result_t *result)
{
    if (!sensor || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    // Clear result
    memset(result, 0, sizeof(sensor_test_result_t));
    
    ESP_LOGI(TAG, "Testing sensor: %s (Unit: %s, Slave: %d)", 
             sensor->name, sensor->unit_id, sensor->slave_id);

    uint32_t start_time = esp_timer_get_time() / 1000;
    
    // Perform Modbus read based on register type
    modbus_result_t modbus_result;
    
    // Default to HOLDING if register_type is empty or invalid
    const char* reg_type = sensor->register_type;
    if (!reg_type || strlen(reg_type) == 0 || 
        (strcmp(reg_type, "HOLDING") != 0 && strcmp(reg_type, "INPUT") != 0)) {
        ESP_LOGW(TAG, "Invalid register type '%s', defaulting to HOLDING", reg_type ? reg_type : "NULL");
        reg_type = "HOLDING";
    }
    
    // For Flow-Meter, ZEST, and Panda USM sensors, read 4 registers
    int quantity_to_read = sensor->quantity;
    if (strcmp(sensor->sensor_type, "Flow-Meter") == 0) {
        quantity_to_read = 4;
        ESP_LOGI(TAG, "Flow-Meter sensor detected, reading 4 registers for UINT32_BADC + FLOAT32_BADC interpretation");
    } else if (strcmp(sensor->sensor_type, "ZEST") == 0) {
        quantity_to_read = 4;
        ESP_LOGI(TAG, "ZEST sensor detected, reading 4 registers for UINT32_CDAB + FLOAT32_ABCD interpretation");
    } else if (strcmp(sensor->sensor_type, "Panda_USM") == 0) {
        quantity_to_read = 4;
        ESP_LOGI(TAG, "Panda USM sensor detected, reading 4 registers for DOUBLE64 (Net Volume)");
    }
    
    // Set the baud rate for this sensor
    int baud_rate = sensor->baud_rate > 0 ? sensor->baud_rate : 9600;
    ESP_LOGI(TAG, "Setting baud rate to %d bps for sensor '%s'", baud_rate, sensor->name);
    esp_err_t baud_err = modbus_set_baud_rate(baud_rate);
    if (baud_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set baud rate for sensor '%s': %s", sensor->name, esp_err_to_name(baud_err));
        // Continue anyway with current baud rate
    }
    
    if (strcmp(reg_type, "HOLDING") == 0) {
        modbus_result = modbus_read_holding_registers(sensor->slave_id, 
                                                     sensor->register_address, 
                                                     quantity_to_read);
    } else if (strcmp(reg_type, "INPUT") == 0) {
        modbus_result = modbus_read_input_registers(sensor->slave_id, 
                                                   sensor->register_address, 
                                                   quantity_to_read);
    } else {
        snprintf(result->error_message, sizeof(result->error_message), 
                "Unknown register type: %s", reg_type);
        return ESP_ERR_INVALID_ARG;
    }

    result->response_time_ms = (esp_timer_get_time() / 1000) - start_time;

    if (modbus_result != MODBUS_SUCCESS) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message), 
                "Modbus error: %d (timeout/CRC/communication)", modbus_result);
        ESP_LOGE(TAG, "Modbus read failed: %d", modbus_result);
        return ESP_FAIL;
    }

    // Get the raw register values
    uint16_t registers[10];
    int reg_count = modbus_get_response_length();
    
    if (reg_count < sensor->quantity) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message), 
                "Insufficient registers received: got %d, expected %d", reg_count, sensor->quantity);
        return ESP_FAIL;
    }

    for (int i = 0; i < reg_count && i < 10; i++) {
        registers[i] = modbus_get_response_buffer(i);
    }

    // Create hex representation
    char hex_buf[64] = {0};
    for (int i = 0; i < reg_count; i++) {
        char temp[8];
        snprintf(temp, sizeof(temp), "%04X ", registers[i]);
        strncat(hex_buf, temp, sizeof(hex_buf) - strlen(hex_buf) - 1);
    }
    strncpy(result->raw_hex, hex_buf, sizeof(result->raw_hex) - 1);

    // Special handling for Flow-Meter sensors (4 registers: UINT32_BADC + FLOAT32_BADC)
    if (strcmp(sensor->sensor_type, "Flow-Meter") == 0 && reg_count >= 4) {
        // Flow-Meter reads 4 registers:
        // Registers [0-1]: Cumulative Flow Integer part (32-bit UINT, BADC word-swapped)
        // Registers [2-3]: Cumulative Flow Decimal part (32-bit FLOAT, BADC word-swapped)
        // Example: 33073.865 m³ = 33073 (registers[0-1]) + 0.865 (float in registers[2-3])

        // Integer part: BADC format (word-swapped) = (reg[1] << 16) | reg[0]
        uint32_t integer_part_raw = ((uint32_t)registers[1] << 16) | registers[0];
        double integer_part = (double)integer_part_raw;

        // Decimal part: BADC format (word-swapped) = (reg[3] << 16) | reg[2]
        uint32_t float_bits = ((uint32_t)registers[3] << 16) | registers[2];
        float decimal_part_float;
        memcpy(&decimal_part_float, &float_bits, sizeof(float));
        double decimal_part = (double)decimal_part_float;

        // Sum integer and decimal parts, then apply scale factor
        result->scaled_value = (integer_part + decimal_part) * sensor->scale_factor;
        result->raw_value = integer_part_raw; // Store integer part as raw value

        ESP_LOGI(TAG, "Flow-Meter Calculation: Integer=0x%08lX(%lu) + Decimal(FLOAT)=0x%08lX(%.6f) = %.6f",
                 (unsigned long)integer_part_raw, (unsigned long)integer_part_raw,
                 (unsigned long)float_bits, decimal_part, result->scaled_value);
    }
    // Special handling for ZEST sensors (AquaGen Flow Meter format)
    else if (strcmp(sensor->sensor_type, "ZEST") == 0 && reg_count >= 4) {
        // ZEST reads 4 registers (per AquaGen Modbus documentation):
        // Register [0] @ 0x1019: Cumulative Flow Integer part (16-bit UINT)
        // Register [1]: Unused (0x0000)
        // Registers [2,3] @ 0x101B-101C: Cumulative Flow Decimal part (FLOAT32, IEEE 754 Big Endian)
        // Example: 43.675 m³ = 43 (register[0]) + 0.675 (float in registers[2-3])

        // First register contains the integer part (16-bit value)
        uint32_t integer_part_raw = (uint32_t)registers[0];
        double integer_part = (double)integer_part_raw;

        // Registers 2-3 contain decimal part as 32-bit FLOAT Big Endian (ABCD)
        // Convert to IEEE 754 float: registers[2] is HIGH word, registers[3] is LOW word
        uint32_t float_bits = ((uint32_t)registers[2] << 16) | registers[3];
        float decimal_part_float;
        memcpy(&decimal_part_float, &float_bits, sizeof(float));
        double decimal_part = (double)decimal_part_float;

        // Sum integer and decimal parts, then apply scale factor
        result->scaled_value = (integer_part + decimal_part) * sensor->scale_factor;
        result->raw_value = integer_part_raw; // Store integer part as raw value

        ESP_LOGI(TAG, "ZEST Calculation: Integer=0x%04X(%lu) + Decimal(FLOAT)=0x%08lX(%.6f) = %.6f",
                 (unsigned int)integer_part_raw, (unsigned long)integer_part_raw,
                 (unsigned long)float_bits, decimal_part, result->scaled_value);
    }
    // Special handling for Panda USM sensors (64-bit double format)
    else if (strcmp(sensor->sensor_type, "Panda_USM") == 0 && reg_count >= 4) {
        // Panda USM stores net volume as 64-bit double at register 4
        // Big-endian format: registers[0] = MSW, registers[3] = LSW
        uint64_t combined_value64 = ((uint64_t)registers[0] << 48) |
                                   ((uint64_t)registers[1] << 32) |
                                   ((uint64_t)registers[2] << 16) |
                                   registers[3];

        // Convert to double
        double net_volume;
        memcpy(&net_volume, &combined_value64, sizeof(double));

        // Apply scale factor
        result->scaled_value = net_volume * sensor->scale_factor;
        result->raw_value = (uint32_t)(combined_value64 >> 32); // Store upper 32 bits as raw value

        ESP_LOGI(TAG, "Panda USM Calculation: DOUBLE64=0x%016llX = %.6f m³",
                 (unsigned long long)combined_value64, result->scaled_value);
    }
    // Special handling for Clampon flow meters (4 registers: UINT32_BADC + FLOAT32_BADC)
    else if (strcmp(sensor->sensor_type, "Clampon") == 0 && reg_count >= 4) {
        // Clampon reads 4 registers:
        // Registers [0-1]: Cumulative Flow Integer part (32-bit UINT, BADC word-swapped)
        // Registers [2-3]: Cumulative Flow Decimal part (32-bit FLOAT, BADC word-swapped)
        // Example: 33073.865 m³ = 33073 (registers[0-1]) + 0.865 (float in registers[2-3])

        // Integer part: BADC format (word-swapped) = (reg[1] << 16) | reg[0]
        uint32_t integer_part_raw = ((uint32_t)registers[1] << 16) | registers[0];
        double integer_part = (double)integer_part_raw;

        // Decimal part: BADC format (word-swapped) = (reg[3] << 16) | reg[2]
        uint32_t float_bits = ((uint32_t)registers[3] << 16) | registers[2];
        float decimal_part_float;
        memcpy(&decimal_part_float, &float_bits, sizeof(float));
        double decimal_part = (double)decimal_part_float;

        // Sum integer and decimal parts, then apply scale factor
        result->scaled_value = (integer_part + decimal_part) * sensor->scale_factor;
        result->raw_value = integer_part_raw; // Store integer part as raw value

        ESP_LOGI(TAG, "Clampon Calculation: Integer=0x%08lX(%lu) + Decimal(FLOAT)=0x%08lX(%.6f) = %.6f",
                 (unsigned long)integer_part_raw, (unsigned long)integer_part_raw,
                 (unsigned long)float_bits, decimal_part, result->scaled_value);
    }
    // Special handling for Dailian EMF flow meters (2 registers: UINT32 word-swapped totaliser)
    else if (strcmp(sensor->sensor_type, "Dailian_EMF") == 0 && reg_count >= 2) {
        // Dailian EMF reads 2 registers at address 0x07D6 (2006):
        // Registers [0-1]: Totaliser value (32-bit UINT, word-swapped)
        // Format: (reg[1] << 16) | reg[0]

        // Totaliser: word-swapped = (reg[1] << 16) | reg[0]
        uint32_t totaliser_raw = ((uint32_t)registers[1] << 16) | registers[0];

        // Apply scale factor
        result->scaled_value = (double)totaliser_raw * sensor->scale_factor;
        result->raw_value = totaliser_raw;

        ESP_LOGI(TAG, "Dailian_EMF Calculation: Totaliser=0x%08lX(%lu) * %.6f = %.6f",
                 (unsigned long)totaliser_raw, (unsigned long)totaliser_raw,
                 sensor->scale_factor, result->scaled_value);
    }
    // Special handling for Panda EMF flow meters (4 registers: INT32_BE + FLOAT32_BE)
    else if (strcmp(sensor->sensor_type, "Panda_EMF") == 0 && reg_count >= 4) {
        // Panda EMF reads 4 registers at address 0x1012 (4114):
        // Registers [0-1]: Totalizer integer part (32-bit INT, big-endian)
        // Registers [2-3]: Totalizer decimal part (32-bit FLOAT, big-endian)
        // Total = integer_part + float_decimal

        // Integer part: Big-endian = (reg[0] << 16) | reg[1]
        int32_t integer_part = (int32_t)(((uint32_t)registers[0] << 16) | registers[1]);
        double integer_value = (double)integer_part;

        // Decimal part: Big-endian = (reg[2] << 16) | reg[3]
        uint32_t float_bits = ((uint32_t)registers[2] << 16) | registers[3];
        float decimal_part_float;
        memcpy(&decimal_part_float, &float_bits, sizeof(float));
        double decimal_value = (double)decimal_part_float;

        // Sum integer and decimal parts, then apply scale factor
        result->scaled_value = (integer_value + decimal_value) * sensor->scale_factor;
        result->raw_value = (uint32_t)integer_part; // Store integer part as raw value

        ESP_LOGI(TAG, "Panda_EMF Calculation: Integer=0x%08lX(%ld) + Decimal(FLOAT)=0x%08lX(%.6f) = %.6f",
                 (unsigned long)((uint32_t)integer_part), (long)integer_part,
                 (unsigned long)float_bits, decimal_value, result->scaled_value);
    }
    // Special handling for Panda Level sensors (1 register: UINT16 level value)
    else if (strcmp(sensor->sensor_type, "Panda_Level") == 0 && reg_count >= 1) {
        // Panda Level reads 1 register at address 0x0001 (1):
        // Register [0]: Level value (distance from sensor to water surface)
        // Calculation: Level % = ((Sensor Height - Raw Value) / Tank Height) * 100

        // Raw level value (distance reading)
        uint16_t raw_level = registers[0];
        double level_value = (double)raw_level;

        // Apply the level calculation if sensor_height and max_water_level are set
        if (sensor->max_water_level > 0) {
            // Level % = ((Sensor Height - Raw Value) / Tank Height) * 100
            result->scaled_value = ((sensor->sensor_height - level_value) / sensor->max_water_level) * 100.0;
            // Clamp to 0-100% range
            if (result->scaled_value < 0) result->scaled_value = 0.0;
            if (result->scaled_value > 100) result->scaled_value = 100.0;
        } else {
            // If no tank height set, just return raw value scaled
            result->scaled_value = level_value * sensor->scale_factor;
        }
        result->raw_value = raw_level;

        ESP_LOGI(TAG, "Panda_Level Calculation: Raw=%u, SensorHeight=%.2f, TankHeight=%.2f, Level%%=%.2f",
                 raw_level, sensor->sensor_height, sensor->max_water_level, result->scaled_value);
    } else {
        // Convert the data using standard conversion
        esp_err_t conv_result = convert_modbus_data(registers, reg_count,
                                                   sensor->data_type, sensor->byte_order,
                                                   sensor->scale_factor,
                                                   &result->scaled_value, &result->raw_value);

        if (conv_result != ESP_OK) {
            result->success = false;
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Data conversion failed");
            return conv_result;
        }
    }

    result->success = true;
    ESP_LOGI(TAG, "Test successful: %.6f (Response: %lu ms)", 
             result->scaled_value, result->response_time_ms);

    return ESP_OK;
}

esp_err_t sensor_read_single(const sensor_config_t *sensor, sensor_reading_t *reading)
{
    if (!sensor || !reading) {
        return ESP_ERR_INVALID_ARG;
    }

    // For water quality sensors, use specialized multi-parameter reading
    if (strcmp(sensor->sensor_type, "QUALITY") == 0) {
        return sensor_read_quality(sensor, reading);
    }

    // Clear reading
    memset(reading, 0, sizeof(sensor_reading_t));
    
    // Copy basic info
    strncpy(reading->unit_id, sensor->unit_id, sizeof(reading->unit_id) - 1);
    strncpy(reading->sensor_name, sensor->name, sizeof(reading->sensor_name) - 1);
    
    // Get timestamp
    time_t now;
    struct tm timeinfo;
    time(&now);
    gmtime_r(&now, &timeinfo);
    strftime(reading->timestamp, sizeof(reading->timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

    // Test the sensor
    sensor_test_result_t test_result;
    esp_err_t ret = sensor_test_live(sensor, &test_result);
    
    if (ret == ESP_OK && test_result.success) {
        // Apply sensor type-specific calculations
        if (strcmp(sensor->sensor_type, "Level") == 0) {
            // Level sensor calculation: (Sensor Height - Raw Value) / Maximum Water Level * 100
            double raw_scaled_value = test_result.scaled_value;
            double level_percentage = 0.0;
            
            if (sensor->max_water_level > 0) {
                level_percentage = ((sensor->sensor_height - raw_scaled_value) / sensor->max_water_level) * 100.0;
                // Ensure percentage is within reasonable bounds
                if (level_percentage < 0) level_percentage = 0.0;
                if (level_percentage > 100) level_percentage = 100.0;
            }
            
            reading->value = level_percentage;
            ESP_LOGI(TAG, "Level Sensor %s: Raw=%.6f, Height=%.2f, MaxLevel=%.2f -> %.2f%%", 
                     reading->unit_id, raw_scaled_value, sensor->sensor_height, sensor->max_water_level, level_percentage);
        } else if (strcmp(sensor->sensor_type, "Radar Level") == 0) {
            // Radar Level sensor calculation: (Raw Value / Maximum Water Level) * 100
            double raw_scaled_value = test_result.scaled_value;
            double level_percentage = 0.0;
            
            if (sensor->max_water_level > 0) {
                level_percentage = (raw_scaled_value / sensor->max_water_level) * 100.0;
                // Ensure percentage is not negative (but allow over 100% to show overflow)
                if (level_percentage < 0) level_percentage = 0.0;
            }
            
            reading->value = level_percentage;
            ESP_LOGI(TAG, "Radar Level Sensor %s: Raw=%.6f, MaxLevel=%.2f -> %.2f%%", 
                     reading->unit_id, raw_scaled_value, sensor->max_water_level, level_percentage);
        } else if (strcmp(sensor->sensor_type, "ZEST") == 0) {
            // ZEST sensor uses the sensor_test_live function which handles the special format
            // The test_result.scaled_value already contains the combined integer + decimal value
            reading->value = test_result.scaled_value;
            ESP_LOGI(TAG, "ZEST Sensor %s: %.6f", reading->unit_id, reading->value);
        } else {
            // Flow-Meter or other sensor types use direct scaled value
            reading->value = test_result.scaled_value;
            ESP_LOGI(TAG, "Sensor %s: %.6f", reading->unit_id, reading->value);
        }
        
        reading->valid = true;
        reading->raw_value = test_result.raw_value;
        strncpy(reading->raw_hex, test_result.raw_hex, sizeof(reading->raw_hex) - 1);
        strcpy(reading->data_source, "modbus_rs485");
    } else {
        reading->valid = false;
        strcpy(reading->data_source, "error");
        ESP_LOGE(TAG, "Failed to read sensor %s: %s", reading->unit_id, test_result.error_message);
    }

    return ret;
}

// Read water quality sensor with multiple sub-parameters
esp_err_t sensor_read_quality(const sensor_config_t *sensor, sensor_reading_t *reading)
{
    if (!sensor || !reading) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check if this is a water quality sensor
    if (strcmp(sensor->sensor_type, "QUALITY") != 0) {
        ESP_LOGE(TAG, "Sensor %s is not a QUALITY sensor", sensor->name);
        return ESP_ERR_INVALID_ARG;
    }

    // Clear reading
    memset(reading, 0, sizeof(sensor_reading_t));
    
    // Copy basic info
    strncpy(reading->unit_id, sensor->unit_id, sizeof(reading->unit_id) - 1);
    strncpy(reading->sensor_name, sensor->name, sizeof(reading->sensor_name) - 1);
    
    // Get timestamp
    time_t now;
    struct tm timeinfo;
    time(&now);
    gmtime_r(&now, &timeinfo);
    strftime(reading->timestamp, sizeof(reading->timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

    // Initialize default parameter values
    reading->quality_params.ph_value = 7.0;        // Default pH
    reading->quality_params.tds_value = 100.0;     // Default TDS
    reading->quality_params.temp_value = 25.0;     // Default Temperature
    reading->quality_params.humidity_value = 60.0; // Default Humidity
    reading->quality_params.tss_value = 10.0;      // Default TSS
    reading->quality_params.bod_value = 5.0;       // Default BOD
    reading->quality_params.cod_value = 8.0;       // Default COD

    bool any_success = false;
    
    // Read each sub-sensor
    for (int i = 0; i < sensor->sub_sensor_count && i < 8; i++) {
        const sub_sensor_t *sub_sensor = &sensor->sub_sensors[i];
        
        if (!sub_sensor->enabled) {
            continue;
        }

        ESP_LOGI(TAG, "Reading sub-sensor %d: %s (Slave:%d, Reg:%d)", 
                 i, sub_sensor->parameter_name, sub_sensor->slave_id, sub_sensor->register_address);

        // Create a temporary sensor config for this sub-sensor
        sensor_config_t temp_sensor = *sensor;  // Copy main sensor config
        temp_sensor.slave_id = sub_sensor->slave_id;
        temp_sensor.register_address = sub_sensor->register_address;
        temp_sensor.quantity = sub_sensor->quantity;
        strncpy(temp_sensor.data_type, sub_sensor->data_type, sizeof(temp_sensor.data_type) - 1);
        strncpy(temp_sensor.register_type, sub_sensor->register_type, sizeof(temp_sensor.register_type) - 1);
        temp_sensor.scale_factor = sub_sensor->scale_factor;
        strncpy(temp_sensor.byte_order, sub_sensor->byte_order, sizeof(temp_sensor.byte_order) - 1);

        // Test this sub-sensor
        sensor_test_result_t test_result;
        esp_err_t ret = sensor_test_live(&temp_sensor, &test_result);
        
        if (ret == ESP_OK && test_result.success) {
            any_success = true;
            double scaled_value = test_result.scaled_value;
            
            // Map parameter to the correct field based on JSON key
            if (strcmp(sub_sensor->json_key, "pH") == 0) {
                reading->quality_params.ph_value = scaled_value;
                ESP_LOGI(TAG, "pH: %.2f", scaled_value);
            } else if (strcmp(sub_sensor->json_key, "TDS") == 0) {
                reading->quality_params.tds_value = scaled_value;
                ESP_LOGI(TAG, "TDS: %.2f ppm", scaled_value);
            } else if (strcmp(sub_sensor->json_key, "Temp") == 0) {
                reading->quality_params.temp_value = scaled_value;
                ESP_LOGI(TAG, "Temperature: %.2fdegC", scaled_value);
            } else if (strcmp(sub_sensor->json_key, "HUMIDITY") == 0) {
                reading->quality_params.humidity_value = scaled_value;
                ESP_LOGI(TAG, "Humidity: %.2f%%", scaled_value);
            } else if (strcmp(sub_sensor->json_key, "TSS") == 0) {
                reading->quality_params.tss_value = scaled_value;
                ESP_LOGI(TAG, "TSS: %.2f mg/L", scaled_value);
            } else if (strcmp(sub_sensor->json_key, "BOD") == 0) {
                reading->quality_params.bod_value = scaled_value;
                ESP_LOGI(TAG, "BOD: %.2f mg/L", scaled_value);
            } else if (strcmp(sub_sensor->json_key, "COD") == 0) {
                reading->quality_params.cod_value = scaled_value;
                ESP_LOGI(TAG, "COD: %.2f mg/L", scaled_value);
            } else {
                ESP_LOGW(TAG, "Unknown parameter key: %s", sub_sensor->json_key);
            }
        } else {
            ESP_LOGE(TAG, "Failed to read sub-sensor %s: %s", 
                     sub_sensor->parameter_name, test_result.error_message);
        }
    }

    if (any_success) {
        reading->valid = true;
        reading->value = reading->quality_params.ph_value; // Use pH as primary value
        strcpy(reading->data_source, "modbus_rs485_multi");
        ESP_LOGI(TAG, "Water Quality Sensor %s: pH=%.2f, TDS=%.2f, Temp=%.2fdegC, Humidity=%.2f%%, TSS=%.2f, BOD=%.2f, COD=%.2f", 
                 reading->unit_id, 
                 reading->quality_params.ph_value, reading->quality_params.tds_value,
                 reading->quality_params.temp_value, reading->quality_params.humidity_value,
                 reading->quality_params.tss_value, reading->quality_params.bod_value,
                 reading->quality_params.cod_value);
    } else {
        reading->valid = false;
        strcpy(reading->data_source, "error");
        ESP_LOGE(TAG, "All sub-sensors failed for water quality sensor %s", reading->unit_id);
    }

    return any_success ? ESP_OK : ESP_FAIL;
}

esp_err_t sensor_read_all_configured(sensor_reading_t *readings, int max_readings, int *actual_count)
{
    if (!readings || !actual_count || max_readings <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    system_config_t *config = get_system_config();
    *actual_count = 0;

    ESP_LOGI(TAG, "Reading all configured sensors (%d total)", config->sensor_count);

    for (int i = 0; i < config->sensor_count && *actual_count < max_readings; i++) {
        if (config->sensors[i].enabled) {
            ESP_LOGI(TAG, "Reading sensor %d: %s (Unit: %s, Slave: %d)", 
                     i + 1, config->sensors[i].name, config->sensors[i].unit_id, config->sensors[i].slave_id);
            esp_err_t ret = sensor_read_single(&config->sensors[i], &readings[*actual_count]);
            if (ret == ESP_OK && readings[*actual_count].valid) {
                ESP_LOGI(TAG, "Sensor %s read successfully: %.2f", 
                         config->sensors[i].unit_id, readings[*actual_count].value);
                (*actual_count)++;
            } else {
                ESP_LOGE(TAG, "Failed to read sensor %s", config->sensors[i].unit_id);
            }
        } else {
            ESP_LOGW(TAG, "Sensor %d (%s) is disabled", i + 1, config->sensors[i].name);
        }
    }

    ESP_LOGI(TAG, "Successfully read %d/%d sensors", *actual_count, config->sensor_count);
    return ESP_OK;
}

// Utility functions
const char* get_register_type_description(const char* reg_type)
{
    if (strcmp(reg_type, "HOLDING") == 0) return "Holding Registers (0x03)";
    if (strcmp(reg_type, "INPUT") == 0) return "Input Registers (0x04)";
    return "Unknown";
}

const char* get_data_type_description(const char* data_type)
{
    if (strcmp(data_type, "UINT16") == 0) return "16-bit Unsigned Integer";
    if (strcmp(data_type, "INT16") == 0) return "16-bit Signed Integer";
    if (strcmp(data_type, "UINT32") == 0) return "32-bit Unsigned Integer";
    if (strcmp(data_type, "INT32") == 0) return "32-bit Signed Integer";
    if (strcmp(data_type, "FLOAT32") == 0) return "32-bit IEEE 754 Float";
    return "Unknown";
}

const char* get_byte_order_description(const char* byte_order)
{
    if (strcmp(byte_order, "BIG_ENDIAN") == 0) return "Big Endian (ABCD)";
    if (strcmp(byte_order, "LITTLE_ENDIAN") == 0) return "Little Endian (CDAB)";
    if (strcmp(byte_order, "MIXED_BADC") == 0) return "Mixed Byte Swap (BADC)";
    if (strcmp(byte_order, "MIXED_DCBA") == 0) return "Mixed Full Reverse (DCBA)";
    return "Unknown";
}