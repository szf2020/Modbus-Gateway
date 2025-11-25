#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <dirent.h>
#include <errno.h>
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sd_card_logger.h"

static const char *TAG = "SD_CARD";

// Hardware pin configuration (hardcoded)
#define SD_CARD_MOSI GPIO_NUM_13
#define SD_CARD_MISO GPIO_NUM_12
#define SD_CARD_CLK GPIO_NUM_14
#define SD_CARD_CS GPIO_NUM_5
#define SD_CARD_SPI_HOST SPI2_HOST

// SD Card state
static bool sd_initialized = false;
static bool sd_available = false;
static sdmmc_card_t *card = NULL;
static uint32_t message_id_counter = 0;
static const char* mount_point = "/sdcard";
// Use 8.3 short filenames for maximum FAT compatibility
static const char* pending_messages_file = "/sdcard/msgs.txt";  // Even shorter
static const char* temp_messages_file = "/sdcard/tmp.txt";  // Even shorter

// Minimum free space required (in bytes) - 1MB
#define MIN_FREE_SPACE_BYTES (1024 * 1024)

// Maximum message file size (10MB)
#define MAX_MESSAGE_FILE_SIZE (10 * 1024 * 1024)

// Initialize SD card with SPI interface
esp_err_t sd_card_init(void) {
    if (sd_initialized) {
        ESP_LOGW(TAG, "SD card already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "🔧 Initializing SD Card on SPI...");
    ESP_LOGI(TAG, "📍 Pin Configuration:");
    ESP_LOGI(TAG, "   CS:   GPIO %d", SD_CARD_CS);
    ESP_LOGI(TAG, "   MOSI: GPIO %d", SD_CARD_MOSI);
    ESP_LOGI(TAG, "   MISO: GPIO %d", SD_CARD_MISO);
    ESP_LOGI(TAG, "   CLK:  GPIO %d", SD_CARD_CLK);
    ESP_LOGI(TAG, "   Host: SPI%d", SD_CARD_SPI_HOST + 1);

    // Enable internal pull-ups on SPI lines first
    ESP_LOGI(TAG, "🔌 Enabling pull-up resistors...");
    gpio_set_pull_mode(SD_CARD_MISO, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SD_CARD_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SD_CARD_CLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SD_CARD_CS, GPIO_PULLUP_ONLY);

    // Configure SPI bus
    ESP_LOGI(TAG, "📡 Configuring SPI bus...");
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_CARD_MOSI,
        .miso_io_num = SD_CARD_MISO,
        .sclk_io_num = SD_CARD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
        .flags = SPICOMMON_BUSFLAG_MASTER,
    };

    ESP_LOGI(TAG, "🚀 Initializing SPI bus...");
    esp_err_t ret = spi_bus_initialize(SD_CARD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // CRITICAL: Add delay after SPI initialization (matching Arduino code line 158)
    ESP_LOGI(TAG, "⏳ Waiting for SPI bus to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(1000));  // 1 second delay like Arduino

    // Configure SD host and slot
    ESP_LOGI(TAG, "⚙️ Configuring SD host...");
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_CARD_SPI_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CARD_CS;
    slot_config.host_id = SD_CARD_SPI_HOST;

    // Use 1MHz like the working Arduino example (not too slow, not too fast)
    host.max_freq_khz = 1000;  // 1MHz - same as working Arduino code
    ESP_LOGI(TAG, "📶 SPI Frequency: %d kHz (matching working Arduino implementation)", host.max_freq_khz);

    // Mount FAT filesystem
    ESP_LOGI(TAG, "💾 Mounting FAT filesystem...");
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,  // Auto-format if needed (like Arduino SD library)
        .max_files = 5,
        .allocation_unit_size = 0  // Use default allocation unit (let FAT decide)
    };

    ESP_LOGI(TAG, "🔍 Attempting to detect and initialize SD card...");
    ESP_LOGI(TAG, "   (This may take a few seconds)");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize SD card: %s (0x%x)", esp_err_to_name(ret), ret);

        // Provide specific troubleshooting advice based on error
        if (ret == ESP_ERR_TIMEOUT || ret == 0x108) {
            ESP_LOGE(TAG, "");
            ESP_LOGE(TAG, "+========================================================+");
            ESP_LOGE(TAG, "|  SD CARD NOT RESPONDING - Check the following:        |");
            ESP_LOGE(TAG, "+========================================================+");
            ESP_LOGE(TAG, "");
            ESP_LOGE(TAG, "1. ✓ Is SD card inserted properly?");
            ESP_LOGE(TAG, "2. ✓ Is SD card formatted as FAT32?");
            ESP_LOGE(TAG, "3. ✓ Check wiring connections:");
            ESP_LOGE(TAG, "     CS:   GPIO %d → SD Card CS pin", SD_CARD_CS);
            ESP_LOGE(TAG, "     MOSI: GPIO %d → SD Card MOSI/DI pin", SD_CARD_MOSI);
            ESP_LOGE(TAG, "     MISO: GPIO %d → SD Card MISO/DO pin", SD_CARD_MISO);
            ESP_LOGE(TAG, "     CLK:  GPIO %d → SD Card CLK/SCK pin", SD_CARD_CLK);
            ESP_LOGE(TAG, "     VCC:  3.3V (NOT 5V!)");
            ESP_LOGE(TAG, "     GND:  GND");
            ESP_LOGE(TAG, "4. ✓ Try a different SD card");
            ESP_LOGE(TAG, "5. ✓ Check if SD card works in computer");
            ESP_LOGE(TAG, "");
            ESP_LOGE(TAG, "💡 System will continue WITHOUT SD card logging");
            ESP_LOGE(TAG, "");
        } else if (ret == ESP_ERR_INVALID_CRC || ret == 0x109) {
            ESP_LOGE(TAG, "");
            ESP_LOGE(TAG, "+========================================================+");
            ESP_LOGE(TAG, "|  SD CARD CRC ERROR - Data Corruption Detected         |");
            ESP_LOGE(TAG, "+========================================================+");
            ESP_LOGE(TAG, "");
            ESP_LOGE(TAG, "The SD card is responding but data is corrupted.");
            ESP_LOGE(TAG, "");
            ESP_LOGE(TAG, "Most likely causes:");
            ESP_LOGE(TAG, "  1. ⚠️ BAD/FAULTY SD CARD - Try a different card!");
            ESP_LOGE(TAG, "  2. ⚠️ Poor wiring - Check for loose connections");
            ESP_LOGE(TAG, "  3. ⚠️ Electrical interference - Keep wires short");
            ESP_LOGE(TAG, "  4. ⚠️ Card not fully inserted");
            ESP_LOGE(TAG, "");
            ESP_LOGE(TAG, "Recommended SD cards:");
            ESP_LOGE(TAG, "  * SanDisk, Samsung, or Kingston brand");
            ESP_LOGE(TAG, "  * 2GB - 16GB size");
            ESP_LOGE(TAG, "  * Class 4 or Class 10");
            ESP_LOGE(TAG, "  * Formatted as FAT32");
            ESP_LOGE(TAG, "");
            ESP_LOGE(TAG, "💡 System will continue WITHOUT SD card logging");
            ESP_LOGE(TAG, "");
        } else if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "❌ Failed to mount filesystem - card may not be formatted");
        }

        sd_initialized = false;
        sd_available = false;
        return ret;
    }

    sd_initialized = true;
    sd_available = true;

    // Print card info
    ESP_LOGI(TAG, "✅ SD Card initialized successfully");
    ESP_LOGI(TAG, "📋 Card Info:");
    ESP_LOGI(TAG, "   Name: %s", card->cid.name);

    // Determine card type based on capacity
    const char* card_type = "SDSC";
    if (card->is_sdio) {
        card_type = "SDIO";
    } else if (card->is_mmc) {
        card_type = "MMC";
    } else if (card->ocr & (1 << 30)) {  // CCS bit indicates SDHC/SDXC
        card_type = "SDHC/SDXC";
    }

    ESP_LOGI(TAG, "   Type: %s", card_type);
    ESP_LOGI(TAG, "   Speed: %s", (card->csd.tr_speed > 25000000) ? "High Speed" : "Default Speed");
    ESP_LOGI(TAG, "   Size: %lluMB", ((uint64_t) card->csd.capacity) * card->csd.sector_size / (1024 * 1024));

    // Restore message ID counter
    sd_card_restore_message_counter();

    // Simple write test (matching Arduino approach)
    ESP_LOGI(TAG, "🧪 Testing SD card write capability...");
    const char* test_file = "/sdcard/test.txt";

    FILE* test_fp = fopen(test_file, "w");
    if (test_fp == NULL) {
        ESP_LOGE(TAG, "❌ Failed to open test file for writing");
        ESP_LOGE(TAG, "   errno: %d (%s)", errno, strerror(errno));
        ESP_LOGE(TAG, "");
        ESP_LOGE(TAG, "💡 TIP: Reformat SD card as FAT32 and try again");
        ESP_LOGE(TAG, "");
        sd_available = false;
        return ESP_FAIL;
    }

    fprintf(test_fp, "SD card test\n");
    fclose(test_fp);

    // Verify and clean up
    struct stat test_st;
    if (stat(test_file, &test_st) == 0) {
        ESP_LOGI(TAG, "✅ SD card write test successful! (test file: %ld bytes)", test_st.st_size);
        unlink(test_file);
    } else {
        ESP_LOGE(TAG, "❌ Test file was not created!");
        sd_available = false;
        return ESP_FAIL;
    }

    return ESP_OK;
}

