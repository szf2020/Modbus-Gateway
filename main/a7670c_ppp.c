#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "esp_netif_ppp.h"
#include "lwip/sockets.h"
#include "netif/ppp/pppapi.h"
#include "a7670c_ppp.h"

static const char *TAG = "A7670C_PPP";

ESP_EVENT_DEFINE_BASE(PPP_EVENT);

// PPP state
static esp_netif_t *ppp_netif = NULL;
static bool ppp_connected = false;
static ppp_config_t modem_config;
static EventGroupHandle_t ppp_event_group;
static const int PPP_CONNECTED_BIT = BIT0;

// UART RX task control
static TaskHandle_t uart_rx_task_handle = NULL;
static volatile bool uart_rx_task_running = false;

// Signal strength storage (checked before entering PPP mode)
static signal_strength_t current_signal = {0};
static bool signal_checked = false;

// Modem initialization failure tracking
static uint8_t modem_init_failures = 0;
static uint8_t modem_power_cycle_count = 0;  // Track number of power cycles performed
#define MAX_MODEM_INIT_FAILURES 3  // Trigger modem restart after 3 consecutive failures

// Get retry delay based on power cycle count (exponential backoff)
static uint32_t get_retry_delay_ms(void) {
    // After power cycle: 30s, 60s, 120s, 180s, max 180s
    if (modem_power_cycle_count == 0) {
        return 10000;  // Normal retry: 10 seconds
    } else if (modem_power_cycle_count == 1) {
        return 30000;  // After 1st power cycle: 30 seconds
    } else if (modem_power_cycle_count == 2) {
        return 60000;  // After 2nd power cycle: 60 seconds
    } else if (modem_power_cycle_count == 3) {
        return 120000; // After 3rd power cycle: 120 seconds
    } else {
        return 180000; // After 4+ power cycles: 180 seconds (3 minutes)
    }
}

// UART buffers
static uint8_t uart_rx_buffer[2048];

