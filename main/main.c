#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "mqtt_client.h"
#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha256.h"

#include "iot_configs.h"
#include "modbus.h"
#include "web_config.h"
#include "sensor_manager.h"
#include "network_stats.h"
#include "json_templates.h"
#include "sd_card_logger.h"
#include "ds3231_rtc.h"
#include "a7670c_ppp.h"

static const char *TAG = "AZURE_IOT";

// GPIO configuration for AP mode trigger
// GPIO configuration for AP mode triggers
#define CONFIG_GPIO_PIN 34
#define CONFIG_GPIO_BOOT_PIN 0
#define CONFIG_GPIO_INTR_FLAG_34 GPIO_INTR_POSEDGE    // GPIO 34: trigger on rising edge
#define CONFIG_GPIO_INTR_FLAG_0 GPIO_INTR_NEGEDGE     // GPIO 0: trigger on falling edge

// GPIO configuration for modem reset
#define MODEM_RESET_GPIO_PIN 2

// GPIO configuration for status LEDs (LOW = LED ON)
#define WEBSERVER_LED_GPIO_PIN 25   // Web server active LED
#define MQTT_LED_GPIO_PIN 26        // MQTT connection status LED  
#define SENSOR_LED_GPIO_PIN 27      // Sensor response status LED

// WiFi event group
// static EventGroupHandle_t wifi_event_group;  // Unused variable
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Task handles for dual core usage
TaskHandle_t modbus_task_handle = NULL;
TaskHandle_t mqtt_task_handle = NULL;
TaskHandle_t telemetry_task_handle = NULL;
QueueHandle_t sensor_data_queue = NULL;

// Global variables
static esp_mqtt_client_handle_t mqtt_client;
static char sas_token[512];
static uint32_t telemetry_send_count = 0;
bool mqtt_connected = false;  // Non-static for external access

// Flow meter configuration now comes from web interface (NVS storage)
// Hardcoded configuration removed - using dynamic sensor configuration

// Configuration flags for debugging different byte orders (moved to web_config.h)

static flow_meter_data_t current_flow_data = {0};

// Production monitoring variables
uint32_t mqtt_reconnect_count = 0;  // Non-static for external access
static uint32_t modbus_failure_count = 0;

// System reliability constants (using values from iot_configs.h)
uint32_t total_telemetry_sent = 0;  // Non-static for external access
static uint32_t system_uptime_start = 0;

// MQTT connection timing for status monitoring
int64_t mqtt_connect_time = 0;  // Timestamp when MQTT connected
int64_t last_telemetry_time = 0;  // Timestamp of last telemetry sent

// Static buffers to avoid stack overflow
static char mqtt_broker_uri[256];
static char mqtt_username[256];
static char telemetry_topic[256];
static char telemetry_payload[8192];  // Increased to support up to 20 sensors
static char c2d_topic[256];

// GPIO interrupt flag for web server toggle
static volatile bool web_server_toggle_requested = false;
static volatile bool system_shutdown_requested = false;
static volatile bool web_server_running = false;

// Modem control variables
static bool modem_reset_enabled = false;
static int modem_reset_gpio_pin = 2; // Default GPIO pin, configurable via web interface
static TaskHandle_t modem_reset_task_handle = NULL;

// LED status variables
static volatile bool sensors_responding = false;
static volatile bool webserver_led_on = false;
static volatile bool mqtt_led_on = false;
static volatile bool sensor_led_on = false;

// Forward declarations
static bool send_telemetry(void);
static void init_modem_reset_gpio(void);
static void perform_modem_reset(void);
static void modem_reset_task(void *pvParameters);
static esp_err_t reinit_modem_reset_gpio(int new_gpio_pin);
static void start_web_server(void);
static void stop_web_server(void);
static void handle_web_server_toggle(void);
static void init_status_leds(void);
static void set_status_led(int gpio_pin, bool on);
static void update_led_status(void);
static bool is_network_connected(void);

// Helper function to check network connectivity (replaces network_manager_is_connected)
static bool is_network_connected(void) {
    system_config_t *config = get_system_config();
    if (!config) return false;

    if (config->network_mode == NETWORK_MODE_WIFI) {
        // Check WiFi connection
        wifi_ap_record_t ap_info;
        return (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
    } else {
        // Check SIM/PPP connection
        return a7670c_is_connected();
    }
}

// Azure IoT Root CA Certificate
extern const uint8_t azure_root_ca_pem_start[] asm("_binary_azure_ca_cert_pem_start");
extern const uint8_t azure_root_ca_pem_end[] asm("_binary_azure_ca_cert_pem_end");

// Legacy WiFi event handler - replaced by web_config system
/*static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi station started, connecting to AP...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*) event_data;
        ESP_LOGI(TAG, "Connected to AP SSID:%s channel:%d", event->ssid, event->channel);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "Disconnected from AP. Reason: %d. Retrying...", event->reason);
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask:" IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway:" IPSTR, IP2STR(&event->ip_info.gw));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}*/

// Legacy WiFi connection function - replaced by web_config system
/*static void connect_to_wifi(void) {
    wifi_event_group = xEventGroupCreate();
    
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = IOT_CONFIG_WIFI_SSID,
            .password = IOT_CONFIG_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // Disable power saving mode for more reliable connection
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init finished.");

    // Wait for connection with timeout
    ESP_LOGI(TAG, "Waiting for WiFi connection (timeout: 30 seconds)...");
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            30000 / portTICK_PERIOD_MS);  // 30 second timeout

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "[OK] Successfully connected to AP SSID:%s", IOT_CONFIG_WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "[ERROR] Failed to connect to SSID:%s", IOT_CONFIG_WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "[ERROR] WiFi connection timeout! Check your network settings.");
        ESP_LOGE(TAG, "Troubleshooting steps:");
        ESP_LOGE(TAG, "1. Verify SSID: '%s'", IOT_CONFIG_WIFI_SSID);
        ESP_LOGE(TAG, "2. Check WiFi password is correct");
        ESP_LOGE(TAG, "3. Ensure router is accessible and not blocking new connections");
        ESP_LOGE(TAG, "4. Try moving ESP32 closer to router");
    }
}*/

static void initialize_time(void) {
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;

    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    ESP_LOGI(TAG, "Time initialized");
}

// URL encode function
static void url_encode(const char* input, char* output, size_t output_size) {
    static const char hex[] = "0123456789ABCDEF";
    size_t input_len = strlen(input);
    size_t output_len = 0;
    
    for (size_t i = 0; i < input_len && output_len < output_size - 1; i++) {
        char c = input[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            output[output_len++] = c;
        } else {
            if (output_len < output_size - 3) {
                output[output_len++] = '%';
                output[output_len++] = hex[(c >> 4) & 0xF];
                output[output_len++] = hex[c & 0xF];
            }
        }
    }
    output[output_len] = '\0';
}

// Generate Azure IoT Hub SAS Token
static int generate_sas_token(char* token, size_t token_size, uint32_t expiry_seconds) {
    time_t now = time(NULL);
    uint32_t expiry = now + expiry_seconds;
    
    // Create the resource URI (no URL encoding needed here)
    char resource_uri[256];
    char string_to_sign[512];
    char encoded_uri[256];
    
    // Get dynamic configuration
    system_config_t* config = get_system_config();
    ESP_LOGI(TAG, "[DYNAMIC] Using Azure Device ID: %s", config->azure_device_id);
    ESP_LOGI(TAG, "[DYNAMIC] Using Azure Device Key (first 10 chars): %.10s...", config->azure_device_key);
    
    // Resource URI format for Azure IoT Hub
    snprintf(resource_uri, sizeof(resource_uri), "%s/devices/%s", 
             IOT_CONFIG_IOTHUB_FQDN, config->azure_device_id);
    
    // URL encode the resource URI
    url_encode(resource_uri, encoded_uri, sizeof(encoded_uri));
    
    // Create the complete string to sign: encoded_uri + "\n" + expiry
    snprintf(string_to_sign, sizeof(string_to_sign), "%s\n%" PRIu32, encoded_uri, expiry);
    
    ESP_LOGI(TAG, "Resource URI: %s", resource_uri);
    ESP_LOGI(TAG, "Encoded URI: %s", encoded_uri);
    ESP_LOGI(TAG, "Expiry: %" PRIu32, expiry);
    ESP_LOGI(TAG, "String to sign: %s", string_to_sign);
    
    // Decode the device key (base64)
    unsigned char decoded_key[64];
    size_t decoded_key_len;
    
    ESP_LOGI(TAG, "Device key length: %d", strlen(config->azure_device_key));
    
    int ret = mbedtls_base64_decode(decoded_key, sizeof(decoded_key), &decoded_key_len,
                                   (const unsigned char*)config->azure_device_key, 
                                   strlen(config->azure_device_key));
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to decode device key: %d (key: %.20s...)", ret, config->azure_device_key);
        return -1;
    }
    
    ESP_LOGI(TAG, "Decoded key length: %d", decoded_key_len);
    
    // Generate HMAC-SHA256 signature
    unsigned char signature[32];
    
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    
    mbedtls_md_init(&ctx);
    ret = mbedtls_md_setup(&ctx, info, 1);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to setup HMAC: %d", ret);
        mbedtls_md_free(&ctx);
        return -1;
    }
    
    ret = mbedtls_md_hmac_starts(&ctx, decoded_key, decoded_key_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to start HMAC: %d", ret);
        mbedtls_md_free(&ctx);
        return -1;
    }
    
    ret = mbedtls_md_hmac_update(&ctx, (const unsigned char*)string_to_sign, strlen(string_to_sign));
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to update HMAC: %d", ret);
        mbedtls_md_free(&ctx);
        return -1;
    }
    
    ret = mbedtls_md_hmac_finish(&ctx, signature);
    mbedtls_md_free(&ctx);
    
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to finish HMAC: %d", ret);
        return -1;
    }
    
    // Base64 encode the signature
    char encoded_signature[128];
    size_t encoded_len;
    
    ret = mbedtls_base64_encode((unsigned char*)encoded_signature, sizeof(encoded_signature), &encoded_len,
                               signature, sizeof(signature));
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to encode signature: %d", ret);
        return -1;
    }
    
    // URL encode the signature
    char url_encoded_signature[256];
    url_encode(encoded_signature, url_encoded_signature, sizeof(url_encoded_signature));
    
    // Create the SAS token
    snprintf(token, token_size,
             "SharedAccessSignature sr=%s&sig=%s&se=%" PRIu32,
             encoded_uri, url_encoded_signature, expiry);
    
    ESP_LOGI(TAG, "Generated SAS token: %.100s...", token);
    return 0;
}