// Deinitialize SD card
esp_err_t sd_card_deinit(void) {
    if (!sd_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(mount_point, card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
        return ret;
    }

    spi_bus_free(SD_CARD_SPI_HOST);

    sd_initialized = false;
    sd_available = false;
    card = NULL;

    ESP_LOGI(TAG, "SD Card deinitialized");
    return ESP_OK;
}

// Check if SD card is available
bool sd_card_is_available(void) {
    return sd_available;
}

// Get SD card status
esp_err_t sd_card_get_status(sd_card_status_t* status) {
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    status->initialized = sd_initialized;
    status->card_available = sd_available;

    if (sd_available && card != NULL) {
        status->card_size_mb = ((uint64_t) card->csd.capacity) * card->csd.sector_size / (1024 * 1024);

        // Get free space using FATFS
        FATFS *fs;
        DWORD fre_clust;
        if (f_getfree("0:", &fre_clust, &fs) == FR_OK) {
            uint64_t free_bytes = (uint64_t)fre_clust * fs->csize * fs->ssize;
            status->free_space_mb = free_bytes / (1024 * 1024);
        } else {
            status->free_space_mb = 0;
        }
    } else {
        status->card_size_mb = 0;
        status->free_space_mb = 0;
    }

    return ESP_OK;
}

// Check if enough space is available
esp_err_t sd_card_check_space(uint64_t required_bytes) {
    if (!sd_available) {
        return ESP_ERR_INVALID_STATE;
    }

    FATFS *fs;
    DWORD fre_clust;

    if (f_getfree("0:", &fre_clust, &fs) != FR_OK) {
        ESP_LOGW(TAG, "Failed to get free space");
        return ESP_FAIL;
    }

    uint64_t free_bytes = (uint64_t)fre_clust * fs->csize * fs->ssize;

    if (free_bytes < (required_bytes + MIN_FREE_SPACE_BYTES)) {
        ESP_LOGW(TAG, "⚠️ Insufficient space: %lluKB free, %lluKB required",
                 free_bytes / 1024, (required_bytes + MIN_FREE_SPACE_BYTES) / 1024);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

// Save message to SD card (matching Arduino approach with added reliability)
esp_err_t sd_card_save_message(const char* topic, const char* payload, const char* timestamp) {
    if (!sd_available) {
        ESP_LOGW(TAG, "SD card not available for logging");
        return ESP_ERR_INVALID_STATE;
    }

    if (topic == NULL || payload == NULL || timestamp == NULL) {
        ESP_LOGE(TAG, "Invalid message parameters");
        return ESP_ERR_INVALID_ARG;
    }

    // Validate message sizes (matching Arduino limits: topic=100, payload=200)
    if (strlen(topic) > 128 || strlen(payload) > 512) {
        ESP_LOGE(TAG, "Message too large to save");
        return ESP_ERR_INVALID_SIZE;
    }

    // Quick filesystem health check (verify mount point exists)
    struct stat mount_st;
    if (stat(mount_point, &mount_st) != 0) {
        ESP_LOGE(TAG, "❌ Mount point %s no longer exists! SD card may have been unmounted", mount_point);
        sd_available = false;
        return ESP_ERR_INVALID_STATE;
    }

    // Try a simple test file first to verify filesystem is still writable
    // Use same naming pattern as actual file (no dot prefix)
    const char* test_file = "/sdcard/test.txt";
    FILE *test_fp = fopen(test_file, "w");
    if (test_fp == NULL) {
        ESP_LOGE(TAG, "❌ Filesystem health check failed! errno: %d (%s)", errno, strerror(errno));
        ESP_LOGW(TAG, "⚠️ Attempting to recover SD card filesystem...");

        // Try to unmount and remount
        if (card != NULL) {
            ESP_LOGI(TAG, "Unmounting SD card...");
            esp_vfs_fat_sdcard_unmount(mount_point, card);
            card = NULL;
        }

        sd_available = false;
        sd_initialized = false;
        vTaskDelay(pdMS_TO_TICKS(500));  // Wait for cleanup

        // Attempt reinit
        ESP_LOGI(TAG, "Attempting to reinitialize SD card...");
        if (sd_card_init() == ESP_OK) {
            ESP_LOGI(TAG, "✅ SD card reinitialized successfully");
            // Retry test file after reinit
            test_fp = fopen(test_file, "w");
        }

        if (test_fp == NULL) {
            ESP_LOGE(TAG, "❌ SD card recovery failed, cannot save message");
            return ESP_FAIL;
        }
    }

    // Clean up test file
    fprintf(test_fp, "ok\n");
    fclose(test_fp);
    unlink(test_file);

    // Now try to open the actual message file
    ESP_LOGI(TAG, "Opening message file: %s", pending_messages_file);
    FILE *file = fopen(pending_messages_file, "a");
    ESP_LOGI(TAG, "fopen() result: %s (errno: %d)", file ? "SUCCESS" : "FAILED", errno);

    // If append fails with EINVAL, try creating file explicitly first
    if (file == NULL && errno == EINVAL) {
        ESP_LOGW(TAG, "Append mode failed, trying to create file first...");

        // Create file if it doesn't exist
        FILE *create_file = fopen(pending_messages_file, "w");
        if (create_file) {
            fclose(create_file);
            ESP_LOGI(TAG, "File created, retrying append...");

            // Retry append
            file = fopen(pending_messages_file, "a");
        } else {
            ESP_LOGE(TAG, "Failed to create file, errno: %d (%s)", errno, strerror(errno));
        }
    }

    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open pending messages file for writing");
        ESP_LOGE(TAG, "errno: %d (%s)", errno, strerror(errno));
        ESP_LOGE(TAG, "File path: %s", pending_messages_file);
        return ESP_FAIL;
    }

    // Increment message ID and write (matching Arduino format)
    message_id_counter++;
    int written = fprintf(file, "%lu|%s|%s|%s\n", message_id_counter, timestamp, topic, payload);
    fflush(file);  // Force write to disk
    fclose(file);

    if (written > 0) {
        ESP_LOGI(TAG, "💾 Message saved to SD card with ID: %lu (%d bytes)", message_id_counter, written);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to write message to file");
        return ESP_FAIL;
    }
}

// Get count of pending messages
esp_err_t sd_card_get_pending_count(uint32_t* count) {
    if (count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sd_available) {
        *count = 0;
        return ESP_ERR_INVALID_STATE;
    }

    FILE *file = fopen(pending_messages_file, "r");
    if (file == NULL) {
        *count = 0;
        return ESP_OK; // No file means no messages
    }

    uint32_t msg_count = 0;
    char line[700];

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strlen(line) > 1) { // Skip empty lines
            msg_count++;
        }
    }

    fclose(file);
    *count = msg_count;

    return ESP_OK;
}

