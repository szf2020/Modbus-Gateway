#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "ds3231_rtc.h"

static const char *TAG = "DS3231_RTC";

// Hardware pin configuration (hardcoded)
#define RTC_I2C_SDA GPIO_NUM_21
#define RTC_I2C_SCL GPIO_NUM_22
#define RTC_I2C_NUM I2C_NUM_0

// I2C timeout
#define I2C_TIMEOUT_MS 1000

// BCD conversion macros
#define BCD_TO_DEC(val) (((val) / 16 * 10) + ((val) % 16))
#define DEC_TO_BCD(val) (((val) / 10 * 16) + ((val) % 10))

// Initialize I2C master
esp_err_t ds3231_init(void) {
    ESP_LOGI(TAG, "🕐 Initializing DS3231 RTC...");
    ESP_LOGI(TAG, "   I2C SDA: GPIO %d", RTC_I2C_SDA);
    ESP_LOGI(TAG, "   I2C SCL: GPIO %d", RTC_I2C_SCL);

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = RTC_I2C_SDA,
        .scl_io_num = RTC_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,  // 100kHz
    };

    esp_err_t ret = i2c_param_config(RTC_I2C_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure I2C: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(RTC_I2C_NUM, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2C driver: %s", esp_err_to_name(ret));
        return ret;
    }

    // Check if DS3231 is responding
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(RTC_I2C_NUM, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ DS3231 not responding! Check wiring:");
        ESP_LOGE(TAG, "   - SDA connected to GPIO %d?", RTC_I2C_SDA);
        ESP_LOGE(TAG, "   - SCL connected to GPIO %d?", RTC_I2C_SCL);
        ESP_LOGE(TAG, "   - VCC connected to 3.3V?");
        ESP_LOGE(TAG, "   - GND connected?");
        ESP_LOGE(TAG, "   - Battery installed in DS3231?");
        return ret;
    }

    ESP_LOGI(TAG, "✅ DS3231 RTC initialized successfully");
    return ESP_OK;
}

// Deinitialize I2C
esp_err_t ds3231_deinit(void) {
    return i2c_driver_delete(RTC_I2C_NUM);
}

// Read register from DS3231
static esp_err_t ds3231_read_reg(uint8_t reg_addr, uint8_t* data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(RTC_I2C_NUM, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    return ret;
}

// Write register to DS3231
static esp_err_t ds3231_write_reg(uint8_t reg_addr, uint8_t* data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(RTC_I2C_NUM, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    return ret;
}

// Get time from DS3231 as struct tm
esp_err_t ds3231_get_time_tm(struct tm* timeinfo) {
    if (timeinfo == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[7];
    esp_err_t ret = ds3231_read_reg(DS3231_REG_SECONDS, data, 7);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read time from DS3231");
        return ret;
    }

    timeinfo->tm_sec = BCD_TO_DEC(data[0] & 0x7F);
    timeinfo->tm_min = BCD_TO_DEC(data[1] & 0x7F);
    timeinfo->tm_hour = BCD_TO_DEC(data[2] & 0x3F);  // 24-hour format
    timeinfo->tm_wday = BCD_TO_DEC(data[3] & 0x07) - 1;  // Sunday = 0
    timeinfo->tm_mday = BCD_TO_DEC(data[4] & 0x3F);
    timeinfo->tm_mon = BCD_TO_DEC(data[5] & 0x1F) - 1;  // January = 0
    timeinfo->tm_year = BCD_TO_DEC(data[6]) + 100;  // Years since 1900

    // Handle century bit
    if (data[5] & 0x80) {
        timeinfo->tm_year += 100;  // 21st century
    }

    timeinfo->tm_isdst = -1;  // Let system determine DST

    return ESP_OK;
}

// Set time to DS3231 from struct tm
esp_err_t ds3231_set_time_tm(const struct tm* timeinfo) {
    if (timeinfo == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[7];

    data[0] = DEC_TO_BCD(timeinfo->tm_sec);
    data[1] = DEC_TO_BCD(timeinfo->tm_min);
    data[2] = DEC_TO_BCD(timeinfo->tm_hour);  // 24-hour format
    data[3] = DEC_TO_BCD(timeinfo->tm_wday + 1);  // Sunday = 1 in DS3231
    data[4] = DEC_TO_BCD(timeinfo->tm_mday);

    int year = timeinfo->tm_year - 100;  // Years since 2000
    uint8_t century = 0;
    if (year >= 100) {
        century = 0x80;  // Century bit
        year -= 100;
    }

    data[5] = DEC_TO_BCD(timeinfo->tm_mon + 1) | century;  // January = 1 in DS3231
    data[6] = DEC_TO_BCD(year);

    esp_err_t ret = ds3231_write_reg(DS3231_REG_SECONDS, data, 7);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write time to DS3231");
        return ret;
    }

    ESP_LOGI(TAG, "✅ Time set to DS3231: %04d-%02d-%02d %02d:%02d:%02d",
             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    return ESP_OK;
}

// Get time as time_t
esp_err_t ds3231_get_time(time_t* time) {
    if (time == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    struct tm timeinfo;
    esp_err_t ret = ds3231_get_time_tm(&timeinfo);
    if (ret != ESP_OK) {
        return ret;
    }

    *time = mktime(&timeinfo);
    return ESP_OK;
}

// Set time from time_t
esp_err_t ds3231_set_time(time_t time) {
    struct tm timeinfo;
    gmtime_r(&time, &timeinfo);
    return ds3231_set_time_tm(&timeinfo);
}

// Get temperature from DS3231
esp_err_t ds3231_get_temperature(float* temp) {
    if (temp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[2];
    esp_err_t ret = ds3231_read_reg(DS3231_REG_TEMP_MSB, data, 2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read temperature from DS3231");
        return ret;
    }

    int16_t temp_raw = (data[0] << 8) | data[1];
    *temp = temp_raw / 256.0f;

    return ESP_OK;
}

// Sync system time from RTC
esp_err_t ds3231_sync_system_time(void) {
    struct tm timeinfo;
    esp_err_t ret = ds3231_get_time_tm(&timeinfo);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read time from RTC");
        return ret;
    }

    time_t now = mktime(&timeinfo);
    struct timeval tv = {
        .tv_sec = now,
        .tv_usec = 0
    };

    ret = settimeofday(&tv, NULL);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to set system time");
        return ESP_FAIL;
    }

    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

    ESP_LOGI(TAG, "+========================================+");
    ESP_LOGI(TAG, "|   ✅ SYSTEM TIME SYNCED FROM RTC!     |");
    ESP_LOGI(TAG, "+========================================+");
    ESP_LOGI(TAG, "📅 Current UTC time: %s", time_str);
    ESP_LOGI(TAG, "🕐 Unix timestamp: %ld", (long)now);

    return ESP_OK;
}

// Update RTC from system time (after NTP sync)
esp_err_t ds3231_update_from_system_time(void) {
    time_t now;
    time(&now);

    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    esp_err_t ret = ds3231_set_time_tm(&timeinfo);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update RTC from system time");
        return ret;
    }

    ESP_LOGI(TAG, "✅ RTC updated from system time (NTP sync)");
    return ESP_OK;
}