// Callback function for replaying cached SD card messages to MQTT
static void replay_message_callback(const pending_message_t* msg) {
    if (!msg || !mqtt_client) {
        ESP_LOGE(TAG, "[SD] Invalid message or MQTT client not initialized");
        return;
    }

    if (!mqtt_connected) {
        ESP_LOGW(TAG, "[SD] Cannot replay message %lu - MQTT not connected", msg->message_id);
        return;
    }

    ESP_LOGI(TAG, "[SD] 📤 Replaying cached message ID %lu", msg->message_id);
    ESP_LOGI(TAG, "[SD]    Topic: %s", msg->topic);
    ESP_LOGI(TAG, "[SD]    Timestamp: %s", msg->timestamp);
    ESP_LOGI(TAG, "[SD]    Payload: %.100s%s", msg->payload, strlen(msg->payload) > 100 ? "..." : "");

    int msg_id = esp_mqtt_client_publish(mqtt_client, msg->topic, msg->payload, 0, 1, 0);
    if (msg_id == -1) {
        ESP_LOGE(TAG, "[SD] ❌ Failed to publish replayed message %lu", msg->message_id);
    } else {
        ESP_LOGI(TAG, "[SD] ✅ Successfully published replayed message %lu (MQTT msg_id: %d)",
                 msg->message_id, msg_id);

        // Remove the message from SD card after successful publish
        esp_err_t ret = sd_card_remove_message(msg->message_id);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "[SD] Failed to remove replayed message %lu from SD card", msg->message_id);
        }
    }

    // Small delay between replayed messages to avoid overwhelming MQTT broker
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "[OK] MQTT_EVENT_CONNECTED - Azure IoT Hub connection established!");
            mqtt_connected = true;
            mqtt_connect_time = esp_timer_get_time() / 1000000;  // Record connection time in seconds
            mqtt_reconnect_count = 0; // Reset reconnect counter on successful connection

            // Subscribe to cloud-to-device messages after connection
            system_config_t* config = get_system_config();
            snprintf(c2d_topic, sizeof(c2d_topic), "devices/%s/messages/devicebound/#", config->azure_device_id);
            esp_mqtt_client_subscribe(mqtt_client, c2d_topic, 1);
            ESP_LOGI(TAG, "[MAIL] Subscribed to C2D messages: %s", c2d_topic);
            
            ESP_LOGI(TAG, "[OK] MQTT connected successfully");
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "[WARN] MQTT_EVENT_DISCONNECTED");
            mqtt_connected = false;
            mqtt_reconnect_count++;
            
            // Trigger modem reset if enabled and not already running
            if (modem_reset_enabled && modem_reset_task_handle == NULL) {
                ESP_LOGI(TAG, "[MODEM] MQTT disconnected, triggering modem reset...");
                
                // Create modem reset task
                BaseType_t result = xTaskCreate(
                    modem_reset_task,
                    "modem_reset",
                    2048,
                    NULL,
                    2,  // Low priority
                    &modem_reset_task_handle
                );
                
                if (result != pdPASS) {
                    ESP_LOGE(TAG, "[MODEM] Failed to create modem reset task");
                    modem_reset_task_handle = NULL;
                }
            }
            
            // Check if we've exceeded maximum reconnection attempts
            if (mqtt_reconnect_count >= MAX_MQTT_RECONNECT_ATTEMPTS) {
                ESP_LOGE(TAG, "[ERROR] Exceeded maximum MQTT reconnection attempts (%d)", MAX_MQTT_RECONNECT_ATTEMPTS);
                if (SYSTEM_RESTART_ON_CRITICAL_ERROR) {
                    ESP_LOGE(TAG, "[PROC] Restarting system due to persistent MQTT connection issues...");
                    esp_restart();
                }
            }
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "[OK] TELEMETRY PUBLISHED SUCCESSFULLY! msg_id=%d", event->msg_id);
            total_telemetry_sent++;
            last_telemetry_time = esp_timer_get_time() / 1000000;  // Record last telemetry time in seconds
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "[MSG] CLOUD-TO-DEVICE MESSAGE RECEIVED:");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "[ERROR] MQTT_EVENT_ERROR");
            mqtt_connected = false;
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "TCP transport error: %d", event->error_handle->esp_transport_sock_errno);
                ESP_LOGE(TAG, "Possible causes: Network connectivity, firewall, DNS");
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "Connection refused: %d", event->error_handle->connect_return_code);
                ESP_LOGE(TAG, "Possible causes: Invalid SAS token, wrong device ID, IoT Hub settings");
                
                // If connection refused, might be SAS token expiry
                if (event->error_handle->connect_return_code == 5) {
                    ESP_LOGE(TAG, "Authentication failed - possibly expired SAS token");
                }
            }
            break;
            
        default:
            ESP_LOGI(TAG, "Other event id:%" PRId32, event_id);
            break;
    }
}

// Function to test DNS resolution
static esp_err_t test_dns_resolution(const char* hostname) {
    ESP_LOGI(TAG, "[FIND] Testing DNS resolution for: %s", hostname);
    
    struct addrinfo hints = {0};
    struct addrinfo *result = NULL;
    
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    
    int ret = getaddrinfo(hostname, "443", &hints, &result);
    if (ret != 0) {
        ESP_LOGE(TAG, "[ERROR] DNS resolution failed for %s: getaddrinfo() returned %d", hostname, ret);
        ESP_LOGE(TAG, "   Error details: %d", ret);
        return ESP_FAIL;
    }
    
    if (result) {
        struct sockaddr_in* addr_in = (struct sockaddr_in*)result->ai_addr;
        ESP_LOGI(TAG, "[OK] DNS resolved %s to: %s", hostname, inet_ntoa(addr_in->sin_addr));
        freeaddrinfo(result);
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "[ERROR] DNS resolution returned no results for %s", hostname);
    return ESP_FAIL;
}

