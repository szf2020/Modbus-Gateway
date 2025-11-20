// modbus.c - Production-ready Modbus communication implementation

#include "modbus.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <time.h>
#include <math.h>

static const char *TAG = "MODBUS";

// Global Variables
static uint16_t response_buffer[MODBUS_MAX_REGISTERS];
static uint8_t response_length = 0;
static QueueHandle_t uart_queue = NULL;
static modbus_stats_t stats = {0};

// Global variable to track current baud rate
static int current_baud_rate = 9600;

// Flag to track if Modbus is already initialized
static bool modbus_initialized = false;

// Function to set baud rate dynamically
esp_err_t modbus_set_baud_rate(int baud_rate)
{
    if (baud_rate == current_baud_rate) {
        // Baud rate already set, no need to change
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "[BAUD] Changing baud rate from %d to %d bps", current_baud_rate, baud_rate);
    
    // Set the new baud rate
    esp_err_t ret = uart_set_baudrate(RS485_UART_PORT, baud_rate);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[ERROR] Failed to set baud rate: %s", esp_err_to_name(ret));
        return ret;
    }
    
    current_baud_rate = baud_rate;
    
    // Small delay to allow UART to stabilize
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Flush UART buffers after baud rate change
    uart_flush(RS485_UART_PORT);
    
    ESP_LOGI(TAG, "[BAUD] Successfully changed baud rate to %d bps", baud_rate);
    return ESP_OK;
}

// Initialize Modbus communication
esp_err_t modbus_init(void)
{
    // Check if already initialized
    if (modbus_initialized) {
        ESP_LOGI(TAG, "[INFO] Modbus already initialized - skipping reinitialization");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "[CONFIG] Initializing Modbus RS485 Communication");
    ESP_LOGI(TAG, "[LOC] Hardware Configuration:");
    ESP_LOGI(TAG, "   * UART Port: UART%d", RS485_UART_PORT);
    ESP_LOGI(TAG, "   * Default Baud Rate: %d bps", RS485_BAUD_RATE);
    ESP_LOGI(TAG, "   * TX Pin: GPIO %d", TXD2);
    ESP_LOGI(TAG, "   * RX Pin: GPIO %d", RXD2);
    ESP_LOGI(TAG, "   * RTS Pin: GPIO %d", RS485_RTS_PIN);
    ESP_LOGI(TAG, "   * Buffer Size: %d bytes", RS485_BUF_SIZE);

    current_baud_rate = RS485_BAUD_RATE;
    
    uart_config_t uart_config = {
        .baud_rate = RS485_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_LOGI(TAG, "[CONF]  Installing UART driver...");
    esp_err_t ret = uart_driver_install(RS485_UART_PORT, RS485_BUF_SIZE * 2, RS485_BUF_SIZE * 2, 20, &uart_queue, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[ERROR] Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "[OK] UART driver installed successfully");

    ESP_LOGI(TAG, "[CONF]  Configuring UART parameters...");
    ret = uart_param_config(RS485_UART_PORT, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[ERROR] Failed to configure UART parameters: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "[OK] UART parameters configured");

    ESP_LOGI(TAG, "[CONF]  Setting UART pins...");
    ret = uart_set_pin(RS485_UART_PORT, TXD2, RXD2, RS485_RTS_PIN, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[ERROR] Failed to set UART pins: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "[OK] UART pins configured");

    ESP_LOGI(TAG, "[CONF]  Setting RS485 half-duplex mode...");
    ret = uart_set_mode(RS485_UART_PORT, UART_MODE_RS485_HALF_DUPLEX);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[ERROR] Failed to set RS485 mode: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "[OK] RS485 half-duplex mode enabled");
    
    ESP_LOGI(TAG, "[DONE] Modbus RS485 initialization complete!");
    ESP_LOGI(TAG, "[INFO] Connection Guide:");
    ESP_LOGI(TAG, "   * Connect RS485 A+ to GPIO %d", TXD2);
    ESP_LOGI(TAG, "   * Connect RS485 B- to GPIO %d", RXD2);
    ESP_LOGI(TAG, "   * Connect RTS to GPIO %d", RS485_RTS_PIN);
    ESP_LOGI(TAG, "   * Ensure common ground connection");
    ESP_LOGI(TAG, "   * Check device baud rate matches %d bps", RS485_BAUD_RATE);

    modbus_reset_statistics();

    // Mark as initialized
    modbus_initialized = true;

    return ESP_OK;
}