// Send AT command and wait for response
static esp_err_t send_at_command(const char* cmd, const char* expected, int timeout_ms) {
    char response[1024] = {0};

    ESP_LOGI(TAG, ">>> %s", cmd);

    // Send command
    char cmd_with_crlf[256];
    snprintf(cmd_with_crlf, sizeof(cmd_with_crlf), "%s\r\n", cmd);
    uart_write_bytes(modem_config.uart_num, cmd_with_crlf, strlen(cmd_with_crlf));

    // Wait for response
    int len = 0;
    int total_len = 0;
    int64_t start_time = esp_timer_get_time() / 1000;

    while ((esp_timer_get_time() / 1000 - start_time) < timeout_ms) {
        len = uart_read_bytes(modem_config.uart_num, uart_rx_buffer, sizeof(uart_rx_buffer),
                             pdMS_TO_TICKS(100));
        if (len > 0) {
            if (total_len + len < sizeof(response)) {
                memcpy(response + total_len, uart_rx_buffer, len);
                total_len += len;
                response[total_len] = '\0';

                if (expected && strstr(response, expected)) {
                    ESP_LOGI(TAG, "<<< %s", response);
                    return ESP_OK;
                }

                if (strstr(response, "ERROR")) {
                    ESP_LOGE(TAG, "Error response: %s", response);
                    return ESP_FAIL;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGE(TAG, "Timeout waiting for: %s", expected ? expected : "response");
    if (total_len > 0) {
        ESP_LOGW(TAG, "Actual response received: %s", response);
    } else {
        ESP_LOGW(TAG, "No response received from modem");
    }
    return ESP_ERR_TIMEOUT;
}

// Hardware reset modem using combined power cycle + RESET pin (for SIM re-detection)
static esp_err_t hardware_reset_modem(void) {
    ESP_LOGI(TAG, "🔄 Performing complete modem reset (power + hardware reset)...");

    // Step 1: Power off modem using PWR_PIN
    ESP_LOGI(TAG, "   Step 1: Powering OFF modem...");
    gpio_set_level(modem_config.pwr_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));  // Hold LOW for 2 seconds to power off
    gpio_set_level(modem_config.pwr_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(3000));  // Wait for complete power down

    // Step 2: Assert hardware RESET
    ESP_LOGI(TAG, "   Step 2: Asserting hardware RESET (LOW)...");
    gpio_set_level(modem_config.reset_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(500));  // Hold reset for 500ms (extended from 200ms)

    // Step 3: Release RESET and power on modem
    ESP_LOGI(TAG, "   Step 3: Releasing RESET and powering ON modem...");
    gpio_set_level(modem_config.reset_pin, 1);  // Release RESET
    vTaskDelay(pdMS_TO_TICKS(500));

    // Power on modem (pulse PWR_PIN)
    gpio_set_level(modem_config.pwr_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(1500));
    gpio_set_level(modem_config.pwr_pin, 1);

    // Step 4: Wait for complete boot + SIM detection (increased from 15s to 30s)
    ESP_LOGI(TAG, "   Step 4: Waiting 30 seconds for modem boot + SIM detection...");
    for (int i = 30; i > 0; i -= 5) {
        ESP_LOGI(TAG, "      %d seconds remaining...", i);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    ESP_LOGI(TAG, "✅ Complete modem reset finished");
    return ESP_OK;
}

// Power on modem
static esp_err_t power_on_modem(void) {
    ESP_LOGI(TAG, "🔌 Powering on modem...");
    ESP_LOGI(TAG, "   PWR Pin: GPIO %d", modem_config.pwr_pin);
    ESP_LOGI(TAG, "   UART TX: GPIO %d", modem_config.tx_pin);
    ESP_LOGI(TAG, "   UART RX: GPIO %d", modem_config.rx_pin);
    ESP_LOGI(TAG, "   Baud Rate: %d", modem_config.baud_rate);

    // Power key pulse: LOW for 1.5s, then HIGH
    ESP_LOGI(TAG, "   Sending power-on pulse (LOW for 1.5s)...");
    gpio_set_level(modem_config.pwr_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(1500));
    gpio_set_level(modem_config.pwr_pin, 1);
    ESP_LOGI(TAG, "   Power pulse sent (now HIGH)");

    // Wait 8 seconds for modem to fully boot
    ESP_LOGI(TAG, "   Waiting 8 seconds for modem to boot...");
    for (int i = 8; i > 0; i -= 2) {
        ESP_LOGI(TAG, "      %d seconds remaining...", i);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    ESP_LOGI(TAG, "   Boot wait complete, modem should be ready");
    return ESP_OK;
}

// PPP transmit callback - sends data from PPP stack to modem via UART
static esp_err_t ppp_output_callback(void *ctx, void *data, size_t len) {
    uart_write_bytes(modem_config.uart_num, (const char*)data, len);
    return ESP_OK;
}

// UART receive task - feeds data from modem to PPP stack
static void uart_rx_task(void *pvParameters) {
    uint8_t *data = (uint8_t*)malloc(2048);
    uart_rx_task_running = true;

    ESP_LOGI(TAG, "UART RX task started");

    while (uart_rx_task_running) {
        int len = uart_read_bytes(modem_config.uart_num, data, 2048, pdMS_TO_TICKS(100));
        if (len > 0 && ppp_netif) {
            // Feed received data to PPP stack
            esp_netif_receive(ppp_netif, data, len, NULL);
        }
    }

    free(data);
    ESP_LOGI(TAG, "UART RX task stopped");
    uart_rx_task_handle = NULL;
    vTaskDelete(NULL);
}

// Initialize modem for PPP
static esp_err_t init_modem_for_ppp(void) {
    // Test communication with retry logic (modem might be in unknown state after reboot)
    ESP_LOGI(TAG, "🔍 Testing modem communication...");

    int retry_count = 0;
    esp_err_t ret = ESP_FAIL;

    // Try multiple times with increasing delays - modem might be already on or in PPP mode
    for (retry_count = 0; retry_count < 3 && ret != ESP_OK; retry_count++) {
        if (retry_count > 0) {
            ESP_LOGW(TAG, "   Retry %d/3: Sending attention command...", retry_count);
            // Send multiple AT commands to break out of any state
            uart_flush(modem_config.uart_num);
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        ret = send_at_command("AT", "OK", 1000);

        if (ret != ESP_OK && retry_count < 2) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Modem not responding after %d attempts", retry_count);
        ESP_LOGW(TAG, "💡 Modem might need hardware reset or power cycle");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ Modem OK");

    // Disable echo
    send_at_command("ATE0", "OK", 1000);

    // Check SIM with retries (SIM needs time to initialize)
    ESP_LOGI(TAG, "📱 Checking SIM card...");
    int sim_retries = 10;
    while (sim_retries-- > 0) {
        esp_err_t ret = send_at_command("AT+CPIN?", "READY", 2000);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✓ SIM OK");
            break;
        }
        ESP_LOGW(TAG, "   SIM not ready, retrying... (%d/10)", 10 - sim_retries);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (sim_retries <= 0) {
        ESP_LOGE(TAG, "SIM card failed - Please check:");
        ESP_LOGE(TAG, "  1. SIM card is inserted correctly");
        ESP_LOGE(TAG, "  2. SIM card contacts are clean");
        ESP_LOGE(TAG, "  3. Power supply is stable (2A minimum)");
        return ESP_FAIL;
    }

    // Wait for network registration
    ESP_LOGI(TAG, "📶 Waiting for network...");
    int retries = 30;
    while (retries-- > 0) {
        // Check for registered status (,1 = registered home network, ,5 = registered roaming)
        // Format can be "+CREG: 0,1", "+CREG: 1,1", "+CREG: 2,1", etc.
        if (send_at_command("AT+CREG?", ",1", 2000) == ESP_OK ||
            send_at_command("AT+CREG?", ",5", 2000) == ESP_OK) {
            ESP_LOGI(TAG, "✓ Network registered!");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    if (retries <= 0) {
        ESP_LOGE(TAG, "Network registration timeout - Check:");
        ESP_LOGE(TAG, "  1. Antenna is connected properly");
        ESP_LOGE(TAG, "  2. SIM card has active service");
        ESP_LOGE(TAG, "  3. Signal strength in your area");
        return ESP_ERR_TIMEOUT;
    }

    // Set APN
    ESP_LOGI(TAG, "🌐 Setting APN: %s", modem_config.apn);
    char apn_cmd[128];
    snprintf(apn_cmd, sizeof(apn_cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", modem_config.apn);
    if (send_at_command(apn_cmd, "OK", 2000) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set APN");
        return ESP_FAIL;
    }

    // Check signal strength BEFORE entering PPP mode (last chance for AT commands)
    ESP_LOGI(TAG, "📶 Checking signal strength...");
    if (a7670c_get_signal_strength(&current_signal) == ESP_OK) {
        signal_checked = true;
    } else {
        ESP_LOGW(TAG, "Failed to get signal strength, continuing anyway...");
        signal_checked = false;
    }

    // Enter PPP mode
    ESP_LOGI(TAG, "🔗 Entering PPP mode...");
    uart_flush(modem_config.uart_num);
    vTaskDelay(pdMS_TO_TICKS(500));
    uart_write_bytes(modem_config.uart_num, "ATD*99#\r\n", 9);
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Clear any remaining AT command responses
    uart_flush(modem_config.uart_num);
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "✓ PPP mode active!");
    return ESP_OK;
}

// PPP status callback
static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
    ESP_LOGI(TAG, "PPP Status Event: %ld", event_id);

    if (event_id == NETIF_PPP_ERRORCONNECT ||
        event_id == NETIF_PPP_ERRORPROTOCOL ||
        event_id == NETIF_PPP_ERRORAUTHFAIL) {
        ESP_LOGE(TAG, "PPP connection error");
        ppp_connected = false;
        xEventGroupClearBits(ppp_event_group, PPP_CONNECTED_BIT);
    }
}

// IP event handler
static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data) {
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_ip_info_t *ip_info = &event->ip_info;

        ESP_LOGI(TAG, "+========================================+");
        ESP_LOGI(TAG, "|   PPP CONNECTED - GOT IP ADDRESS!      |");
        ESP_LOGI(TAG, "+========================================+");
        ESP_LOGI(TAG, "📡 IP: " IPSTR, IP2STR(&ip_info->ip));
        ESP_LOGI(TAG, "📡 Gateway: " IPSTR, IP2STR(&ip_info->gw));

        // Set DNS servers (Google DNS as fallback)
        esp_netif_dns_info_t dns_info;
        dns_info.ip.u_addr.ip4.addr = ESP_IP4TOADDR(8, 8, 8, 8);
        dns_info.ip.type = ESP_IPADDR_TYPE_V4;
        esp_netif_set_dns_info(ppp_netif, ESP_NETIF_DNS_MAIN, &dns_info);

        dns_info.ip.u_addr.ip4.addr = ESP_IP4TOADDR(8, 8, 4, 4);
        esp_netif_set_dns_info(ppp_netif, ESP_NETIF_DNS_BACKUP, &dns_info);
        ESP_LOGI(TAG, "📡 DNS: 8.8.8.8, 8.8.4.4");

        ppp_connected = true;
        xEventGroupSetBits(ppp_event_group, PPP_CONNECTED_BIT);
        esp_event_post(PPP_EVENT, PPP_EVENT_CONNECTED, NULL, 0, 0);

        // Reset modem failure and power cycle counters on successful connection
        if (modem_init_failures > 0 || modem_power_cycle_count > 0) {
            ESP_LOGI(TAG, "🔄 Modem counters reset - failures: %d → 0, power cycles: %d → 0",
                     modem_init_failures, modem_power_cycle_count);
            modem_init_failures = 0;
            modem_power_cycle_count = 0;
        }

    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGE(TAG, "PPP Lost IP");
        ppp_connected = false;
        xEventGroupClearBits(ppp_event_group, PPP_CONNECTED_BIT);
        esp_event_post(PPP_EVENT, PPP_EVENT_DISCONNECTED, NULL, 0, 0);
    }
}

// Initialize PPP
esp_err_t a7670c_ppp_init(const ppp_config_t* config) {
    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }

    // Store configuration
    memcpy(&modem_config, config, sizeof(ppp_config_t));

    // Create event group
    ppp_event_group = xEventGroupCreate();
    if (ppp_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }

    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = modem_config.baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_ERROR_CHECK(uart_param_config(modem_config.uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(modem_config.uart_num, modem_config.tx_pin,
                                 modem_config.rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(modem_config.uart_num, 2048, 2048, 0, NULL, 0));

    // Configure power pin
    gpio_config_t pwr_io_conf = {
        .pin_bit_mask = (1ULL << modem_config.pwr_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&pwr_io_conf);

    // Configure reset pin
    gpio_config_t rst_io_conf = {
        .pin_bit_mask = (1ULL << modem_config.reset_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&rst_io_conf);
    gpio_set_level(modem_config.reset_pin, 1);  // RESET pin HIGH = normal operation

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                               &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID,
                                               &on_ppp_changed, NULL));

    return ESP_OK;
}

// Connect PPP
esp_err_t a7670c_ppp_connect(void) {
    esp_err_t ret;

    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "📡 Initializing A7670C Modem...");
    ESP_LOGI(TAG, "===========================================");

    // Check if we need to reset modem due to repeated failures
    if (modem_init_failures >= MAX_MODEM_INIT_FAILURES) {
        modem_power_cycle_count++;
        ESP_LOGW(TAG, "🚨 Modem initialization failed %d times consecutively", modem_init_failures);
        ESP_LOGW(TAG, "🔄 Performing automatic hardware reset #%d to recover...", modem_power_cycle_count);
        ESP_LOGW(TAG, "💡 This will force the modem to re-detect the SIM card");

        // Use hardware reset to force SIM re-detection
        hardware_reset_modem();

        uint32_t next_retry_delay = get_retry_delay_ms();
        ESP_LOGW(TAG, "⏰ Next connection attempt will be in %lu seconds", next_retry_delay / 1000);
        ESP_LOGW(TAG, "💡 If you removed the SIM card, it should be detected now");

        // Reset failure counter to give fresh attempts
        modem_init_failures = 0;
        signal_checked = false;
    } else {
        // Normal power on sequence
        power_on_modem();
    }

    ret = init_modem_for_ppp();
    if (ret != ESP_OK) {
        modem_init_failures++;
        ESP_LOGE(TAG, "Failed to initialize modem for PPP (failure %d/%d)",
                 modem_init_failures, MAX_MODEM_INIT_FAILURES);

        if (modem_init_failures >= MAX_MODEM_INIT_FAILURES) {
            ESP_LOGW(TAG, "💡 Automatic modem power cycle will be triggered on next connection attempt");
        }

        return ret;
    }

    // Reset failure counter on successful initialization
    if (modem_init_failures > 0) {
        ESP_LOGI(TAG, "✅ Modem initialized successfully after %d previous failures", modem_init_failures);
    }
    modem_init_failures = 0;

    ESP_LOGI(TAG, "🔧 Creating PPP network interface...");

    // Create PPP network interface using default configuration
    esp_netif_config_t ppp_netif_config = ESP_NETIF_DEFAULT_PPP();
    ppp_netif = esp_netif_new(&ppp_netif_config);
    if (ppp_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create PPP netif");
        return ESP_FAIL;
    }

    // Configure PPP parameters
    esp_netif_ppp_config_t ppp_config = {
        .ppp_phase_event_enabled = true,
        .ppp_error_event_enabled = true,
    };
    ESP_ERROR_CHECK(esp_netif_ppp_set_params(ppp_netif, &ppp_config));

    // Set PPP authentication (required even if empty)
    const char *user = (modem_config.user && strlen(modem_config.user) > 0) ? modem_config.user : "";
    const char *pass = (modem_config.pass && strlen(modem_config.pass) > 0) ? modem_config.pass : "";
    ESP_ERROR_CHECK(esp_netif_ppp_set_auth(ppp_netif, NETIF_PPP_AUTHTYPE_PAP, user, pass));

    // Create PPP over serial using UART
    void *uart_handle = (void *)(intptr_t)modem_config.uart_num;
    esp_netif_driver_ifconfig_t driver_cfg = {
        .handle = ppp_netif,
        .transmit = ppp_output_callback,
        .driver_free_rx_buffer = NULL
    };

    // Set the driver configuration
    ESP_ERROR_CHECK(esp_netif_set_driver_config(ppp_netif, &driver_cfg));

    // Start PPP (this function returns void)
    ESP_LOGI(TAG, "🚀 Starting PPP...");
    esp_netif_action_connected(ppp_netif, 0, 0, NULL);
    esp_netif_action_start(ppp_netif, 0, 0, NULL);

    // Start UART receive task to feed data to PPP
    xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 12, &uart_rx_task_handle);

    // Wait for IP
    ESP_LOGI(TAG, "⏳ Waiting for PPP IP address...");
    EventBits_t bits = xEventGroupWaitBits(ppp_event_group, PPP_CONNECTED_BIT,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));

    if (bits & PPP_CONNECTED_BIT) {
        ESP_LOGI(TAG, "✅ Internet connected via PPP!");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get PPP IP");
        return ESP_ERR_TIMEOUT;
    }
}

// Exit PPP mode and reset modem
static esp_err_t exit_ppp_mode(void) {
    ESP_LOGI(TAG, "Exiting PPP mode and resetting modem...");

    // Method 1: Try Hayes escape sequence (+++)
    vTaskDelay(pdMS_TO_TICKS(1000));  // Guard time before +++
    uart_write_bytes(modem_config.uart_num, "+++", 3);
    vTaskDelay(pdMS_TO_TICKS(1000));  // Guard time after +++
    uart_flush(modem_config.uart_num);

    // Method 2: Hardware reset via power pin
    gpio_set_level(modem_config.pwr_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    gpio_set_level(modem_config.pwr_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(8000));  // Wait for modem to boot

    ESP_LOGI(TAG, "Modem reset complete");
    return ESP_OK;
}

// Disconnect PPP
esp_err_t a7670c_ppp_disconnect(void) {
    ESP_LOGI(TAG, "Disconnecting PPP...");

    // First, exit PPP mode on the modem
    exit_ppp_mode();

    // Then destroy the PPP interface
    if (ppp_netif != NULL) {
        esp_netif_action_stop(ppp_netif, NULL, 0, NULL);
        esp_netif_destroy(ppp_netif);
        ppp_netif = NULL;
    }

    ppp_connected = false;
    xEventGroupClearBits(ppp_event_group, PPP_CONNECTED_BIT);

    ESP_LOGI(TAG, "PPP disconnected");
    return ESP_OK;
}

// Deinitialize A7670C PPP (cleanup resources)
esp_err_t a7670c_ppp_deinit(void) {
    ESP_LOGI(TAG, "Deinitializing A7670C PPP...");

    // Disconnect PPP if still connected
    if (ppp_connected) {
        a7670c_ppp_disconnect();
    }

    // Stop UART RX task BEFORE deleting UART driver
    if (uart_rx_task_handle != NULL) {
        ESP_LOGI(TAG, "Stopping UART RX task...");
        uart_rx_task_running = false;  // Signal task to exit

        // Wait up to 2 seconds for task to finish
        int wait_count = 0;
        while (uart_rx_task_handle != NULL && wait_count < 20) {
            vTaskDelay(pdMS_TO_TICKS(100));
            wait_count++;
        }

        if (uart_rx_task_handle != NULL) {
            ESP_LOGW(TAG, "UART RX task did not stop gracefully, deleting forcefully");
            vTaskDelete(uart_rx_task_handle);
            uart_rx_task_handle = NULL;
        } else {
            ESP_LOGI(TAG, "UART RX task stopped successfully");
        }
    }

    // Delete event group
    if (ppp_event_group) {
        vEventGroupDelete(ppp_event_group);
        ppp_event_group = NULL;
    }

    // Now safe to deinitialize UART
    ESP_LOGI(TAG, "Deleting UART driver...");
    uart_driver_delete(modem_config.uart_num);

    // Reset GPIO pins to input
    gpio_reset_pin(modem_config.tx_pin);
    gpio_reset_pin(modem_config.rx_pin);
    gpio_reset_pin(modem_config.pwr_pin);
    if (modem_config.reset_pin >= 0) {
        gpio_reset_pin(modem_config.reset_pin);
    }

    // Clear state variables
    ppp_connected = false;
    signal_checked = false;
    modem_init_failures = 0;
    modem_power_cycle_count = 0;

    ESP_LOGI(TAG, "A7670C PPP deinitialized");
    return ESP_OK;
}

// Check if PPP is connected
bool a7670c_ppp_is_connected(void) {
    return ppp_connected;
}

// Alias for compatibility
bool a7670c_is_connected(void) {
    return a7670c_ppp_is_connected();
}

// Get netif
esp_netif_t* a7670c_ppp_get_netif(void) {
    return ppp_netif;
}

// Get IP info
esp_err_t a7670c_ppp_get_ip_info(char* ip_str, size_t ip_str_size) {
    if (!ppp_connected || ppp_netif == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(ppp_netif, &ip_info) == ESP_OK) {
        snprintf(ip_str, ip_str_size, IPSTR, IP2STR(&ip_info.ip));
        return ESP_OK;
    }

    return ESP_FAIL;
}

// Get signal strength
esp_err_t a7670c_get_signal_strength(signal_strength_t* signal) {
    if (signal == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize operator_name
    strncpy(signal->operator_name, "Unknown", sizeof(signal->operator_name));

    char response[256] = {0};

    // Send AT+CSQ command
    ESP_LOGI(TAG, ">>> AT+CSQ");
    const char* cmd = "AT+CSQ\r\n";
    uart_write_bytes(modem_config.uart_num, cmd, strlen(cmd));

    // Wait for response
    int len = 0;
    int total_len = 0;
    int64_t start_time = esp_timer_get_time() / 1000;

    while ((esp_timer_get_time() / 1000 - start_time) < 2000) {
        len = uart_read_bytes(modem_config.uart_num, uart_rx_buffer, sizeof(uart_rx_buffer),
                             pdMS_TO_TICKS(100));
        if (len > 0) {
            if (total_len + len < sizeof(response)) {
                memcpy(response + total_len, uart_rx_buffer, len);
                total_len += len;
                response[total_len] = '\0';

                if (strstr(response, "+CSQ:")) {
                    ESP_LOGI(TAG, "<<< %s", response);

                    // Parse response: +CSQ: <rssi>,<ber>
                    char* csq_start = strstr(response, "+CSQ:");
                    if (csq_start) {
                        int rssi, ber;
                        if (sscanf(csq_start, "+CSQ: %d,%d", &rssi, &ber) == 2) {
                            signal->rssi = rssi;
                            signal->ber = ber;

                            // Convert RSSI to dBm
                            if (rssi == 99) {
                                signal->rssi_dbm = -999;  // Unknown
                                signal->quality = "Unknown";
                            } else if (rssi >= 0 && rssi <= 31) {
                                signal->rssi_dbm = -113 + (rssi * 2);

                                // Determine quality
                                if (rssi >= 20) {
                                    signal->quality = "Excellent";
                                } else if (rssi >= 15) {
                                    signal->quality = "Good";
                                } else if (rssi >= 10) {
                                    signal->quality = "Fair";
                                } else if (rssi >= 5) {
                                    signal->quality = "Poor";
                                } else {
                                    signal->quality = "Very Poor";
                                }
                            } else {
                                signal->rssi_dbm = -999;
                                signal->quality = "Invalid";
                            }

                            // Get operator name with AT+COPS?
                            char cops_response[256] = {0};
                            memset(uart_rx_buffer, 0, sizeof(uart_rx_buffer));
                            vTaskDelay(pdMS_TO_TICKS(100));

                            ESP_LOGI(TAG, ">>> AT+COPS?");
                            const char* cops_cmd = "AT+COPS?\r\n";
                            uart_write_bytes(modem_config.uart_num, cops_cmd, strlen(cops_cmd));

                            int cops_len = 0;
                            int cops_total = 0;
                            int64_t cops_start = esp_timer_get_time() / 1000;

                            while ((esp_timer_get_time() / 1000 - cops_start) < 2000) {
                                cops_len = uart_read_bytes(modem_config.uart_num, uart_rx_buffer,
                                                          sizeof(uart_rx_buffer), pdMS_TO_TICKS(100));
                                if (cops_len > 0) {
                                    if (cops_total + cops_len < sizeof(cops_response)) {
                                        memcpy(cops_response + cops_total, uart_rx_buffer, cops_len);
                                        cops_total += cops_len;
                                        cops_response[cops_total] = '\0';

                                        if (strstr(cops_response, "+COPS:")) {
                                            // Parse: +COPS: 0,0,"Operator",<mode>
                                            char* cops_start = strstr(cops_response, "+COPS:");
                                            char* op_start = strchr(cops_start, '"');
                                            if (op_start) {
                                                op_start++;  // Skip opening quote
                                                char* op_end = strchr(op_start, '"');
                                                if (op_end) {
                                                    size_t op_len = op_end - op_start;
                                                    if (op_len < sizeof(signal->operator_name)) {
                                                        strncpy(signal->operator_name, op_start, op_len);
                                                        signal->operator_name[op_len] = '\0';
                                                    }
                                                }
                                            }
                                            break;
                                        }
                                    }
                                }
                                vTaskDelay(pdMS_TO_TICKS(10));
                            }

                            ESP_LOGI(TAG, "📶 Signal: RSSI=%d (%d dBm), BER=%d, Quality=%s, Operator=%s",
                                    signal->rssi, signal->rssi_dbm, signal->ber, signal->quality, signal->operator_name);
                            return ESP_OK;
                        }
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGE(TAG, "Failed to get signal strength");
    return ESP_ERR_TIMEOUT;
}

// Restart modem (power cycle)
esp_err_t a7670c_restart_modem(void) {
    ESP_LOGW(TAG, "🔄 Restarting modem due to poor signal...");

    // Disconnect PPP first
    a7670c_ppp_disconnect();
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Power off modem (pulse PWR_KEY)
    ESP_LOGI(TAG, "   Powering off modem...");
    gpio_set_level(modem_config.pwr_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    gpio_set_level(modem_config.pwr_pin, 1);

    // Wait for modem to shut down
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Power on modem (pulse PWR_KEY again)
    ESP_LOGI(TAG, "   Powering on modem...");
    gpio_set_level(modem_config.pwr_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(1500));
    gpio_set_level(modem_config.pwr_pin, 1);

    // Wait for modem to boot
    ESP_LOGI(TAG, "   Waiting for modem to boot...");
    vTaskDelay(pdMS_TO_TICKS(8000));

    ESP_LOGI(TAG, "✅ Modem restart complete");

    // Clear signal flag so it will be checked again
    signal_checked = false;

    return ESP_OK;
}

// Get stored signal strength (checked before PPP mode was entered)
esp_err_t a7670c_get_stored_signal_strength(signal_strength_t* signal) {
    if (signal == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!signal_checked) {
        ESP_LOGW(TAG, "Signal strength not yet checked");
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(signal, &current_signal, sizeof(signal_strength_t));
    return ESP_OK;
}

// Get the recommended retry delay based on connection failure history
uint32_t a7670c_get_retry_delay_ms(void) {
    return get_retry_delay_ms();
}