// Function to test basic internet connectivity
static esp_err_t test_internet_connectivity(void) {
    ESP_LOGI(TAG, "[NET] Testing internet connectivity...");
    
    // Test multiple DNS servers
    const char* dns_servers[] = {
        "8.8.8.8",           // Google DNS
        "1.1.1.1",           // Cloudflare DNS
        "208.67.222.222"     // OpenDNS
    };
    
    bool any_dns_works = false;
    for (int i = 0; i < 3; i++) {
        if (test_dns_resolution(dns_servers[i]) == ESP_OK) {
            any_dns_works = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Wait between tests
    }
    
    if (!any_dns_works) {
        ESP_LOGE(TAG, "[ERROR] No DNS servers are reachable - internet connectivity issue");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "[OK] Basic internet connectivity confirmed");
    return ESP_OK;
}

// Function to troubleshoot Azure IoT Hub connectivity
static void troubleshoot_azure_connectivity(void) {
    ESP_LOGI(TAG, "[CONFIG] Azure IoT Hub connectivity troubleshooting...");
    system_config_t* config = get_system_config();
    ESP_LOGI(TAG, "Hub FQDN: %s", IOT_CONFIG_IOTHUB_FQDN);
    ESP_LOGI(TAG, "Device ID: %s", config->azure_device_id);
    
    // Check if the hostname format is correct
    if (!strstr(IOT_CONFIG_IOTHUB_FQDN, ".azure-devices.net")) {
        ESP_LOGE(TAG, "[WARN] WARNING: Hostname doesn't end with .azure-devices.net");
        ESP_LOGE(TAG, "   Expected format: <hub-name>.azure-devices.net");
    }
    
    // Try to resolve microsoft.com as a test
    ESP_LOGI(TAG, "[FIND] Testing Microsoft domain resolution...");
    if (test_dns_resolution("microsoft.com") == ESP_OK) {
        ESP_LOGI(TAG, "[OK] Microsoft domains are reachable");
        ESP_LOGI(TAG, "[TIP] Issue is likely with specific IoT Hub hostname");
    } else {
        ESP_LOGE(TAG, "[ERROR] Microsoft domains not reachable - possible firewall/DNS filtering");
    }
    
    // Test Azure IoT Hub service endpoint
    ESP_LOGI(TAG, "[FIND] Testing Azure service endpoint...");
    if (test_dns_resolution("azure-devices.net") == ESP_OK) {
        ESP_LOGI(TAG, "[OK] Azure IoT service is reachable");
        ESP_LOGI(TAG, "[TIP] Issue is likely with specific hub name: %s", IOT_CONFIG_IOTHUB_FQDN);
    } else {
        ESP_LOGE(TAG, "[ERROR] Azure IoT service not reachable - check firewall/DNS");
    }
}

static int initialize_mqtt_client(void) {
    ESP_LOGI(TAG, "[LINK] Initializing MQTT client on core %d", xPortGetCoreID());

    system_config_t* config = get_system_config();

    // Check network connection based on mode
    if (config->network_mode == NETWORK_MODE_WIFI) {
        // WiFi mode - check WiFi connection
        wifi_ap_record_t ap_info;
        esp_err_t wifi_status = esp_wifi_sta_get_ap_info(&ap_info);
        if (wifi_status != ESP_OK) {
            ESP_LOGE(TAG, "[ERROR] WiFi not connected: %s", esp_err_to_name(wifi_status));
            return -1;
        }

        ESP_LOGI(TAG, "[WIFI] WiFi connected to: %s (RSSI: %d dBm)", ap_info.ssid, ap_info.rssi);

        // Get and log IP address
        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            ESP_LOGI(TAG, "[WEB] IP Address: " IPSTR, IP2STR(&ip_info.ip));
            ESP_LOGI(TAG, "[WEB] Gateway: " IPSTR, IP2STR(&ip_info.gw));
            ESP_LOGI(TAG, "[WEB] Netmask: " IPSTR, IP2STR(&ip_info.netmask));
        }
    } else {
        // SIM mode - check PPP connection
        if (!a7670c_ppp_is_connected()) {
            ESP_LOGE(TAG, "[ERROR] PPP not connected");
            return -1;
        }

        // Get and log PPP IP address
        char ip_str[32];
        if (a7670c_ppp_get_ip_info(ip_str, sizeof(ip_str)) == ESP_OK) {
            ESP_LOGI(TAG, "[SIM] PPP IP Address: %s", ip_str);
        }

        // Get stored signal strength
        signal_strength_t signal;
        if (a7670c_get_stored_signal_strength(&signal) == ESP_OK) {
            ESP_LOGI(TAG, "[SIM] Signal: %d dBm (%s), Operator: %s",
                     signal.rssi_dbm, signal.quality ? signal.quality : "Unknown", signal.operator_name);
        }
    }
    
    // Test basic internet connectivity
    if (test_internet_connectivity() != ESP_OK) {
        ESP_LOGE(TAG, "[ERROR] Basic internet connectivity failed");
        return -1;
    }
    
    // Test Azure IoT Hub DNS resolution
    if (test_dns_resolution(IOT_CONFIG_IOTHUB_FQDN) != ESP_OK) {
        ESP_LOGE(TAG, "[ERROR] Cannot resolve Azure IoT Hub: %s", IOT_CONFIG_IOTHUB_FQDN);
        troubleshoot_azure_connectivity();
        
        ESP_LOGE(TAG, "[TOOLS] TROUBLESHOOTING STEPS:");
        ESP_LOGE(TAG, "   1. Verify IoT Hub name in web configuration");
        ESP_LOGE(TAG, "   2. Check if IoT Hub exists in Azure portal");
        ESP_LOGE(TAG, "   3. Ensure network allows Azure domains");
        ESP_LOGE(TAG, "   4. Try restarting WiFi router");
        ESP_LOGE(TAG, "   5. Check DNS settings (try 8.8.8.8)");
        return -1;
    }
    
    ESP_LOGI(TAG, "[OK] Azure IoT Hub DNS resolution successful");
    
    // Generate SAS token (valid for 1 hour)
    if (generate_sas_token(sas_token, sizeof(sas_token), 3600) != 0) {
        ESP_LOGE(TAG, "Failed to generate SAS token");
        return -1;
    }

    // Use config from earlier declaration (already loaded at start of function)
    ESP_LOGI(TAG, "[DYNAMIC CONFIG] Loading Azure credentials from web configuration");
    ESP_LOGI(TAG, "[DYNAMIC CONFIG] Device ID: %s", config->azure_device_id);
    ESP_LOGI(TAG, "[DYNAMIC CONFIG] Device Key Length: %d", strlen(config->azure_device_key));
    
    // Create Azure IoT Hub MQTT broker URI
    snprintf(mqtt_broker_uri, sizeof(mqtt_broker_uri), "mqtts://%s", IOT_CONFIG_IOTHUB_FQDN);
    
    // Create MQTT username exactly like Arduino CCL (older API version)
    snprintf(mqtt_username, sizeof(mqtt_username), "%s/%s/?api-version=2018-06-30", 
             IOT_CONFIG_IOTHUB_FQDN, config->azure_device_id);
    
    ESP_LOGI(TAG, "MQTT Broker: %s", mqtt_broker_uri);
    ESP_LOGI(TAG, "MQTT Username: %s", mqtt_username);
    ESP_LOGI(TAG, "MQTT Client ID: %s", config->azure_device_id);
    ESP_LOGI(TAG, "SAS Token: %.100s...", sas_token);

    esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = mqtt_broker_uri,
        .broker.address.port = 8883,
        .credentials.client_id = config->azure_device_id,
        .credentials.username = mqtt_username,
        .credentials.authentication.password = sas_token,
        .session.keepalive = 30,
        .session.disable_clean_session = 0,
        .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1,  // Force MQTT 3.1.1 like Arduino 1.0.6
        .network.disable_auto_reconnect = false,
        .broker.verification.certificate = (const char*)azure_root_ca_pem_start,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_config);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return -1;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));

    esp_err_t start_result = esp_mqtt_client_start(mqtt_client);
    if (start_result != ESP_OK) {
        ESP_LOGE(TAG, "Could not start MQTT client: %s", esp_err_to_name(start_result));
        ESP_LOGE(TAG, "[TOOLS] MQTT CLIENT START TROUBLESHOOTING:");
        ESP_LOGE(TAG, "   1. Check SAS token validity");
        ESP_LOGE(TAG, "   2. Verify device exists in IoT Hub");
        ESP_LOGE(TAG, "   3. Check device key is correct");
        ESP_LOGE(TAG, "   4. Ensure IoT Hub allows new connections");
        return -1;
    }

    ESP_LOGI(TAG, "MQTT client started successfully");
    ESP_LOGI(TAG, "[TIME] Waiting for MQTT connection establishment...");
    
    // Give MQTT some time to establish connection
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    if (!mqtt_connected) {
        ESP_LOGW(TAG, "[WARN] MQTT not connected yet after 5 seconds");
        ESP_LOGW(TAG, "   This is normal - connection may take longer");
        ESP_LOGW(TAG, "   Check MQTT_EVENT_CONNECTED logs for success");
    }
    
    return 0;
}

static void create_telemetry_payload(char* payload, size_t payload_size) {
    system_config_t *config = get_system_config();

    // Get network statistics for telemetry
    network_stats_t net_stats = {0};
    if (is_network_connected()) {
        // Gather network stats based on mode
        if (config->network_mode == NETWORK_MODE_WIFI) {
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                net_stats.signal_strength = ap_info.rssi;
                strncpy(net_stats.network_type, "WiFi", sizeof(net_stats.network_type));

                // Determine quality
                if (ap_info.rssi >= -60) {
                    strncpy(net_stats.network_quality, "Excellent", sizeof(net_stats.network_quality));
                } else if (ap_info.rssi >= -70) {
                    strncpy(net_stats.network_quality, "Good", sizeof(net_stats.network_quality));
                } else if (ap_info.rssi >= -80) {
                    strncpy(net_stats.network_quality, "Fair", sizeof(net_stats.network_quality));
                } else {
                    strncpy(net_stats.network_quality, "Poor", sizeof(net_stats.network_quality));
                }
            }
        } else {
            // SIM mode - use stored signal strength (cannot use AT commands in PPP mode)
            signal_strength_t signal;
            if (a7670c_get_stored_signal_strength(&signal) == ESP_OK) {
                net_stats.signal_strength = signal.rssi_dbm;
                strncpy(net_stats.network_type, "4G", sizeof(net_stats.network_type));

                // Use quality from stored signal data
                if (signal.quality != NULL) {
                    strncpy(net_stats.network_quality, signal.quality, sizeof(net_stats.network_quality) - 1);
                } else {
                    // Fallback quality determination based on RSSI
                    if (signal.rssi_dbm >= -70) {
                        strncpy(net_stats.network_quality, "Excellent", sizeof(net_stats.network_quality));
                    } else if (signal.rssi_dbm >= -85) {
                        strncpy(net_stats.network_quality, "Good", sizeof(net_stats.network_quality));
                    } else if (signal.rssi_dbm >= -100) {
                        strncpy(net_stats.network_quality, "Fair", sizeof(net_stats.network_quality));
                    } else {
                        strncpy(net_stats.network_quality, "Poor", sizeof(net_stats.network_quality));
                    }
                }
            }
        }
        ESP_LOGI(TAG, "[NET] Signal: %d dBm, Type: %s", net_stats.signal_strength, net_stats.network_type);
    } else {
        // Default values when offline
        net_stats.signal_strength = 0;
        strncpy(net_stats.network_type, "Offline", sizeof(net_stats.network_type));
    }

    // Dynamically allocate memory for sensor readings to avoid stack overflow
    sensor_reading_t* readings = (sensor_reading_t*)malloc(20 * sizeof(sensor_reading_t));
    char* temp_json = (char*)malloc(MAX_JSON_PAYLOAD_SIZE);

    if (!readings || !temp_json) {
        ESP_LOGE(TAG, "[ERROR] Failed to allocate memory for sensor readings");
        if (readings) free(readings);
        if (temp_json) free(temp_json);
        payload[0] = '\0';
        return;
    }

    int actual_count = 0;
    esp_err_t ret = sensor_read_all_configured(readings, 20, &actual_count);
    
    if (ret == ESP_OK && actual_count > 0) {
        ESP_LOGI(TAG, "[FLOW] Creating merged JSON for %d sensors", actual_count);
        
        // Log sensor data for debugging
        for (int i = 0; i < actual_count; i++) {
            ESP_LOGI(TAG, "[DATA] Reading[%d]: Unit=%s, Valid=%d, Value=%.2f, Hex=%s", 
                     i, readings[i].unit_id, readings[i].valid, readings[i].value, readings[i].raw_hex);
        }
        
        int payload_pos = 0;
        
        // JSON array start - send as direct array, Azure will wrap it in "body"
        payload_pos += snprintf(payload + payload_pos, payload_size - payload_pos, "[");
        
        int valid_sensors = 0;
        for (int i = 0; i < actual_count; i++) {
            if (readings[i].valid) {
                // Find the matching sensor config by unit_id
                sensor_config_t* matching_sensor = NULL;
                for (int j = 0; j < config->sensor_count; j++) {
                    if (strcmp(config->sensors[j].unit_id, readings[i].unit_id) == 0) {
                        matching_sensor = &config->sensors[j];
                        break;
                    }
                }
                
                if (!matching_sensor || !matching_sensor->enabled) {
                    ESP_LOGW(TAG, "[WARN] Sensor %s not found or disabled", readings[i].unit_id);
                    continue;
                }
                
                ESP_LOGI(TAG, "[TARGET] Sensor: Name='%s', Unit='%s', Type='%s', Value=%.2f", 
                         matching_sensor->name, matching_sensor->unit_id, 
                         matching_sensor->sensor_type, readings[i].value);
                
                // Generate JSON for this specific sensor using template system
                esp_err_t json_result;

                // Check if sensor is ENERGY type and has hex string available
                if (strcasecmp(matching_sensor->sensor_type, "ENERGY") == 0 &&
                    strlen(readings[i].raw_hex) > 0) {
                    // Use the hex string from Test RS485 for ENERGY sensors
                    json_result = generate_sensor_json_with_hex(
                        matching_sensor,
                        readings[i].value,
                        readings[i].raw_value,
                        readings[i].raw_hex,
                        &net_stats,  // Pass network stats
                        temp_json,
                        MAX_JSON_PAYLOAD_SIZE
                    );
                } else {
                    // Use standard JSON generation for other sensor types
                    json_result = generate_sensor_json(
                        matching_sensor,
                        readings[i].value,
                        readings[i].raw_value ? readings[i].raw_value : (uint32_t)(readings[i].value * 10000),
                        &net_stats,  // Pass network stats
                        temp_json,
                        MAX_JSON_PAYLOAD_SIZE
                    );
                }
                
                if (json_result == ESP_OK) {
                    // Add comma if not first sensor
                    if (valid_sensors > 0) {
                        payload_pos += snprintf(payload + payload_pos, payload_size - payload_pos, ",");
                    }
                    
                    // Add this sensor's JSON to the array
                    int remaining_space = payload_size - payload_pos;
                    if (remaining_space > strlen(temp_json) + 50) { // Leave room for closing JSON
                        payload_pos += snprintf(payload + payload_pos, remaining_space, "%s", temp_json);
                        valid_sensors++;
                    } else {
                        ESP_LOGW(TAG, "[WARN] Payload buffer too small for sensor %d", i);
                        break;
                    }
                } else {
                    ESP_LOGW(TAG, "[WARN] Failed to generate JSON for sensor %d", i);
                }
            }
        }
        
        // Close JSON array and add metadata
        char timestamp[32];
        time_t now;
        time(&now);
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
        
        // Close JSON array - remove extra metadata as Azure adds its own
        payload_pos += snprintf(payload + payload_pos, payload_size - payload_pos, "]");
        
        ESP_LOGI(TAG, "[OK] Merged JSON created with %d sensors (%d bytes)", valid_sensors, payload_pos);
    } else {
        ESP_LOGW(TAG, "[WARN] No valid sensor data available, skipping telemetry");
        payload[0] = '\0'; // Empty payload to indicate no data
    }
    
    // Free dynamically allocated memory
    free(readings);
    free(temp_json);
}