// Replay pending messages with callback
esp_err_t sd_card_replay_messages(void (*publish_callback)(const pending_message_t* msg)) {
    if (!sd_available) {
        ESP_LOGW(TAG, "SD card not available");
        return ESP_ERR_INVALID_STATE;
    }

    if (publish_callback == NULL) {
        ESP_LOGE(TAG, "Invalid callback function");
        return ESP_ERR_INVALID_ARG;
    }

    FILE *file = fopen(pending_messages_file, "r");
    if (file == NULL) {
        ESP_LOGI(TAG, "No pending messages file found");
        return ESP_OK;
    }

    uint32_t total_messages = 0;
    sd_card_get_pending_count(&total_messages);

    if (total_messages == 0) {
        ESP_LOGI(TAG, "No messages to replay");
        fclose(file);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "📤 Found %lu pending messages to replay", total_messages);

    char line[700];
    uint32_t replayed_count = 0;
    const uint32_t MAX_REPLAY_BATCH = 20; // Limit messages per batch

    while (fgets(line, sizeof(line), file) != NULL && replayed_count < MAX_REPLAY_BATCH) {
        // Remove newline
        line[strcspn(line, "\r\n")] = 0;

        if (strlen(line) < 10) {
            continue; // Skip invalid lines
        }

        // Parse line: ID|TIMESTAMP|TOPIC|PAYLOAD
        char *id_str = strtok(line, "|");
        char *timestamp = strtok(NULL, "|");
        char *topic = strtok(NULL, "|");
        // Get everything after the 3rd pipe as payload (handles any content in payload)
        char *payload = topic ? (topic + strlen(topic) + 1) : NULL;

        // Validate payload isn't empty
        if (payload && *payload == '\0') {
            payload = NULL;
        }

        if (id_str == NULL || timestamp == NULL || topic == NULL || payload == NULL) {
            ESP_LOGW(TAG, "Malformed message line, skipping (ID=%s, TS=%s, Topic=%s, Payload=%s)",
                     id_str ? id_str : "NULL",
                     timestamp ? timestamp : "NULL",
                     topic ? topic : "NULL",
                     payload ? "NULL" : "empty");
            continue;
        }

        // Additional validation: check timestamp isn't empty
        if (*timestamp == '\0') {
            ESP_LOGW(TAG, "Malformed message line, skipping (empty timestamp)");
            continue;
        }

        // Validate timestamp - skip messages from 1970 (invalid RTC time)
        if (strncmp(timestamp, "1970-", 5) == 0) {
            ESP_LOGW(TAG, "⏭️ Skipping message ID %s - invalid timestamp from 1970 (RTC was not set)", id_str);
            continue;
        }

        // Validate topic - skip messages with placeholder device IDs
        if (strstr(topic, "your-device-id") != NULL) {
            ESP_LOGW(TAG, "⏭️ Skipping message ID %s - invalid topic with placeholder device ID", id_str);
            continue;
        }

        pending_message_t msg;
        msg.message_id = atoi(id_str);
        strncpy(msg.timestamp, timestamp, sizeof(msg.timestamp) - 1);
        strncpy(msg.topic, topic, sizeof(msg.topic) - 1);
        strncpy(msg.payload, payload, sizeof(msg.payload) - 1);

        ESP_LOGI(TAG, "📤 Replaying message ID: %lu from %s", msg.message_id, msg.timestamp);

        // Call publish callback
        publish_callback(&msg);
        replayed_count++;

        // Small delay to avoid overwhelming the network
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    fclose(file);

    ESP_LOGI(TAG, "✅ Replayed %lu messages", replayed_count);
    return ESP_OK;
}

// Remove a specific message by ID
esp_err_t sd_card_remove_message(uint32_t message_id) {
    if (!sd_available || message_id == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    FILE *source_file = fopen(pending_messages_file, "r");
    if (source_file == NULL) {
        ESP_LOGW(TAG, "No pending messages file to clean");
        return ESP_OK;
    }

    FILE *temp_file = fopen(temp_messages_file, "w");
    if (temp_file == NULL) {
        ESP_LOGE(TAG, "Failed to create temp file");
        fclose(source_file);
        return ESP_FAIL;
    }

    bool message_found = false;
    char line[700];

    while (fgets(line, sizeof(line), source_file) != NULL) {
        if (strlen(line) < 10) {
            continue;
        }

        // Extract message ID from line
        uint32_t line_msg_id = atoi(line);

        if (line_msg_id != message_id) {
            fputs(line, temp_file);
        } else {
            message_found = true;
            ESP_LOGI(TAG, "🗑️ Removing published message ID: %lu", message_id);
        }
    }

    fflush(temp_file);  // Ensure all data is written to disk
    fclose(source_file);
    fclose(temp_file);

    if (message_found) {
        // Remove old file
        if (remove(pending_messages_file) != 0) {
            ESP_LOGE(TAG, "❌ Failed to remove old messages file: %s", strerror(errno));
            remove(temp_messages_file);  // Clean up temp file
            return ESP_FAIL;
        }

        // Rename temp file to original
        if (rename(temp_messages_file, pending_messages_file) != 0) {
            ESP_LOGE(TAG, "❌ Failed to rename temp file: %s", strerror(errno));
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "✅ Successfully removed message ID %lu from SD card", message_id);
    } else {
        ESP_LOGW(TAG, "⚠️ Message ID %lu not found in SD card", message_id);
        remove(temp_messages_file);
    }

    return ESP_OK;
}

// Clear all pending messages
esp_err_t sd_card_clear_all_messages(void) {
    if (!sd_available) {
        return ESP_ERR_INVALID_STATE;
    }

    if (remove(pending_messages_file) == 0) {
        ESP_LOGI(TAG, "✅ All pending messages cleared");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "No messages file to clear");
        return ESP_OK; // Not an error if file doesn't exist
    }
}

// Get next message ID
uint32_t sd_card_get_next_message_id(void) {
    return message_id_counter + 1;
}

// Restore message ID counter from existing messages
esp_err_t sd_card_restore_message_counter(void) {
    if (!sd_available) {
        return ESP_ERR_INVALID_STATE;
    }

    FILE *file = fopen(pending_messages_file, "r");
    if (file == NULL) {
        ESP_LOGI(TAG, "No existing messages file - starting with ID counter 0");
        message_id_counter = 0;
        return ESP_OK;
    }

    uint32_t max_id = 0;
    uint32_t message_count = 0;
    char line[700];

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strlen(line) < 10) {
            continue;
        }

        uint32_t msg_id = atoi(line);
        if (msg_id > max_id) {
            max_id = msg_id;
        }
        message_count++;
    }

    fclose(file);

    message_id_counter = max_id;
    ESP_LOGI(TAG, "📋 Restored message ID counter to: %lu", message_id_counter);
    ESP_LOGI(TAG, "📋 Found %lu existing messages on SD card", message_count);

    return ESP_OK;
}