// Deinitialize Modbus communication
void modbus_deinit(void)
{
    if (uart_queue != NULL) {
        uart_driver_delete(RS485_UART_PORT);
        uart_queue = NULL;
    }

    // Mark as deinitialized
    modbus_initialized = false;

    ESP_LOGI(TAG, "Modbus deinitialized");
}

// Calculate Modbus CRC16
uint16_t modbus_calculate_crc(const uint8_t* data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// Verify CRC in received data
bool modbus_verify_crc(const uint8_t* data, size_t length)
{
    if (length < 3) return false;
    
    uint16_t calculated_crc = modbus_calculate_crc(data, length - 2);
    uint16_t received_crc = (data[length - 1] << 8) | data[length - 2];
    
    return calculated_crc == received_crc;
}

// Generic Modbus request function
static modbus_result_t modbus_send_request(uint8_t slave_id, uint8_t function_code, 
                                         uint16_t start_addr, uint16_t data, 
                                         uint8_t* response_data, size_t max_response_length)
{
    uint8_t request[8];
    uint8_t response[MODBUS_MAX_BUFFER_SIZE];
    
    stats.total_requests++;
    
    // Build request frame
    request[0] = slave_id;
    request[1] = function_code;
    request[2] = (start_addr >> 8) & 0xFF;
    request[3] = start_addr & 0xFF;
    request[4] = (data >> 8) & 0xFF;
    request[5] = data & 0xFF;
    
    uint16_t crc = modbus_calculate_crc(request, 6);
    request[6] = crc & 0xFF;
    request[7] = (crc >> 8) & 0xFF;
    
    // Clear receive buffer and log request details
    uart_flush_input(RS485_UART_PORT);
    
    ESP_LOGI(TAG, "[SEND] Sending Modbus request to Slave %d: [%02X %02X %02X %02X %02X %02X %02X %02X]",
             slave_id, request[0], request[1], request[2], request[3], 
             request[4], request[5], request[6], request[7]);
    
    // Send request
    int bytes_written = uart_write_bytes(RS485_UART_PORT, request, 8);
    if (bytes_written != 8) {
        ESP_LOGE(TAG, "[ERROR] Failed to send Modbus request - only %d bytes written", bytes_written);
        stats.failed_requests++;
        stats.last_error_code = MODBUS_INVALID_RESPONSE;
        return MODBUS_INVALID_RESPONSE;
    }
    
    ESP_LOGI(TAG, "[OK] Modbus request sent successfully (%d bytes)", bytes_written);
    
    // Wait for transmission complete
    uart_wait_tx_done(RS485_UART_PORT, pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "[WAIT] Waiting for response (timeout: %d ms)...", MODBUS_RESPONSE_TIMEOUT_MS);
    
    // Clear response buffer for safety
    memset(response, 0, sizeof(response));
    
    // Read response with bounds checking
    int response_length = uart_read_bytes(RS485_UART_PORT, response, sizeof(response), 
                                        pdMS_TO_TICKS(MODBUS_RESPONSE_TIMEOUT_MS));
    
    ESP_LOGI(TAG, "[RECV] Received %d bytes from RS485", response_length);
    
    if (response_length > 0) {
        // Log received bytes for debugging
        ESP_LOGI(TAG, "[INFO] Raw response data:");
        for (int i = 0; i < response_length && i < 16; i++) {
            printf("%02X ", response[i]);
        }
        printf("\n");
    }
    
    if (response_length < 5) {
        if (response_length == 0) {
            ESP_LOGE(TAG, "[ERROR] No response from Modbus device (timeout)");
            ESP_LOGE(TAG, "[CONFIG] Troubleshooting:");
            ESP_LOGE(TAG, "   * Check RS485 wiring (A+, B-, GND)");
            ESP_LOGE(TAG, "   * Verify slave ID (%d) is correct", slave_id);
            ESP_LOGE(TAG, "   * Check baud rate (%d bps)", RS485_BAUD_RATE);
            ESP_LOGE(TAG, "   * Ensure device is powered and connected");
        } else {
            ESP_LOGE(TAG, "[ERROR] Invalid response length: %d bytes (minimum 5 required)", response_length);
        }
        stats.failed_requests++;
        stats.timeout_errors++;
        stats.last_error_code = MODBUS_TIMEOUT;
        return MODBUS_TIMEOUT;
    }
    
    // Verify CRC
    if (!modbus_verify_crc(response, response_length)) {
        ESP_LOGE(TAG, "[ERROR] CRC verification failed");
        stats.failed_requests++;
        stats.crc_errors++;
        stats.last_error_code = MODBUS_INVALID_CRC;
        return MODBUS_INVALID_CRC;
    }
    
    // Check for exception response
    if (response[1] & 0x80) {
        uint8_t exception_code = response[2];
        ESP_LOGE(TAG, "[ERROR] Modbus exception: 0x%02X", exception_code);
        stats.failed_requests++;
        stats.last_error_code = exception_code;
        return (modbus_result_t)exception_code;
    }
    
    // Verify response header
    if (response[0] != slave_id || response[1] != function_code) {
        ESP_LOGE(TAG, "[ERROR] Invalid response header (slave: %d vs %d, func: %d vs %d)", 
                 response[0], slave_id, response[1], function_code);
        stats.failed_requests++;
        stats.last_error_code = MODBUS_INVALID_RESPONSE;
        return MODBUS_INVALID_RESPONSE;
    }
    
    // Copy response data
    if (response_data && max_response_length > 0) {
        size_t copy_length = (response_length < max_response_length) ? response_length : max_response_length;
        memcpy(response_data, response, copy_length);
    }
    
    stats.successful_requests++;
    ESP_LOGI(TAG, "[OK] Modbus request successful");
    return MODBUS_SUCCESS;
}

// Read Holding Registers
modbus_result_t modbus_read_holding_registers(uint8_t slave_id, uint16_t start_addr, uint16_t num_regs)
{
    uint8_t response[MODBUS_MAX_BUFFER_SIZE];
    
    ESP_LOGI(TAG, "[READ] Reading %d holding registers from slave %d, starting at 0x%04X", 
             num_regs, slave_id, start_addr);
    
    modbus_result_t result = modbus_send_request(slave_id, MODBUS_READ_HOLDING_REGISTERS, 
                                               start_addr, num_regs, response, sizeof(response));
    
    if (result == MODBUS_SUCCESS) {
        uint8_t byte_count = response[2];
        int num_registers = byte_count / 2;
        
        // Extract register values into response buffer
        for (int i = 0; i < num_registers && i < MODBUS_MAX_REGISTERS; i++) {
            response_buffer[i] = (response[3 + i*2] << 8) | response[4 + i*2];
        }
        
        response_length = num_registers;
        
        ESP_LOGI(TAG, "[OK] Successfully read %d registers", num_registers);
        
        // Log register values for debugging
        for (int i = 0; i < num_registers; i++) {
            ESP_LOGI(TAG, "[DATA] Register[%d]: 0x%04X (%d)", i, response_buffer[i], response_buffer[i]);
        }
    }
    
    return result;
}

// Read Input Registers
modbus_result_t modbus_read_input_registers(uint8_t slave_id, uint16_t start_addr, uint16_t num_regs)
{
    uint8_t response[MODBUS_MAX_BUFFER_SIZE];
    
    ESP_LOGI(TAG, "[READ] Reading %d input registers from slave %d, starting at 0x%04X", 
             num_regs, slave_id, start_addr);
    
    modbus_result_t result = modbus_send_request(slave_id, MODBUS_READ_INPUT_REGISTERS, 
                                               start_addr, num_regs, response, sizeof(response));
    
    if (result == MODBUS_SUCCESS) {
        uint8_t byte_count = response[2];
        int num_registers = byte_count / 2;
        
        // Extract register values into response buffer
        for (int i = 0; i < num_registers && i < MODBUS_MAX_REGISTERS; i++) {
            response_buffer[i] = (response[3 + i*2] << 8) | response[4 + i*2];
        }
        
        response_length = num_registers;
        ESP_LOGI(TAG, "[OK] Successfully read %d input registers", num_registers);
        
        // Log register values for debugging
        for (int i = 0; i < num_registers; i++) {
            ESP_LOGI(TAG, "[DATA] Register[%d]: 0x%04X (%d)", i, response_buffer[i], response_buffer[i]);
        }
    }
    
    return result;
}

// Write Single Register
modbus_result_t modbus_write_single_register(uint8_t slave_id, uint16_t addr, uint16_t value)
{
    ESP_LOGI(TAG, "Writing value 0x%04X to register 0x%04X on slave %d", value, addr, slave_id);
    
    uint8_t response[MODBUS_MAX_BUFFER_SIZE];
    return modbus_send_request(slave_id, MODBUS_WRITE_SINGLE_REGISTER, addr, value, response, sizeof(response));
}

// Write Multiple Registers
modbus_result_t modbus_write_multiple_registers(uint8_t slave_id, uint16_t start_addr, uint16_t num_regs, const uint16_t* values)
{
    if (!values || num_regs == 0 || num_regs > MODBUS_MAX_REGISTERS) {
        ESP_LOGE(TAG, "[ERROR] Invalid parameters for write multiple registers");
        stats.failed_requests++;
        return MODBUS_ILLEGAL_DATA_VALUE;
    }
    
    ESP_LOGI(TAG, "Writing %d registers starting at 0x%04X on slave %d", num_regs, start_addr, slave_id);
    
    // Calculate request length: 1(slave) + 1(func) + 2(addr) + 2(qty) + 1(bytes) + (qty*2)(data) + 2(crc)
    uint8_t byte_count = num_regs * 2;
    uint16_t request_length = 7 + byte_count + 2;
    uint8_t request[MODBUS_MAX_BUFFER_SIZE];
    uint8_t response[MODBUS_MAX_BUFFER_SIZE];
    
    if (request_length > MODBUS_MAX_BUFFER_SIZE) {
        ESP_LOGE(TAG, "[ERROR] Request too large: %d bytes", request_length);
        stats.failed_requests++;
        return MODBUS_ILLEGAL_DATA_VALUE;
    }
    
    stats.total_requests++;
    
    // Build request frame
    request[0] = slave_id;
    request[1] = MODBUS_WRITE_MULTIPLE_REGISTERS;
    request[2] = (start_addr >> 8) & 0xFF;
    request[3] = start_addr & 0xFF;
    request[4] = (num_regs >> 8) & 0xFF;
    request[5] = num_regs & 0xFF;
    request[6] = byte_count;
    
    // Add register values
    for (int i = 0; i < num_regs; i++) {
        request[7 + (i * 2)] = (values[i] >> 8) & 0xFF;
        request[8 + (i * 2)] = values[i] & 0xFF;
        ESP_LOGI(TAG, "Register[%d]: 0x%04X (%d)", i, values[i], values[i]);
    }
    
    // Calculate and add CRC
    uint16_t crc = modbus_calculate_crc(request, request_length - 2);
    request[request_length - 2] = crc & 0xFF;
    request[request_length - 1] = (crc >> 8) & 0xFF;
    
    // Clear receive buffer and log request
    uart_flush_input(RS485_UART_PORT);
    
    ESP_LOGI(TAG, "[SEND] Sending %d-byte write multiple request to Slave %d", request_length, slave_id);
    
    // Send request
    int bytes_written = uart_write_bytes(RS485_UART_PORT, request, request_length);
    if (bytes_written != request_length) {
        ESP_LOGE(TAG, "[ERROR] Failed to send request - only %d/%d bytes written", bytes_written, request_length);
        stats.failed_requests++;
        stats.last_error_code = MODBUS_INVALID_RESPONSE;
        return MODBUS_INVALID_RESPONSE;
    }
    
    ESP_LOGI(TAG, "[OK] Write multiple request sent successfully (%d bytes)", bytes_written);
    
    // Wait for transmission complete
    uart_wait_tx_done(RS485_UART_PORT, pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "[WAIT] Waiting for response (timeout: %d ms)...", MODBUS_RESPONSE_TIMEOUT_MS);
    
    // Clear response buffer for safety
    memset(response, 0, sizeof(response));
    
    // Read response with bounds checking
    int response_length = uart_read_bytes(RS485_UART_PORT, response, sizeof(response), 
                                        pdMS_TO_TICKS(MODBUS_RESPONSE_TIMEOUT_MS));
    
    ESP_LOGI(TAG, "[RECV] Received %d bytes from RS485", response_length);
    
    if (response_length <= 0) {
        ESP_LOGE(TAG, "[ERROR] No response received (timeout)");
        stats.timeout_errors++;
        stats.failed_requests++;
        stats.last_error_code = MODBUS_TIMEOUT;
        return MODBUS_TIMEOUT;
    }
    
    // Log received bytes
    ESP_LOGI(TAG, "[RECV] Response data:");
    for (int i = 0; i < response_length; i++) {
        ESP_LOGI(TAG, "   [%d]: 0x%02X", i, response[i]);
    }
    
    // Check minimum response length (8 bytes for normal response)
    if (response_length < 8) {
        ESP_LOGE(TAG, "[ERROR] Response too short: %d bytes", response_length);
        stats.failed_requests++;
        stats.last_error_code = MODBUS_INVALID_RESPONSE;
        return MODBUS_INVALID_RESPONSE;
    }
    
    // Verify CRC
    if (!modbus_verify_crc(response, response_length)) {
        ESP_LOGE(TAG, "[ERROR] Invalid CRC in response");
        stats.crc_errors++;
        stats.failed_requests++;
        stats.last_error_code = MODBUS_INVALID_CRC;
        return MODBUS_INVALID_CRC;
    }
    
    // Check for error response
    if (response[1] & 0x80) {
        uint8_t error_code = response[2];
        ESP_LOGE(TAG, "[ERROR] Modbus error response: 0x%02X", error_code);
        stats.failed_requests++;
        stats.last_error_code = error_code;
        return (modbus_result_t)error_code;
    }
    
    // Verify response matches request
    if (response[0] != slave_id || response[1] != MODBUS_WRITE_MULTIPLE_REGISTERS) {
        ESP_LOGE(TAG, "[ERROR] Response mismatch - Slave: %d (expected %d), Function: %d (expected %d)",
                 response[0], slave_id, response[1], MODBUS_WRITE_MULTIPLE_REGISTERS);
        stats.failed_requests++;
        stats.last_error_code = MODBUS_INVALID_RESPONSE;
        return MODBUS_INVALID_RESPONSE;
    }
    
    // Extract response data
    uint16_t resp_start_addr = (response[2] << 8) | response[3];
    uint16_t resp_num_regs = (response[4] << 8) | response[5];
    
    if (resp_start_addr != start_addr || resp_num_regs != num_regs) {
        ESP_LOGE(TAG, "[ERROR] Response data mismatch - Addr: %d (expected %d), Qty: %d (expected %d)",
                 resp_start_addr, start_addr, resp_num_regs, num_regs);
        stats.failed_requests++;
        stats.last_error_code = MODBUS_INVALID_RESPONSE;
        return MODBUS_INVALID_RESPONSE;
    }
    
    stats.successful_requests++;
    ESP_LOGI(TAG, "[OK] Successfully wrote %d registers starting at 0x%04X", num_regs, start_addr);
    
    return MODBUS_SUCCESS;
}

// Get Response Buffer Value
uint16_t modbus_get_response_buffer(uint8_t index)
{
    if (index < response_length && index < MODBUS_MAX_REGISTERS) {
        return response_buffer[index];
    }
    ESP_LOGW(TAG, "[WARN] Invalid response buffer index: %d (length: %d)", index, response_length);
    return 0;
}

// Get Response Length
uint8_t modbus_get_response_length(void)
{
    return response_length;
}

// Clear Response Buffer
void modbus_clear_response_buffer(void)
{
    memset(response_buffer, 0, sizeof(response_buffer));
    response_length = 0;
}

// Get Statistics
void modbus_get_statistics(modbus_stats_t* stats_out)
{
    if (stats_out) {
        memcpy(stats_out, &stats, sizeof(modbus_stats_t));
    }
}

// Reset Statistics
void modbus_reset_statistics(void)
{
    memset(&stats, 0, sizeof(modbus_stats_t));
    ESP_LOGI(TAG, "[STATS] Modbus statistics reset");
}

// Flow Meter Data Reading Function
esp_err_t flow_meter_read_data(const meter_config_t* config, flow_meter_data_t* data)
{
    if (!config || !data) {
        ESP_LOGE(TAG, "[ERROR] Invalid parameters for flow meter reading");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "[FLOW] Reading flow meter data: Slave %d, Register 0x%04X, Length %d", 
             config->slave_id, config->register_address, config->register_length);
    
    // Clear previous data
    memset(data, 0, sizeof(flow_meter_data_t));
    
    // Read the registers
    modbus_result_t result = modbus_read_holding_registers(
        config->slave_id, 
        config->register_address, 
        config->register_length
    );
    
    if (result != MODBUS_SUCCESS) {
        ESP_LOGE(TAG, "[ERROR] Failed to read flow meter registers: %d", result);
        data->data_valid = false;
        return ESP_FAIL;
    }
    
    // Get the current time for timestamp
    time_t now;
    struct tm timeinfo;
    time(&now);
    gmtime_r(&now, &timeinfo);
    strftime(data->timestamp, sizeof(data->timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    
    // Process the data based on data type
    if (strcmp(config->data_type, "INT32") == 0 && config->register_length >= 2) {
        // Get the raw register values
        uint16_t reg0 = modbus_get_response_buffer(0);
        uint16_t reg1 = modbus_get_response_buffer(1);
        
        ESP_LOGI(TAG, "[FIND] Raw Register Values: Reg[0]=0x%04X (%d), Reg[1]=0x%04X (%d)", 
                 reg0, reg0, reg1, reg1);
        
        // Try different byte order combinations to find the correct one
        uint32_t raw_value_big_endian = ((uint32_t)reg0 << 16) | reg1;     // Big endian (ABCD)
        uint32_t raw_value_little_endian = ((uint32_t)reg1 << 16) | reg0;  // Little endian (CDAB)
        // Note: mixed byte orders (BADC, DCBA) removed as unused
        
        ESP_LOGI(TAG, "[FIND] Byte Order Analysis:");
        ESP_LOGI(TAG, "   Big Endian (ABCD):    0x%08lX = %lu → %.4f m³", 
                 raw_value_big_endian, raw_value_big_endian, (double)raw_value_big_endian * config->scale_factor);
        
        // Use little endian byte order and process the data
        uint32_t raw_value = raw_value_little_endian;
        data->raw_totalizer = raw_value;
        data->totalizer_value = (double)raw_value * config->scale_factor;
        
    } else if (strcmp(config->data_type, "UINT16") == 0 && config->register_length >= 1) {
        // Single 16-bit register
        uint16_t raw_value = modbus_get_response_buffer(0);
        
        data->raw_totalizer = raw_value;
        data->totalizer_value = (double)raw_value * config->scale_factor;
        
        ESP_LOGI(TAG, "[DATA] UINT16 Raw: %u, Scaled: %.3f (factor: %.3f)", 
                 raw_value, data->totalizer_value, config->scale_factor);
        
    } else if (strcmp(config->data_type, "FLOAT32") == 0 && config->register_length >= 2) {
        // IEEE 754 32-bit float (two 16-bit registers)
        uint32_t raw_bits = ((uint32_t)modbus_get_response_buffer(0) << 16) | 
                           modbus_get_response_buffer(1);
        
        union {
            uint32_t i;
            float f;
        } converter;
        converter.i = raw_bits;
        
        data->totalizer_value = (double)converter.f * config->scale_factor;
        data->raw_totalizer = raw_bits;
        
        ESP_LOGI(TAG, "[DATA] FLOAT32 Raw: %.3f, Scaled: %.3f (factor: %.3f)", 
                 converter.f, data->totalizer_value, config->scale_factor);
        
    } else {
        ESP_LOGE(TAG, "[ERROR] Unsupported data type: %s or insufficient register length: %d", 
                 config->data_type, config->register_length);
        data->data_valid = false;
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    data->last_read_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    data->data_valid = true;
    
    ESP_LOGI(TAG, "[OK] Flow meter reading successful: %.3f %s", 
             data->totalizer_value, config->sensor_type);
    
    return ESP_OK;
}

// Print Flow Meter Data
void flow_meter_print_data(const flow_meter_data_t* data)
{
    if (!data) return;
    
    ESP_LOGI(TAG, "[FLOW] Flow Meter Data:");
    ESP_LOGI(TAG, "   Totalizer: %.3f", data->totalizer_value);
    ESP_LOGI(TAG, "   Raw Value: %lu", data->raw_totalizer);
    ESP_LOGI(TAG, "   Timestamp: %s", data->timestamp);
    ESP_LOGI(TAG, "   Valid: %s", data->data_valid ? "Yes" : "No");
    ESP_LOGI(TAG, "   Last Read: %lu ms", data->last_read_time);
}