static esp_err_t read_configured_sensors_data(void) {
    ESP_LOGI(TAG, "[FLOW] Reading configured sensors via Modbus RS485...");
    
    system_config_t *config = get_system_config();
    
    if (config->sensor_count == 0) {
        ESP_LOGW(TAG, "[WARN] No sensors configured, using fallback data");
        current_flow_data.data_valid = false;
        return ESP_FAIL;
    }
    
    // Read all configured sensors using sensor_manager
    sensor_reading_t readings[8];
    int actual_count = 0;
    
    esp_err_t ret = sensor_read_all_configured(readings, 8, &actual_count);
    
    if (ret == ESP_OK && actual_count > 0) {
        ESP_LOGI(TAG, "[OK] Successfully read %d sensors", actual_count);
        
        // Use the first valid sensor reading for telemetry
        for (int i = 0; i < actual_count; i++) {
            if (readings[i].valid) {
                // Map sensor_reading_t to flow_meter_data_t for compatibility
                current_flow_data.totalizer_value = readings[i].value;
                current_flow_data.raw_totalizer = (uint32_t)(readings[i].value * 10000); // Approximate raw
                strncpy(current_flow_data.timestamp, readings[i].timestamp, sizeof(current_flow_data.timestamp) - 1);
                current_flow_data.data_valid = true;
                current_flow_data.last_read_time = esp_timer_get_time() / 1000000;
                
                ESP_LOGI(TAG, "[DATA] Primary sensor %s: %.6f (Slave %d, Reg %d)", 
                         readings[i].unit_id, readings[i].value, 
                         config->sensors[i].slave_id, config->sensors[i].register_address);
                break;
            }
        }
        
        modbus_failure_count = 0; // Reset failure counter on success
        
        // Print Modbus statistics
        modbus_stats_t stats;
        modbus_get_statistics(&stats);
        ESP_LOGI(TAG, "[STATS] Modbus Stats - Total: %lu, Success: %lu, Failed: %lu", 
                 stats.total_requests, stats.successful_requests, stats.failed_requests);
    } else {
        ESP_LOGE(TAG, "[ERROR] Failed to read configured sensors");
        current_flow_data.data_valid = false;
        modbus_failure_count++;
        
        // Check if we've exceeded maximum Modbus failures
        if (modbus_failure_count >= MAX_MODBUS_READ_FAILURES) {
            ESP_LOGE(TAG, "[ERROR] Exceeded maximum Modbus read failures (%d)", MAX_MODBUS_READ_FAILURES);
            ESP_LOGE(TAG, "[CONFIG] Attempting to reinitialize Modbus communication...");
            
            // Try to reinitialize Modbus
            modbus_deinit();
            vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1 second
            
            esp_err_t init_ret = modbus_init();
            if (init_ret == ESP_OK) {
                ESP_LOGI(TAG, "[OK] Modbus reinitialized successfully");
                modbus_failure_count = 0;
            } else {
                ESP_LOGE(TAG, "[ERROR] Failed to reinitialize Modbus: %s", esp_err_to_name(init_ret));
                if (SYSTEM_RESTART_ON_CRITICAL_ERROR) {
                    ESP_LOGE(TAG, "[PROC] Restarting system due to persistent Modbus issues...");
                    esp_restart();
                }
            }
        }
    }
    
    return ret;
}

// GPIO interrupt handler - toggles web server on/off
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    web_server_toggle_requested = true;
}

// Initialize GPIO for configuration trigger
static void init_config_gpio(int gpio_pin)
{
    // Validate GPIO pin
    if (gpio_pin < 0 || gpio_pin > 39) {
        ESP_LOGW(TAG, "[CONFIG] Invalid trigger GPIO %d, using default GPIO %d", gpio_pin, CONFIG_GPIO_PIN);
        gpio_pin = CONFIG_GPIO_PIN;
    }
   // Configure GPIO 34 (main trigger) - pull-down, trigger on rising edge
    gpio_config_t io_conf_main = {
        .intr_type = CONFIG_GPIO_INTR_FLAG_34,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << gpio_pin),
        .pull_down_en = (gpio_pin == 34) ? 1 : 0,  // Pull-down for GPIO 34
        .pull_up_en = (gpio_pin == 34) ? 0 : 1     // Pull-up for other pins
    };
    gpio_config(&io_conf_main);

    // Configure BOOT button (GPIO 0) - pull-up, trigger on falling edge  
    gpio_config_t io_conf_boot = {
        .intr_type = CONFIG_GPIO_INTR_FLAG_0,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << CONFIG_GPIO_BOOT_PIN),
        .pull_down_en = 0,
        .pull_up_en = 1
    };
    gpio_config(&io_conf_boot);
    
    // Install GPIO ISR service (only once)
    static bool isr_service_installed = false;
    if (!isr_service_installed) {
        gpio_install_isr_service(0);
        isr_service_installed = true;
    }
    
    // Hook ISR handler for specific GPIO pin
    gpio_isr_handler_add(gpio_pin, gpio_isr_handler, NULL);
    gpio_isr_handler_add(CONFIG_GPIO_BOOT_PIN, gpio_isr_handler, NULL);
    
    ESP_LOGI(TAG, "[CORE] GPIO %d configured for web server toggle (connect to 3.3V)", gpio_pin);
    ESP_LOGI(TAG, "[CORE] GPIO 0 (BOOT button) configured for web server toggle (press button)");
}

// Initialize GPIO for modem reset
static void init_modem_reset_gpio(void)
{
    // Validate GPIO pin range
    if (modem_reset_gpio_pin < 0 || modem_reset_gpio_pin > 39) {
        ESP_LOGW(TAG, "[MODEM] Invalid GPIO pin %d, using default GPIO 2", modem_reset_gpio_pin);
        modem_reset_gpio_pin = 2;
    }
    
    // Skip certain pins that are typically reserved
   if (modem_reset_gpio_pin == 1 || modem_reset_gpio_pin == 6 ||
        modem_reset_gpio_pin == 7 || modem_reset_gpio_pin == 8 || modem_reset_gpio_pin == 9 || 
        modem_reset_gpio_pin == 10 || modem_reset_gpio_pin == 11) {
        ESP_LOGW(TAG, "[MODEM] GPIO %d is reserved, using default GPIO 2", modem_reset_gpio_pin);
        modem_reset_gpio_pin = 2;
    }
    
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << modem_reset_gpio_pin),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    esp_err_t result = gpio_config(&io_conf);
    
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "[MODEM] Failed to configure GPIO %d: %s", modem_reset_gpio_pin, esp_err_to_name(result));
        return;
    }
    
    // Set initial state to LOW (modem powered on)
    gpio_set_level(modem_reset_gpio_pin, 0);
    
    ESP_LOGI(TAG, "[MODEM] GPIO %d configured for modem reset control", modem_reset_gpio_pin);
}

// Perform modem reset with WiFi reconnection
static void perform_modem_reset(void)
{
    if (!modem_reset_enabled) {
        ESP_LOGI(TAG, "[MODEM] Modem reset disabled, skipping reset");
        return;
    }
    
    system_config_t* config = get_system_config();
    int boot_delay = (config->modem_boot_delay > 0) ? config->modem_boot_delay : 15; // Default 15 seconds
    
    ESP_LOGI(TAG, "[MODEM] Starting modem reset sequence...");
    
    // Step 1: Disconnect WiFi gracefully
    ESP_LOGI(TAG, "[MODEM] Disconnecting WiFi before modem reset");
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for WiFi to disconnect cleanly
    
    // Step 2: Reset modem power (2-second power cycle)
    ESP_LOGI(TAG, "[MODEM] Power cycling modem...");
    gpio_set_level(modem_reset_gpio_pin, 1);
    ESP_LOGI(TAG, "[MODEM] Power disconnected (GPIO %d HIGH)", modem_reset_gpio_pin);
    
    // Wait 2 seconds for power cycle
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Restore modem power
    gpio_set_level(modem_reset_gpio_pin, 0);
    ESP_LOGI(TAG, "[MODEM] Power restored (GPIO %d LOW)", modem_reset_gpio_pin);
    
    // Step 3: Wait for modem to boot up
    ESP_LOGI(TAG, "[MODEM] Waiting %d seconds for modem to boot up...", boot_delay);
    vTaskDelay(pdMS_TO_TICKS(boot_delay * 1000));
    
    // Step 4: Reconnect WiFi
    ESP_LOGI(TAG, "[MODEM] Attempting WiFi reconnection...");
    esp_err_t wifi_result = esp_wifi_connect();
    if (wifi_result == ESP_OK) {
        ESP_LOGI(TAG, "[MODEM] WiFi reconnection initiated successfully");
        
        // Wait up to 30 seconds for WiFi connection
        wifi_ap_record_t ap_info;
        int retry_count = 0;
        while (retry_count < 30) {
            esp_err_t status = esp_wifi_sta_get_ap_info(&ap_info);
            if (status == ESP_OK) {
                ESP_LOGI(TAG, "[MODEM] WiFi reconnected successfully to: %s", ap_info.ssid);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            retry_count++;
            
            if (retry_count % 5 == 0) {
                ESP_LOGI(TAG, "[MODEM] Still waiting for WiFi connection... (%d/30)", retry_count);
            }
        }
        
        if (retry_count >= 30) {
            ESP_LOGW(TAG, "[MODEM] WiFi reconnection timeout - check modem and network");
        }
    } else {
        ESP_LOGE(TAG, "[MODEM] Failed to initiate WiFi reconnection: %s", esp_err_to_name(wifi_result));
    }
    
    ESP_LOGI(TAG, "[MODEM] Modem reset sequence completed");
}

// Reinitialize modem reset GPIO with new pin
static esp_err_t reinit_modem_reset_gpio(int new_gpio_pin)
{
    // Reset current GPIO pin to input mode to release it
    if (modem_reset_gpio_pin >= 0 && modem_reset_gpio_pin <= 39) {
        gpio_reset_pin(modem_reset_gpio_pin);
        ESP_LOGI(TAG, "[MODEM] Released GPIO %d", modem_reset_gpio_pin);
    }
    
    // Update to new pin
    modem_reset_gpio_pin = new_gpio_pin;
    
    // Initialize new GPIO pin
    init_modem_reset_gpio();
    
    return ESP_OK;
}

// External function to update modem GPIO pin (called from web_config)
esp_err_t update_modem_gpio_pin(int new_gpio_pin)
{
    return reinit_modem_reset_gpio(new_gpio_pin);
}

// Modem reset task
static void modem_reset_task(void *pvParameters)
{
    ESP_LOGI(TAG, "[MODEM] Modem reset task started");
    
    // Perform the reset
    perform_modem_reset();
    
    // Task cleanup
    modem_reset_task_handle = NULL;
    vTaskDelete(NULL);
}

// Graceful shutdown of all tasks
// Function removed - no longer used in unified operation mode

// Switch to configuration mode without restart
// Function removed - no longer used in unified operation mode

// Function removed - no longer used in unified operation mode

// Start web server for configuration
static void start_web_server(void)
{
    if (!web_server_running) {
        ESP_LOGI(TAG, "[WEB] GPIO trigger detected - starting web server with SoftAP");
        esp_err_t ret = web_config_start_server_only();
        if (ret == ESP_OK) {
            web_server_running = true;
            update_led_status();  // Update LED to show web server is running
            ESP_LOGI(TAG, "[WEB] Web server started successfully with SoftAP");
            ESP_LOGI(TAG, "[ACCESS] Connect to WiFi: 'ModbusIoT-Config' (password: config123)");
            ESP_LOGI(TAG, "[ACCESS] Then visit: http://192.168.4.1 to configure");
        } else {
            ESP_LOGE(TAG, "[ERROR] Failed to start web server: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGI(TAG, "[WEB] Web server already running - ignoring trigger");
    }
}

// Stop web server and return to operation only
static void stop_web_server(void)
{
    if (web_server_running) {
        ESP_LOGI(TAG, "[WEB] GPIO trigger detected - stopping web server");
        web_config_stop();
        web_server_running = false;
        update_led_status();  // Update LED to show web server is stopped
        ESP_LOGI(TAG, "[WEB] Web server stopped - returning to operation mode");
    } else {
        ESP_LOGI(TAG, "[WEB] Web server not running - ignoring trigger");
    }
}

// Handle web server toggle based on current state
static void handle_web_server_toggle(void)
{
    if (web_server_running) {
        stop_web_server();
    } else {
        start_web_server();
    }
    
    // Reset the toggle flag
    web_server_toggle_requested = false;
}

// Initialize status LEDs
static void init_status_leds(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = ((1ULL << WEBSERVER_LED_GPIO_PIN) | 
                        (1ULL << MQTT_LED_GPIO_PIN) | 
                        (1ULL << SENSOR_LED_GPIO_PIN)),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);
    
    // Initialize all LEDs to OFF (HIGH)
    gpio_set_level(WEBSERVER_LED_GPIO_PIN, 1);
    gpio_set_level(MQTT_LED_GPIO_PIN, 1);
    gpio_set_level(SENSOR_LED_GPIO_PIN, 1);
    
    ESP_LOGI(TAG, "[LED] Status LEDs initialized - GPIO %d:%d:%d (LOW=ON)", 
             WEBSERVER_LED_GPIO_PIN, MQTT_LED_GPIO_PIN, SENSOR_LED_GPIO_PIN);
}

// Set LED state (LOW = ON, HIGH = OFF)
static void set_status_led(int gpio_pin, bool on)
{
    gpio_set_level(gpio_pin, on ? 0 : 1);
}

// Update all LED states based on system status
static void update_led_status(void)
{
    // Web server LED: ON when web server is running
    if (web_server_running != webserver_led_on) {
        webserver_led_on = web_server_running;
        set_status_led(WEBSERVER_LED_GPIO_PIN, webserver_led_on);
    }
    
    // MQTT LED: ON when MQTT is connected
    if (mqtt_connected != mqtt_led_on) {
        mqtt_led_on = mqtt_connected;
        set_status_led(MQTT_LED_GPIO_PIN, mqtt_led_on);
    }
    
    // Sensor LED: ON when sensors are responding
    if (sensors_responding != sensor_led_on) {
        sensor_led_on = sensors_responding;
        set_status_led(SENSOR_LED_GPIO_PIN, sensor_led_on);
    }
}

// Modbus reading task (Core 0)
static void modbus_task(void *pvParameters)
{
    ESP_LOGI(TAG, "[CONFIG] Modbus task started on core %d", xPortGetCoreID());
    
    while (1) {
        if (read_configured_sensors_data() == ESP_OK) {
            // Mark sensors as responding for LED status
            sensors_responding = true;
            
            // Send data to telemetry task via queue
            if (sensor_data_queue != NULL) {
                BaseType_t result = xQueueSend(sensor_data_queue, &current_flow_data, pdMS_TO_TICKS(100));
                if (result != pdTRUE) {
                    // Clear the queue if it's full and try again
                    ESP_LOGW(TAG, "[WARN] Queue full, clearing old data and retrying...");
                    xQueueReset(sensor_data_queue);
                    result = xQueueSend(sensor_data_queue, &current_flow_data, 0);
                    if (result != pdTRUE) {
                        ESP_LOGW(TAG, "[WARN] Still failed to send sensor data to queue");
                    } else {
                        ESP_LOGI(TAG, "[OK] Sensor data sent to queue after clearing");
                    }
                } else {
                    ESP_LOGI(TAG, "[OK] Sensor data sent to queue successfully");
                }
            }
        } else {
            // Mark sensors as not responding if read failed
            sensors_responding = false;
        }
        
        // Check for web server toggle request (handled by main monitoring loop)
        if (web_server_toggle_requested) {
            ESP_LOGI(TAG, "[WEB] Web server toggle requested via GPIO - signaling main loop");
            // Just set the flag, main loop will handle the transition
            // Don't break - continue operation while web server toggles
        }
        
        // Check for shutdown request
        if (system_shutdown_requested) {
            ESP_LOGI(TAG, "[CONFIG] Modbus task exiting due to shutdown request");
            modbus_task_handle = NULL;
            vTaskDelete(NULL);
            return;
        }
        
        vTaskDelay(pdMS_TO_TICKS(120000)); // Read every 2 minutes (120 seconds)
    }
    
    // Task exiting normally (due to config mode request)
    ESP_LOGI(TAG, "[CONFIG] Modbus task exiting normally");
    modbus_task_handle = NULL;
    vTaskDelete(NULL);
}

// MQTT handling task (Core 1)
static void mqtt_task(void *pvParameters)
{
    ESP_LOGI(TAG, "[NET] MQTT task started on core %d", xPortGetCoreID());
    
    // Initialize MQTT client
    if (initialize_mqtt_client() != 0) {
        ESP_LOGE(TAG, "[ERROR] Failed to initialize MQTT client");
        vTaskDelete(NULL);
        return;
    }
    
    while (1) {
        // Check for shutdown request only (web server toggle doesn't affect MQTT)
        if (system_shutdown_requested) {
            ESP_LOGI(TAG, "[NET] MQTT task exiting due to shutdown request");
            break;
        }
        
        // Handle MQTT events and maintain connection
        if (!mqtt_connected) {
            ESP_LOGW(TAG, "[WARN] MQTT disconnected, checking connection...");
        }

        // Check every 10 seconds to reduce power consumption
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
    
    // Task exiting normally
    ESP_LOGI(TAG, "[NET] MQTT task exiting normally");
    mqtt_task_handle = NULL;
    vTaskDelete(NULL);
}

// Telemetry sending task (Core 1)
static void telemetry_task(void *pvParameters)
{
    ESP_LOGI(TAG, "[DATA] Telemetry task started on core %d", xPortGetCoreID());
    
    system_config_t* config = get_system_config();
    TickType_t last_send_time = 0;
    bool first_telemetry_sent = false;
    
    while (1) {
        // Check for shutdown request only (web server toggle doesn't affect telemetry)
        if (system_shutdown_requested) {
            ESP_LOGI(TAG, "[DATA] Telemetry task exiting due to shutdown request");
            break;
        }
        
        TickType_t current_time = xTaskGetTickCount();
        
        // For first telemetry: wait for valid sensor data, then send immediately
        // For subsequent telemetry: follow normal interval
        bool should_send_telemetry = false;
        
        if (!first_telemetry_sent) {
            // First telemetry: check if we have valid sensor data from modbus task
            flow_meter_data_t received_data;
            if (xQueueReceive(sensor_data_queue, &received_data, pdMS_TO_TICKS(100)) == pdTRUE) {
                current_flow_data = received_data;
                ESP_LOGI(TAG, "[RECV] Received first sensor data from queue - sending immediate telemetry");
                should_send_telemetry = true;
                first_telemetry_sent = true;
                last_send_time = current_time;
            }
        } else {
            // Subsequent telemetry: follow normal interval
            // Check if enough time has passed since last successful send
            bool interval_ready = (current_time - last_send_time) >= pdMS_TO_TICKS(config->telemetry_interval * 1000);
            
            if (interval_ready) {
                // Try to receive fresh sensor data from queue (get the latest data)
                flow_meter_data_t received_data;
                bool got_fresh_data = false;
                
                // Clear all old data from queue and get the most recent
                while (xQueueReceive(sensor_data_queue, &received_data, 0) == pdTRUE) {
                    current_flow_data = received_data;
                    got_fresh_data = true;
                }
                
                if (got_fresh_data) {
                    ESP_LOGI(TAG, "[RECV] Received fresh sensor data from queue (cleared old data)");
                } else {
                    ESP_LOGI(TAG, "[RECV] No new sensor data, using previous data for telemetry");
                }
                
                should_send_telemetry = true;
                // DON'T update last_send_time here - wait for successful transmission
            }
        }
        
        if (should_send_telemetry) {
            // Always call send_telemetry() - it will handle SD caching if MQTT is disconnected
            bool telemetry_success = send_telemetry();
            // Update last_send_time AFTER successful telemetry call
            if (telemetry_success && first_telemetry_sent) {
                // For subsequent telemetries, only update timestamp after successful send
                last_send_time = current_time;
                ESP_LOGI(TAG, "[OK] Telemetry timestamp updated after successful send");
            } else if (!telemetry_success) {
                ESP_LOGW(TAG, "[WARN] Telemetry failed - timestamp not updated, will retry on next interval");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000)); // Increased to 5 seconds to prevent timing edge cases
    }
    
    // Task exiting normally
    ESP_LOGI(TAG, "[DATA] Telemetry task exiting normally");
    telemetry_task_handle = NULL;
    vTaskDelete(NULL);
}

static bool send_telemetry(void) {
    static TickType_t last_actual_send_time = 0;
    static uint32_t call_counter = 0;
    static bool send_in_progress = false;
    TickType_t current_time = xTaskGetTickCount();
    
    call_counter++;
    ESP_LOGI(TAG, "[TRACK] send_telemetry() called #%lu, time=%lu, last_send=%lu, mqtt_connected=%d", 
             call_counter, current_time, last_actual_send_time, mqtt_connected);
    
    // Check if a send is already in progress
    if (send_in_progress) {
        ESP_LOGW(TAG, "[WARN] Telemetry send already in progress, skipping duplicate call #%lu", call_counter);
        return false;
    }
    
    // Prevent duplicate sends within 10 seconds (increased safety window)
    if (last_actual_send_time != 0 && 
        (current_time - last_actual_send_time) < pdMS_TO_TICKS(10000)) {
        ESP_LOGW(TAG, "[WARN] Telemetry send attempted too soon (%.1f sec ago), skipping call #%lu", 
                (current_time - last_actual_send_time) * portTICK_PERIOD_MS / 1000.0, call_counter);
        return false;
    }
    
    send_in_progress = true;
    
    // Check network connectivity first
    system_config_t* config = get_system_config();
    bool network_available = is_network_connected();

    if (!network_available) {
        ESP_LOGW(TAG, "[WARN] ⚠️ Network not connected");

        // If SD card is enabled and caching is enabled, cache the telemetry
        if (config->sd_config.enabled && config->sd_config.cache_on_failure) {
            ESP_LOGI(TAG, "[SD] 💾 Caching telemetry to SD card (network unavailable)...");

            // Generate the telemetry payload first
            snprintf(telemetry_topic, sizeof(telemetry_topic),
                     "devices/%s/messages/events/", config->azure_device_id);
            create_telemetry_payload(telemetry_payload, sizeof(telemetry_payload));

            if (strlen(telemetry_payload) > 0) {
                // Generate timestamp for SD card message
                time_t now = time(NULL);
                struct tm timeinfo;
                gmtime_r(&now, &timeinfo);
                char timestamp[32];
                strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

                esp_err_t ret = sd_card_save_message(telemetry_topic, telemetry_payload, timestamp);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "[SD] ✅ Telemetry cached to SD card - will replay when network reconnects");
                    send_in_progress = false;
                    last_actual_send_time = current_time; // Update to respect telemetry interval
                    // Return FALSE to indicate not sent to cloud (only cached locally)
                    // Telemetry task will retry when network comes back online
                    return false;
                } else {
                    ESP_LOGE(TAG, "[SD] ❌ Failed to cache telemetry: %s", esp_err_to_name(ret));
                }
            }
        }

        send_in_progress = false;
        return false;
    }

    // Check if MQTT client and connection are valid
    if (!mqtt_client) {
        ESP_LOGE(TAG, "[ERROR] MQTT client not initialized - skipping telemetry send");
        send_in_progress = false;
        return false;
    }

    if (!mqtt_connected) {
        ESP_LOGW(TAG, "[WARN]  MQTT not connected");

        // Debug: Log SD configuration status
        ESP_LOGI(TAG, "[SD] DEBUG: SD enabled=%d, cache_on_failure=%d",
                 config->sd_config.enabled, config->sd_config.cache_on_failure);

        // If SD card caching is enabled, cache the telemetry
        if (config->sd_config.enabled && config->sd_config.cache_on_failure) {
            ESP_LOGI(TAG, "[SD] 💾 Caching telemetry to SD card (MQTT disconnected)...");

            // Generate the telemetry payload first
            snprintf(telemetry_topic, sizeof(telemetry_topic),
                     "devices/%s/messages/events/", config->azure_device_id);
            create_telemetry_payload(telemetry_payload, sizeof(telemetry_payload));

            if (strlen(telemetry_payload) > 0) {
                // Generate timestamp for SD card message
                time_t now = time(NULL);
                struct tm timeinfo;
                gmtime_r(&now, &timeinfo);
                char timestamp[32];
                strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

                esp_err_t ret = sd_card_save_message(telemetry_topic, telemetry_payload, timestamp);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "[SD] ✅ Telemetry cached to SD card - will replay when MQTT reconnects");
                    send_in_progress = false;
                    last_actual_send_time = current_time; // Update to respect telemetry interval
                    return false;
                } else {
                    ESP_LOGE(TAG, "[SD] ❌ Failed to cache telemetry: %s", esp_err_to_name(ret));
                }
            }
        } else {
            if (!config->sd_config.enabled) {
                ESP_LOGW(TAG, "[SD] SD card is DISABLED in configuration - enable it in web portal");
            } else if (!config->sd_config.cache_on_failure) {
                ESP_LOGW(TAG, "[SD] SD caching is DISABLED - enable 'Cache Messages When Network Unavailable' in web portal");
            }
        }

        ESP_LOGW(TAG, "[WARN] Skipping telemetry - no MQTT connection and no SD caching");
        send_in_progress = false;
        return false;
    }
    
    ESP_LOGI(TAG, "[SEND] Sending telemetry message #%lu...", telemetry_send_count);
    
    // Data is now provided by the modbus task via queue

    // Use Azure IoT Hub D2C telemetry topic format
    snprintf(telemetry_topic, sizeof(telemetry_topic), 
             "devices/%s/messages/events/", config->azure_device_id);
    
    // Validate topic format
    if (strlen(telemetry_topic) == 0 || !strstr(telemetry_topic, "devices/")) {
        ESP_LOGE(TAG, "[ERROR] Invalid telemetry topic format: %s", telemetry_topic);
        send_in_progress = false;
        return false;
    }

    create_telemetry_payload(telemetry_payload, sizeof(telemetry_payload));

    // Check if payload is empty (no valid sensor data)
    if (strlen(telemetry_payload) == 0) {
        ESP_LOGW(TAG, "[WARN] No sensor data available, skipping telemetry transmission");
        send_in_progress = false;
        return false;
    }

    ESP_LOGI(TAG, "[LOC] Topic: %s", telemetry_topic);
    ESP_LOGI(TAG, "[PKG] Payload: %s", telemetry_payload);
    ESP_LOGI(TAG, "[PKG] Payload Length: %d bytes", strlen(telemetry_payload));
    ESP_LOGI(TAG, "[KEY] Using SAS Token: %.50s...", sas_token);
    ESP_LOGI(TAG, "[NET] Device ID: %s", config->azure_device_id);
    ESP_LOGI(TAG, "[HUB] IoT Hub: %s", IOT_CONFIG_IOTHUB_FQDN);
    ESP_LOGI(TAG, "[LINK] MQTT Connected: %s", mqtt_connected ? "YES" : "NO");
    
    // Check payload size limit (Azure IoT Hub has 256KB limit)
    if (strlen(telemetry_payload) > 262144) {
        ESP_LOGE(TAG, "[ERROR] Payload too large: %d bytes (max 256KB)", strlen(telemetry_payload));
        send_in_progress = false;
        return false;
    }
    
    // Try QoS 0 for compatibility with Arduino 1.0.6
    int msg_id = esp_mqtt_client_publish(
        mqtt_client,
        telemetry_topic,
        telemetry_payload,
        strlen(telemetry_payload),
        0,  // QoS 0 for Arduino 1.0.6 compatibility
        0   // DO_NOT_RETAIN_MSG
    );

    if (msg_id == -1) {
        ESP_LOGE(TAG, "[ERROR] FAILED to publish telemetry - MQTT client error");
        ESP_LOGE(TAG, "   Check: MQTT connection, topic format, payload size");
        ESP_LOGE(TAG, "   Topic: %s", telemetry_topic);
        ESP_LOGE(TAG, "   Payload size: %d bytes", strlen(telemetry_payload));
        ESP_LOGE(TAG, "   MQTT connected: %s", mqtt_connected ? "YES" : "NO");
        
        // Try to reconnect MQTT if disconnected
        if (!mqtt_connected) {
            ESP_LOGW(TAG, "[WARN] Attempting MQTT reconnection...");
            esp_mqtt_client_reconnect(mqtt_client);
        }
        send_in_progress = false; // Reset flag on failure
        return false;
    } else {
        ESP_LOGI(TAG, "[OK] Telemetry queued for publish, msg_id=%d", msg_id);
        ESP_LOGI(TAG, "   Waiting for MQTT_EVENT_PUBLISHED confirmation...");
        telemetry_send_count++;
        last_actual_send_time = current_time; // Record successful send time

        // Log detailed publish info
        ESP_LOGI(TAG, "[SEND] Published to Azure IoT Hub:");
        ESP_LOGI(TAG, "   Topic: %s", telemetry_topic);
        ESP_LOGI(TAG, "   Message ID: %d", msg_id);
        ESP_LOGI(TAG, "   Payload: %.200s%s", telemetry_payload, strlen(telemetry_payload) > 200 ? "..." : "");

        // After successful send, try to replay any cached messages from SD card
        if (config->sd_config.enabled) {
            ESP_LOGI(TAG, "[SD] 📤 Checking for cached messages to replay...");
            esp_err_t replay_ret = sd_card_replay_messages(replay_message_callback);
            if (replay_ret == ESP_OK) {
                ESP_LOGI(TAG, "[SD] ✅ Cached messages replayed successfully");
            } else if (replay_ret == ESP_ERR_NOT_FOUND) {
                ESP_LOGD(TAG, "[SD] No cached messages to replay");
            } else {
                ESP_LOGW(TAG, "[SD] ⚠️ Failed to replay cached messages: %s", esp_err_to_name(replay_ret));
            }
        }

        send_in_progress = false; // Reset flag on success
        return true;
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "[START] Starting Unified Modbus IoT Operation System");
    
    // Initialize system uptime tracking
    system_uptime_start = esp_timer_get_time() / 1000000;

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize web configuration system
    ESP_LOGI(TAG, "[WEB] Initializing web configuration system...");
    ret = web_config_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[ERROR] Failed to initialize web config: %s", esp_err_to_name(ret));
        return;
    }

    // Get system configuration
    system_config_t* config = get_system_config();

    // Initialize status LEDs early so they can be used in both SETUP and OPERATION modes
    init_status_leds();

    // Check if configuration is complete, otherwise start in setup mode
    if (config->config_complete) {
        ESP_LOGI(TAG, "[SYS] Configuration complete - Starting in OPERATION mode");
        set_config_state(CONFIG_STATE_OPERATION);
    } else {
        ESP_LOGI(TAG, "[SYS] No configuration found - Starting in SETUP mode with web server");
        set_config_state(CONFIG_STATE_SETUP);

        // Initialize WiFi infrastructure first
        ESP_LOGI(TAG, "[WEB] Initializing WiFi for web server...");
        ret = web_config_start_ap_mode();
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "[ERROR] Failed to initialize WiFi: %s", esp_err_to_name(ret));
            return;
        }

        // Initialize Modbus for sensor testing
        ESP_LOGI(TAG, "[WEB] Initializing Modbus for sensor testing...");
        ret = modbus_init();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "[WARN] Modbus initialization failed - sensor testing will not work");
        }

        // Now start the web server with SoftAP
        ESP_LOGI(TAG, "[WEB] Starting web server with SoftAP...");
        ret = web_config_start_server_only();
        if (ret == ESP_OK) {
            web_server_running = true;
            update_led_status();  // Turn on LED to indicate config mode is active
            ESP_LOGI(TAG, "[WEB] ✅ Web server started successfully");
            ESP_LOGI(TAG, "[ACCESS] Connect to WiFi: 'ModbusIoT-Config' (password: config123)");
            ESP_LOGI(TAG, "[ACCESS] Then visit: http://192.168.4.1 to configure");

            // Keep the system running by not returning - web server is active
            // The main task will complete but FreeRTOS keeps the system alive
        } else {
            ESP_LOGE(TAG, "[ERROR] Failed to start web server: %s", esp_err_to_name(ret));
            return;
        }

        // Don't continue to normal operation - stay in setup mode
        return;
    }

    // Log Azure configuration loaded from NVS
    ESP_LOGI(TAG, "[AZURE CONFIG] Loaded from NVS:");
    ESP_LOGI(TAG, "  - Device ID: %s", config->azure_device_id);
    ESP_LOGI(TAG, "  - Device Key (first 10 chars): %.10s...", config->azure_device_key);
    ESP_LOGI(TAG, "  - Device Key Length: %d", strlen(config->azure_device_key));
    ESP_LOGI(TAG, "  - Telemetry Interval: %d seconds", config->telemetry_interval);
    ESP_LOGI(TAG, "  - Network Mode: %s", config->network_mode == NETWORK_MODE_WIFI ? "WiFi" : "SIM Module");

    // Initialize RTC if enabled
    if (config->rtc_config.enabled) {
        ESP_LOGI(TAG, "[RTC] 🕐 Initializing DS3231 Real-Time Clock...");
        esp_err_t rtc_ret = ds3231_init();
        if (rtc_ret == ESP_OK) {
            ESP_LOGI(TAG, "[RTC] ✅ RTC initialized successfully");

            // Sync system time FROM RTC immediately (before network connects)
            // This ensures timestamps are accurate even if network fails
            rtc_ret = ds3231_sync_system_time();
            if (rtc_ret == ESP_OK) {
                time_t now = time(NULL);
                struct tm timeinfo;
                gmtime_r(&now, &timeinfo);
                char time_str[32];
                strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
                ESP_LOGI(TAG, "[RTC] ✅ System time synced from RTC: %s UTC", time_str);
            } else {
                ESP_LOGW(TAG, "[RTC] ⚠️ Could not sync system time from RTC");
            }
        } else {
            ESP_LOGW(TAG, "[RTC] ⚠️ RTC initialization failed: %s (optional feature - continuing)",
                     esp_err_to_name(rtc_ret));
        }
    } else {
        ESP_LOGI(TAG, "[RTC] RTC disabled in configuration");
    }

    // Initialize SD card if enabled
    ESP_LOGI(TAG, "[SD] 🔧 SD Config: enabled=%d, cache_on_failure=%d",
             config->sd_config.enabled, config->sd_config.cache_on_failure);

    if (config->sd_config.enabled) {
        ESP_LOGI(TAG, "[SD] 💾 Initializing SD Card for offline data caching...");
        esp_err_t sd_ret = sd_card_init();
        if (sd_ret == ESP_OK) {
            ESP_LOGI(TAG, "[SD] ✅ SD card mounted successfully");
            ESP_LOGI(TAG, "[SD] 📊 Caching enabled: %s", config->sd_config.cache_on_failure ? "YES" : "NO");
        } else {
            ESP_LOGW(TAG, "[SD] ⚠️ SD card mount failed: %s", esp_err_to_name(sd_ret));
            ESP_LOGW(TAG, "[SD] System will continue without offline caching");
            config->sd_config.enabled = false; // Disable SD if mount fails
        }
    } else {
        ESP_LOGI(TAG, "[SD] SD card logging disabled in configuration");
    }
    
    // Load modem reset settings from configuration
    modem_reset_enabled = config->modem_reset_enabled;
    modem_reset_gpio_pin = (config->modem_reset_gpio_pin > 0) ? config->modem_reset_gpio_pin : 2;
    
    // Initialize GPIO for web server toggle (use configured pin)
    int trigger_gpio = (config->trigger_gpio_pin > 0) ? config->trigger_gpio_pin : CONFIG_GPIO_PIN;
    ESP_LOGI(TAG, "[WEB] GPIO %d configured for web server toggle", trigger_gpio);
    init_config_gpio(trigger_gpio);
    
    // Initialize GPIO for modem reset
    init_modem_reset_gpio();
    
    // Initialize Modbus RS485 communication
    ESP_LOGI(TAG, "[CONFIG] Initializing Modbus RS485 communication...");
    ret = modbus_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[ERROR] Failed to initialize Modbus: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "[WARN] System will continue with simulated data only");
    } else {
        ESP_LOGI(TAG, "[OK] Modbus RS485 initialized successfully");
        
        // Test all configured sensors
        ESP_LOGI(TAG, "[TEST] Testing %d configured sensors...", config->sensor_count);
        for (int i = 0; i < config->sensor_count; i++) {
            if (config->sensors[i].enabled) {
                ESP_LOGI(TAG, "Testing sensor %d: %s (Unit: %s)", 
                         i + 1, config->sensors[i].name, config->sensors[i].unit_id);
                // TODO: Test individual sensor
            }
        }
    }
    
    // Create queue for sensor data communication between tasks
    sensor_data_queue = xQueueCreate(5, sizeof(flow_meter_data_t));
    if (sensor_data_queue == NULL) {
        ESP_LOGE(TAG, "[ERROR] Failed to create sensor data queue");
        return;
    }

    // Initialize WiFi stack (always needed for web config AP mode)
    ret = web_config_start_ap_mode();  // This actually starts STA mode for normal operation
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "[WARN] WiFi initialization had issues - some features may not work");
        // Don't return - allow system to continue for modbus-only or SIM operation
    }

    // Network initialization based on configured mode (WiFi or SIM)
    ESP_LOGI(TAG, "[NET] ═══════════════════════════════════════════════════════");
    ESP_LOGI(TAG, "[NET] 🌐 Initializing Network Connection");
    ESP_LOGI(TAG, "[NET] ═══════════════════════════════════════════════════════");

    if (config->network_mode == NETWORK_MODE_WIFI) {
        // WiFi Mode - Check if WiFi was initialized successfully
        if (strlen(config->wifi_ssid) == 0) {
            ESP_LOGW(TAG, "[WIFI] ⚠️ WiFi SSID not configured");
            ESP_LOGI(TAG, "[WIFI] 💡 To use WiFi:");
            ESP_LOGI(TAG, "[WIFI]    1. Configure WiFi via web interface");
            ESP_LOGI(TAG, "[WIFI]    2. Or switch to SIM module mode");
            ESP_LOGI(TAG, "[WIFI] System will operate in offline mode (Modbus only)");
        } else {
            ESP_LOGI(TAG, "[WIFI] ✅ WiFi STA mode already configured by web_config");
            ESP_LOGI(TAG, "[WIFI] ⏳ Waiting for WiFi connection to %s...", config->wifi_ssid);

            // Just wait for connection to complete
            int retry = 0;
            while (retry < 30) {
                wifi_ap_record_t ap_info;
                if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                    ESP_LOGI(TAG, "[WIFI] ✅ Connected successfully");
                    ESP_LOGI(TAG, "[WIFI] 📊 Signal Strength: %d dBm", ap_info.rssi);
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
                retry++;
            }

            if (retry >= 30) {
                ESP_LOGW(TAG, "[WIFI] ⚠️ Connection timeout - continuing in offline mode");
                ESP_LOGW(TAG, "[WIFI] System will cache telemetry to SD card if enabled");
            }
        }

    } else if (config->network_mode == NETWORK_MODE_SIM) {
        // SIM Mode - Direct A7670C initialization
        ESP_LOGI(TAG, "[SIM] 📱 Starting SIM module (A7670C)...");

        ppp_config_t ppp_config = {
            .uart_num = config->sim_config.uart_num,
            .tx_pin = config->sim_config.uart_tx_pin,
            .rx_pin = config->sim_config.uart_rx_pin,
            .pwr_pin = config->sim_config.pwr_pin,
            .reset_pin = config->sim_config.reset_pin,
            .baud_rate = config->sim_config.uart_baud_rate,
            .apn = config->sim_config.apn,
            .user = config->sim_config.apn_user,
            .pass = config->sim_config.apn_pass,
        };

        ret = a7670c_ppp_init(&ppp_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[SIM] ❌ Failed to initialize A7670C: %s", esp_err_to_name(ret));
            ESP_LOGW(TAG, "[SIM] Entering offline mode");
        } else {
            ret = a7670c_ppp_connect();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "[SIM] ❌ Failed to connect PPP: %s", esp_err_to_name(ret));
                ESP_LOGW(TAG, "[SIM] Entering offline mode");
            } else {
                // Wait for PPP connection with timeout
                ESP_LOGI(TAG, "[SIM] ⏳ Waiting for PPP connection...");
                int retry = 0;
                while (retry < 60) {
                    if (a7670c_is_connected()) {
                        ESP_LOGI(TAG, "[SIM] ✅ PPP connection established");

                        // Get stored signal strength (checked before PPP mode)
                        // IMPORTANT: Cannot use AT commands once in PPP mode!
                        signal_strength_t signal;
                        if (a7670c_get_stored_signal_strength(&signal) == ESP_OK) {
                            ESP_LOGI(TAG, "[SIM] 📊 Signal Strength: %d dBm (%s)",
                                     signal.rssi_dbm, signal.quality ? signal.quality : "Unknown");
                            ESP_LOGI(TAG, "[SIM] 📡 Operator: %s", signal.operator_name);
                        }
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    retry++;
                }

                if (retry >= 60) {
                    ESP_LOGW(TAG, "[SIM] ⚠️ PPP connection timeout - entering offline mode");
                    ESP_LOGW(TAG, "[SIM] System will cache telemetry to SD card if enabled");
                }
            }
        }
    }

    // Initialize SNTP time synchronization (always run, will timeout gracefully if network unavailable)
    ESP_LOGI(TAG, "[TIME] 🕐 Initializing SNTP time synchronization...");
    initialize_time();

    // If RTC is enabled, sync it with NTP time
    if (config->rtc_config.enabled && is_network_connected()) {
        ESP_LOGI(TAG, "[RTC] 🔄 Syncing RTC with NTP time...");
        esp_err_t sync_ret = ds3231_update_from_system_time();
        if (sync_ret == ESP_OK) {
            ESP_LOGI(TAG, "[RTC] ✅ RTC synchronized with NTP");
        } else {
            ESP_LOGW(TAG, "[RTC] ⚠️ Failed to sync RTC: %s", esp_err_to_name(sync_ret));
        }
    }

    // Wait a bit to ensure time is properly synchronized
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "[START] Starting dual-core task distribution...");
    
    // Create Modbus task on Core 0 (dedicated for sensor reading)
    BaseType_t modbus_result = xTaskCreatePinnedToCore(
        modbus_task,
        "modbus_task",
        8192,  // Increased from 4096 to handle sensor_manager integration
        NULL,
        5,  // High priority for sensor reading
        &modbus_task_handle,
        0   // Core 0
    );
    
    if (modbus_result != pdPASS) {
        ESP_LOGE(TAG, "[ERROR] Failed to create Modbus task");
        return;
    }
    
    // Create MQTT task on Core 1 (handles connectivity)
    BaseType_t mqtt_result = xTaskCreatePinnedToCore(
        mqtt_task,
        "mqtt_task",
        8192,
        NULL,
        4,  // Medium priority
        &mqtt_task_handle,
        1   // Core 1
    );
    
    if (mqtt_result != pdPASS) {
        ESP_LOGE(TAG, "[ERROR] Failed to create MQTT task");
        return;
    }
    
    // Create Telemetry task on Core 1 (handles data transmission)
    BaseType_t telemetry_result = xTaskCreatePinnedToCore(
        telemetry_task,
        "telemetry_task",
        8192,  // Increased stack size to prevent overflow
        NULL,
        3,  // Lower priority than MQTT
        &telemetry_task_handle,
        1   // Core 1
    );
    
    if (telemetry_result != pdPASS) {
        ESP_LOGE(TAG, "[ERROR] Failed to create Telemetry task");
        return;
    }
    
    ESP_LOGI(TAG, "[OK] All tasks created successfully");
    ESP_LOGI(TAG, "[CORE] Modbus reading: Core 0 (priority 5)");
    ESP_LOGI(TAG, "[NET] MQTT handling: Core 1 (priority 4)");
    ESP_LOGI(TAG, "[DATA] Telemetry sending: Core 1 (priority 3)");
    ESP_LOGI(TAG, "[WEB] GPIO %d: Pull LOW to toggle web server ON/OFF", trigger_gpio);
    
    // Main monitoring loop with web server toggle support
    while (1) {
        // Check for web server toggle request
        if (web_server_toggle_requested) {
            ESP_LOGI(TAG, "[WEB] GPIO %d trigger detected - toggling web server", trigger_gpio);
            handle_web_server_toggle();
        }
        
        // Update LED status based on current system state
        update_led_status();
        
        // Log system status every 30 seconds
        static uint32_t last_status_log = 0;
        uint32_t current_time_ms = esp_timer_get_time() / 1000;
        
        if (current_time_ms - last_status_log > 30000) {
            ESP_LOGI(TAG, "[DATA] System Status:");
            ESP_LOGI(TAG, "   MQTT: %s | Messages: %lu | Sensors: %d", 
                     mqtt_connected ? "CONNECTED" : "DISCONNECTED", 
                     telemetry_send_count, config->sensor_count);
            ESP_LOGI(TAG, "   Web Server: %s | GPIO Trigger: %d", 
                     web_server_running ? "RUNNING" : "STOPPED", trigger_gpio);
            ESP_LOGI(TAG, "   LEDs: WebServer=%s | MQTT=%s | Sensors=%s", 
                     webserver_led_on ? "ON" : "OFF",
                     mqtt_led_on ? "ON" : "OFF", 
                     sensor_led_on ? "ON" : "OFF");
            ESP_LOGI(TAG, "   Free heap: %lu bytes | Min free: %lu bytes",
                     esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
            ESP_LOGI(TAG, "   Tasks running: Modbus=%s, MQTT=%s, Telemetry=%s",
                     (modbus_task_handle != NULL) ? "OK" : "FAIL",
                     (mqtt_task_handle != NULL) ? "OK" : "FAIL",
                     (telemetry_task_handle != NULL) ? "OK" : "FAIL");
            last_status_log = current_time_ms;
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000)); // Check every 5 seconds
    }
}