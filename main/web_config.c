// web_config.c - Web-based configuration implementation
// Fluxgen ESP32 Modbus IoT Gateway - Version 1.0.0 (Production Ready - Complete Implementation)
// Professional industrial IoT gateway with real-time RS485 Modbus communication

#include "web_config.h"
#include "modbus.h"
#include "sensor_manager.h"
#include "iot_configs.h"  // For hardcoded values
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "cJSON.h"
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "a7670c_ppp.h"
#include "sd_card_logger.h"
#include "ds3231_rtc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <time.h>
#include "esp_flash.h"
#include "esp_partition.h"
#include "driver/spi_common.h"
#include "driver/i2c.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"
#include "driver/gpio.h"
#include "esp_task_wdt.h"

// Define MIN macro if not available
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "WEB_CONFIG";

// External declarations for MQTT/Azure status from main.c
extern bool mqtt_connected;
extern uint32_t total_telemetry_sent;
extern uint32_t mqtt_reconnect_count;
extern int64_t mqtt_connect_time;
extern int64_t last_telemetry_time;

// LOGO CONFIGURATION - Edit these values to customize your logo
#define COMPANY_NAME "Fluxgen"
#define COMPANY_TAGLINE "Building a Water Positive Future"
#define LOGO_COLOR_1 "#0066cc"  // Fluxgen Blue
#define LOGO_COLOR_2 "#00aaff"  // Fluxgen Light Blue

// Global configuration
static system_config_t g_system_config = {0};
static config_state_t g_config_state = CONFIG_STATE_SETUP;
static httpd_handle_t g_server = NULL;
static bool g_web_server_auto_start = false;
// static esp_netif_t *g_netif_sta = NULL;  // Unused variable
// static esp_netif_t *g_netif_ap = NULL;  // Reserved for future AP mode

// SIM test status tracking
typedef struct {
    bool in_progress;
    bool completed;
    bool success;
    char ip[32];
    int signal;
    char signal_quality[32];
    char operator_name[64];
    char apn[64];
    char error[256];
} sim_test_status_t;

static sim_test_status_t g_sim_test_status = {0};
static SemaphoreHandle_t g_sim_test_mutex = NULL;

// HTML templates
static const char* html_header = 
"<!DOCTYPE html><html><head>"
"<meta charset=UTF-8>"
"<title>FLUXGEN IoT Gateway</title>"
"<link href='//fonts.googleapis.com/css2?family=Orbitron:wght@400;700&family=Rajdhani:wght@400;600&display=swap' rel=stylesheet>"
"<meta name=viewport content='width=device-width,initial-scale=1'>"
"<style>"
"/* ===== DESIGN SYSTEM: CSS VARIABLES ===== */"
":root{"
"/* Color Palette - Professional Industrial IoT */"
"--primary-900:#0c2d5e;--primary-800:#1e40af;--primary-700:#1d4ed8;--primary-600:#2563eb;--primary-500:#3b82f6;--primary-400:#60a5fa;--primary-300:#93c5fd;--primary-200:#bfdbfe;--primary-100:#dbeafe;"
"--accent-600:#0891b2;--accent-500:#06b6d4;--accent-400:#22d3ee;--accent-300:#67e8f9;"
"--success-600:#059669;--success-500:#10b981;--success-400:#34d399;"
"--warning-600:#d97706;--warning-500:#f59e0b;--warning-400:#fbbf24;"
"--error-600:#dc2626;--error-500:#ef4444;--error-400:#f87171;"
"--gray-900:#0f172a;--gray-800:#1e293b;--gray-700:#334155;--gray-600:#475569;--gray-500:#64748b;--gray-400:#94a3b8;--gray-300:#cbd5e1;--gray-200:#e2e8f0;--gray-100:#f1f5f9;--gray-50:#f8fafc;"
"/* Semantic Colors */"
"--color-primary:var(--primary-600);--color-primary-hover:var(--primary-700);--color-primary-light:var(--primary-100);"
"--color-accent:var(--accent-500);--color-accent-hover:var(--accent-600);"
"--color-success:var(--success-500);--color-warning:var(--warning-500);--color-error:var(--error-500);"
"--color-text-primary:var(--gray-900);--color-text-secondary:var(--gray-700);--color-text-tertiary:var(--gray-500);--color-text-disabled:var(--gray-400);"
"--color-bg-primary:#ffffff;--color-bg-secondary:var(--gray-50);--color-bg-tertiary:var(--gray-100);"
"--color-border-light:var(--gray-200);--color-border-medium:var(--gray-300);--color-border-dark:var(--gray-400);"
"/* Glass & Effects */"
"--glass-white:rgba(255,255,255,0.85);--glass-white-light:rgba(255,255,255,0.6);--glass-dark:rgba(15,23,42,0.6);"
"--shadow-xs:0 1px 2px rgba(0,0,0,0.05);--shadow-sm:0 2px 8px rgba(0,0,0,0.08);--shadow-md:0 4px 16px rgba(0,0,0,0.12);--shadow-lg:0 8px 32px rgba(0,0,0,0.16);--shadow-xl:0 20px 60px rgba(0,0,0,0.24);"
"--glow-primary:rgba(37,99,235,0.4);--glow-accent:rgba(6,182,212,0.4);--glow-success:rgba(16,185,129,0.4);"
"/* Spacing Scale (Mobile-First) */"
"--space-xs:4px;--space-sm:8px;--space-md:16px;--space-lg:24px;--space-xl:32px;--space-2xl:48px;--space-3xl:64px;"
"/* Border Radius */"
"--radius-sm:6px;--radius-md:12px;--radius-lg:16px;--radius-xl:20px;--radius-2xl:24px;--radius-full:9999px;"
"/* Typography Scale */"
"--text-xs:0.75rem;--text-sm:0.875rem;--text-base:1rem;--text-lg:1.125rem;--text-xl:1.25rem;--text-2xl:1.5rem;--text-3xl:1.875rem;--text-4xl:2.25rem;"
"/* Font Weights */"
"--weight-normal:400;--weight-medium:500;--weight-semibold:600;--weight-bold:700;--weight-extrabold:800;--weight-black:900;"
"/* Line Heights */"
"--leading-tight:1.25;--leading-normal:1.5;--leading-relaxed:1.625;"
"/* Z-Index Layers */"
"--z-base:0;--z-dropdown:100;--z-sticky:200;--z-modal-backdrop:1000;--z-modal:1001;--z-tooltip:1200;"
"/* Transitions */"
"--transition-fast:150ms cubic-bezier(0.4,0,0.2,1);--transition-base:300ms cubic-bezier(0.4,0,0.2,1);--transition-slow:500ms cubic-bezier(0.4,0,0.2,1);"
"}"
"/* ===== RESET & BASE STYLES ===== */"
"*,*::before,*::after{margin:0;padding:0;box-sizing:border-box}"
"html{font-size:16px;-webkit-font-smoothing:antialiased;-moz-osx-font-smoothing:grayscale;scroll-behavior:smooth;overflow-x:hidden;width:100%;max-width:100vw}"
"body{font-family:Rajdhani,sans-serif;font-weight:var(--weight-normal);line-height:var(--leading-normal);color:var(--color-text-primary);background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;overflow-x:hidden;position:relative;width:100%;max-width:100vw}"
"body::before{content:'';position:fixed;top:0;left:0;right:0;bottom:0;background:url('data:image/svg+xml,%3Csvg width=\"60\" height=\"60\" viewBox=\"0 0 60 60\" xmlns=\"http://www.w3.org/2000/svg\"%3E%3Cg fill=\"none\" fill-rule=\"evenodd\"%3E%3Cg fill=\"%23ffffff\" fill-opacity=\"0.03\"%3E%3Cpath d=\"M36 34v-4h-2v4h-4v2h4v4h2v-4h4v-2h-4zm0-30V0h-2v4h-4v2h4v4h2V6h4V4h-4zM6 34v-4H4v4H0v2h4v4h2v-4h4v-2H6zM6 4V0H4v4H0v2h4v4h2V6h4V4H6z\"/%3E%3C/g%3E%3C/g%3E%3C/svg%3E');z-index:0;pointer-events:none;will-change:auto}"
"/* ===== TYPOGRAPHY ===== */"
"h1,h2,h3,h4,h5,h6{font-family:Orbitron,monospace;font-weight:var(--weight-bold);line-height:var(--leading-tight);color:var(--color-primary);margin-bottom:var(--space-md)}"
"h1{font-size:var(--text-4xl);font-weight:var(--weight-black);letter-spacing:-0.02em}"
"h2{font-size:var(--text-3xl);font-weight:var(--weight-extrabold)}"
"h3{font-size:var(--text-2xl);font-weight:var(--weight-bold)}"
"h4{font-size:var(--text-xl);font-weight:var(--weight-semibold)}"
"p{margin-bottom:var(--space-md);line-height:var(--leading-relaxed)}"
"strong,b{font-weight:var(--weight-semibold);color:var(--color-text-primary)}"
"small{font-size:var(--text-sm);color:var(--color-text-tertiary)}"
"/* ===== FORM ELEMENTS ===== */"
"input,select,textarea{width:100%;padding:var(--space-md);border:2px solid var(--color-border-light);border-radius:var(--radius-md);background:var(--color-bg-primary);color:var(--color-text-primary);font-family:Rajdhani,sans-serif;font-size:var(--text-base);transition:all var(--transition-base);box-shadow:var(--shadow-xs)}"
"input:hover,select:hover,textarea:hover{border-color:var(--color-border-medium)}"
"input:focus,select:focus,textarea:focus{outline:0;border-color:var(--color-primary);box-shadow:0 0 0 4px var(--color-primary-light),var(--shadow-sm);background:var(--color-bg-primary)}"
"input:disabled,select:disabled,textarea:disabled{background:var(--color-bg-tertiary);color:var(--color-text-disabled);cursor:not-allowed;opacity:0.6}"
"input::placeholder,textarea::placeholder{color:var(--color-text-tertiary)}"
"/* ===== BUTTONS ===== */"
"button,.btn{display:inline-flex;align-items:center;justify-content:center;gap:var(--space-sm);padding:var(--space-md) var(--space-xl);border:none;border-radius:var(--radius-md);font-family:Orbitron,monospace;font-size:var(--text-sm);font-weight:var(--weight-semibold);text-transform:uppercase;letter-spacing:0.05em;cursor:pointer;transition:transform var(--transition-fast),box-shadow var(--transition-fast);position:relative;overflow:hidden;box-shadow:var(--shadow-sm);will-change:transform;transform:translateZ(0)}"
".btn-primary,button{background:linear-gradient(135deg,var(--color-primary),var(--color-accent));color:#fff}"
".btn-primary:hover,button:hover{transform:translateY(-2px);box-shadow:0 8px 16px rgba(37,99,235,0.3)}"
".btn-primary:active,button:active{transform:translateY(0) scale(0.98)}"
".btn-secondary{background:var(--color-bg-primary);color:var(--color-primary);border:2px solid var(--color-primary)}"
".btn-secondary:hover{background:var(--color-primary);color:#fff;border-color:var(--color-primary);box-shadow:0 8px 24px var(--glow-primary)}"
".btn-success{background:linear-gradient(135deg,var(--color-success),var(--success-400));color:#fff}"
".btn-success:hover{box-shadow:0 12px 32px var(--glow-success)}"
".btn-danger{background:linear-gradient(135deg,var(--error-600),var(--color-error));color:#fff}"
".btn-ghost{background:transparent;color:var(--color-primary);border:1px solid transparent}"
".btn-ghost:hover{background:var(--color-primary-light);border-color:var(--color-primary)}"
".btn-small{padding:var(--space-sm) var(--space-md);font-size:var(--text-xs)}"
".btn-large{padding:var(--space-lg) var(--space-2xl);font-size:var(--text-base)}"
"button:disabled,.btn:disabled{opacity:0.5;cursor:not-allowed;transform:none!important}"
"/* ===== TABLES ===== */"
"table{width:100%;border-collapse:separate;border-spacing:0;background:var(--color-bg-primary);border-radius:var(--radius-lg);overflow:hidden;box-shadow:var(--shadow-md);margin:var(--space-lg) 0}"
"thead{background:linear-gradient(135deg,var(--color-primary),var(--color-accent))}"
"th{padding:var(--space-md) var(--space-lg);text-align:left;font-family:Orbitron,monospace;font-weight:var(--weight-bold);font-size:var(--text-sm);color:#fff;text-transform:uppercase;letter-spacing:0.05em;border-bottom:2px solid rgba(255,255,255,0.2)}"
"td{padding:var(--space-md) var(--space-lg);color:var(--color-text-secondary);border-bottom:1px solid var(--color-border-light);font-size:var(--text-sm)}"
"tbody tr{transition:background-color var(--transition-fast)}"
"tbody tr:hover{background:var(--color-bg-secondary)}"
"tbody tr:last-child td{border-bottom:none}"
"/* ===== STATUS BADGES ===== */"
".badge{display:inline-flex;align-items:center;gap:var(--space-xs);padding:var(--space-xs) var(--space-md);border-radius:var(--radius-full);font-size:var(--text-xs);font-weight:var(--weight-semibold);text-transform:uppercase;letter-spacing:0.05em}"
".badge-success,.status-good{background:rgba(16,185,129,0.1);color:var(--color-success);border:1px solid var(--color-success)}"
".badge-warning,.status-warning{background:rgba(245,158,11,0.1);color:var(--warning-600);border:1px solid var(--color-warning)}"
".badge-error,.status-error{background:rgba(239,68,68,0.1);color:var(--error-600);border:1px solid var(--color-error)}"
".badge-info{background:rgba(59,130,246,0.1);color:var(--color-primary);border:1px solid var(--color-primary)}"
".badge::before{content:'';width:6px;height:6px;border-radius:50%}"
".badge-success::before{background:var(--color-success)}"
".badge-warning::before{background:var(--color-warning)}"
".badge-error::before{background:var(--color-error)}"
".badge-info::before{background:var(--color-primary)}"
"/* ===== ALERT/RESULT BOXES ===== */"
".test-result,.alert{padding:var(--space-lg);margin:var(--space-lg) 0;border-radius:var(--radius-lg);border:1px solid;box-shadow:var(--shadow-md);animation:slideIn 0.3s;background:white;overflow:hidden;max-width:100%;box-sizing:border-box}"
".test-result{border-color:var(--color-success);background:linear-gradient(135deg,rgba(16,185,129,0.05),rgba(16,185,129,0.02))}"
".test-result h4{color:var(--color-success);margin:0 0 var(--space-md) 0;font-family:Orbitron,monospace;font-size:var(--text-lg);display:flex;align-items:center;gap:var(--space-sm)}"
".alert-success{background:rgba(16,185,129,0.05);border-color:var(--color-success);color:var(--success-600)}"
".alert-warning{background:rgba(245,158,11,0.05);border-color:var(--color-warning);color:var(--warning-600)}"
".alert-error{background:rgba(239,68,68,0.05);border-color:var(--color-error);color:var(--error-600)}"
"/* ===== SCADACORE FORMAT TABLE ===== */"
".scada-table{width:100%;border-collapse:collapse;margin-top:var(--space-md);font-size:var(--text-sm);box-shadow:var(--shadow-sm);border-radius:var(--radius-md);overflow:hidden;table-layout:fixed}"
".scada-table th{padding:var(--space-md);font-weight:var(--weight-bold);text-align:center;color:white;font-family:Orbitron,monospace;word-wrap:break-word}"
".scada-table td{padding:var(--space-sm) var(--space-md);border-bottom:1px solid var(--color-border-light);text-align:left;word-wrap:break-word;overflow-wrap:break-word;white-space:normal}"
".scada-table td:first-child{width:50%;font-weight:var(--weight-semibold)}"
".scada-table td:last-child{width:50%;text-align:right;font-family:monospace}"
".scada-table tr:last-child td{border-bottom:none}"
".scada-table tr:nth-child(even){background:var(--color-bg-secondary)}"
".scada-table strong{color:var(--color-primary);font-weight:var(--weight-semibold)}"
".scada-header-main{background:linear-gradient(135deg,var(--gray-700),var(--gray-900))}"
".scada-header-float{background:linear-gradient(135deg,var(--primary-600),var(--primary-700))}"
".scada-header-int{background:linear-gradient(135deg,var(--success-600),var(--success-700))}"
".scada-header-uint{background:linear-gradient(135deg,var(--warning-600),var(--warning-700))}"
".scada-header-float64{background:linear-gradient(135deg,#6610f2,#520dc2)}"
".scada-header-int64{background:linear-gradient(135deg,#20c997,#17a579)}"
".scada-header-uint64{background:linear-gradient(135deg,#dc3545,#b02a37)}"
".value-box{background:var(--color-bg-tertiary);padding:var(--space-sm);border-radius:var(--radius-sm);font-family:monospace;margin:var(--space-xs) 0}"
".hex-display{font-family:monospace;color:var(--color-accent);font-weight:var(--weight-semibold);letter-spacing:1px}"
".scada-breakdown{background:linear-gradient(135deg,rgba(59,130,246,0.08),rgba(59,130,246,0.03));padding:var(--space-md);border-radius:var(--radius-md);margin:var(--space-md) 0;border-left:4px solid var(--color-primary)}"
".alert-info{background:rgba(59,130,246,0.05);border-color:var(--color-primary);color:var(--primary-700)}"
"@keyframes slideIn{from{opacity:0;transform:translateX(-20px)}to{opacity:1;transform:translateX(0)}}"
"/* ===== HEADER ===== */"
".header{display:flex;align-items:center;gap:var(--space-lg);padding:var(--space-xl);background:rgba(255,255,255,0.95);border:1px solid rgba(255,255,255,0.3);border-radius:var(--radius-xl);box-shadow:var(--shadow-lg);margin-bottom:var(--space-xl);position:relative;overflow:hidden}"
".header::before{content:'';position:absolute;top:0;left:0;right:0;height:4px;background:linear-gradient(90deg,var(--color-primary) 0%,var(--color-accent) 50%,var(--color-success) 100%);box-shadow:0 2px 8px var(--glow-primary)}"
".logo{width:auto;max-width:100%;height:45px;object-fit:contain;display:block}"
"/* ===== CARDS ===== */"
".card,.sensor-card{background:rgba(255,255,255,0.95);border:1px solid rgba(255,255,255,0.4);border-radius:var(--radius-xl);padding:var(--space-xl);margin:var(--space-lg) 0;box-shadow:var(--shadow-lg);position:relative;transition:transform var(--transition-base),box-shadow var(--transition-base);overflow:visible;min-height:100px;display:flex;flex-direction:column;justify-content:center;will-change:transform;transform:translateZ(0);width:100%;box-sizing:border-box}"
".card::before,.sensor-card::before{content:'';position:absolute;top:0;left:0;right:0;height:4px;background:linear-gradient(90deg,var(--color-primary) 0%,var(--color-accent) 50%,var(--color-success) 100%);opacity:0.9}"
".card:hover,.sensor-card:hover{transform:translateY(-4px);box-shadow:0 12px 40px rgba(0,0,0,0.2);border-color:rgba(255,255,255,0.7)}"
".card h3,.sensor-card h3{margin:0 0 var(--space-md);font-size:var(--text-xl);font-weight:var(--weight-bold);color:var(--color-primary);font-family:Orbitron,monospace}"
".card p strong,.sensor-card p strong{display:inline;color:var(--color-text-primary);font-weight:var(--weight-semibold)}"
".card p:has(strong),.sensor-card p:has(strong){color:var(--color-text-secondary);line-height:var(--leading-relaxed);margin-bottom:var(--space-sm);display:grid;grid-template-columns:minmax(140px,auto) 1fr;gap:var(--space-md);align-items:baseline}"
".card p:has(strong) strong,.sensor-card p:has(strong) strong{text-align:left}"
".card p:has(strong) span,.sensor-card p:has(strong) span{text-align:right;color:var(--color-text-secondary);word-break:break-word}"
".card p:not(:has(strong)),.sensor-card p:not(:has(strong)){color:var(--color-text-secondary);line-height:1.4;margin-bottom:var(--space-sm);display:block;word-wrap:break-word;overflow-wrap:break-word;letter-spacing:normal}"
".card-header{display:flex;align-items:center;justify-content:space-between;margin-bottom:var(--space-lg);padding-bottom:var(--space-md);border-bottom:2px solid var(--color-border-light)}"
".card-footer{margin-top:var(--space-lg);padding-top:var(--space-md);border-top:1px solid var(--color-border-light);display:flex;gap:var(--space-md);flex-wrap:wrap}"
".card-metric{display:flex;flex-direction:column;gap:var(--space-xs);align-items:center;text-align:center}"
".card-metric-label{font-size:var(--text-xs);color:var(--color-text-tertiary);text-transform:uppercase;letter-spacing:0.05em;font-weight:var(--weight-semibold);text-align:center}"
".card-metric-value{font-size:var(--text-2xl);font-weight:var(--weight-bold);color:var(--color-primary);font-family:Orbitron,monospace;text-align:center;line-height:1.2}"
".status-item{display:flex;flex-direction:column;align-items:center;text-align:center;padding:var(--space-md);gap:var(--space-xs)}"
".status-item strong{display:block;font-size:var(--text-sm);color:var(--color-text-primary);margin-bottom:var(--space-xs);font-weight:var(--weight-semibold)}"
".status-item span{font-size:var(--text-base);color:var(--color-text-secondary)}"
"/* ===== FORM LAYOUTS ===== */"
".form-grid{display:grid;grid-template-columns:180px 1fr;gap:var(--space-md);align-items:center;margin:var(--space-md) 0;width:100%}"
".form-grid label{font-weight:var(--weight-semibold);color:var(--color-text-primary);text-align:left}"
".form-grid input,.form-grid select,.form-grid textarea{width:100%;max-width:100%;padding:var(--space-sm);border:1px solid var(--color-border-medium);border-radius:var(--radius-sm);font-size:var(--text-base)}"
".form-grid textarea{font-family:monospace;resize:vertical}"
"/* ===== HEAP USAGE BAR ===== */"
".heap-bar{width:100%;height:24px;background:var(--color-bg-tertiary);border-radius:var(--radius-sm);position:relative;overflow:hidden;margin-top:var(--space-xs)}"
".heap-bar-fill{height:100%;background:var(--color-success);transition:width 0.3s ease,background-color 0.3s ease;display:flex;align-items:center;justify-content:flex-end;padding-right:var(--space-sm);color:white;font-weight:var(--weight-semibold);font-size:var(--text-sm)}"
".heap-bar-fill.warning{background:var(--color-warning)}"
".heap-bar-fill.critical{background:var(--color-error)}"
"/* ===== SECTION SPACING ===== */"
".section-title{padding:var(--space-lg) 0;margin-bottom:var(--space-xl);border-bottom:3px solid var(--color-primary);display:flex;align-items:center;gap:var(--space-md)}"
".sensor-card{margin-bottom:var(--space-xl)}"
".sensor-card h3{margin-top:0;margin-bottom:var(--space-md);padding-bottom:var(--space-sm);border-bottom:2px solid var(--color-border-light)}"
".sensor-card p:last-child{margin-bottom:0}"
".sensor-card>p{word-wrap:break-word;overflow-wrap:break-word;hyphens:auto;max-width:100%}"
"/* ===== LAYOUT ===== */"
".container{display:flex;min-height:100vh;position:relative;z-index:1}"
"/* ===== SIDEBAR ===== */"
".sidebar{width:320px;background:rgba(255,255,255,0.95);padding:0;position:fixed;height:100vh;overflow-y:auto;overflow-x:hidden;border-right:1px solid rgba(255,255,255,0.3);box-shadow:8px 0 40px rgba(0,0,0,0.12);z-index:var(--z-sticky);scrollbar-width:thin;scrollbar-color:var(--color-border-medium) transparent;-webkit-overflow-scrolling:touch}"
".sidebar::-webkit-scrollbar{width:6px}"
".sidebar::-webkit-scrollbar-track{background:transparent}"
".sidebar::-webkit-scrollbar-thumb{background:var(--color-border-medium);border-radius:var(--radius-full)}"
".sidebar::-webkit-scrollbar-thumb:hover{background:var(--color-border-dark)}"
".main-content{margin-left:320px;padding:var(--space-2xl);flex:1;max-width:calc(100% - 320px)}"
"/* ===== NAVIGATION MENU ===== */"
".menu-item{display:flex;align-items:center;gap:var(--space-md);width:100%;padding:var(--space-xl) var(--space-xl);color:var(--color-text-primary);text-decoration:none;border:none;background:transparent;cursor:pointer;text-align:left;font-size:var(--text-base);font-family:Orbitron,monospace;font-weight:var(--weight-semibold);text-transform:uppercase;letter-spacing:0.05em;border-bottom:1px solid rgba(255,255,255,0.1);transition:background-color var(--transition-fast),color var(--transition-fast);position:relative;overflow:hidden}"
".menu-item::before{content:'';position:absolute;left:0;top:0;bottom:0;width:0;background:linear-gradient(180deg,var(--color-primary),var(--color-accent));transition:width var(--transition-fast)}"
".menu-item:hover{background:var(--color-primary-light);color:var(--color-primary)}"
".menu-item:hover::before{width:4px}"
".menu-item.active{background:linear-gradient(135deg,var(--color-primary),var(--color-accent));color:#fff;box-shadow:0 4px 16px var(--glow-primary),inset 0 1px 0 rgba(255,255,255,0.2);font-weight:var(--weight-bold)}"
".menu-item.active::before{width:4px;background:var(--color-success)}"
".menu-item.active .menu-icon{transform:scale(1.1);filter:drop-shadow(0 0 8px rgba(255,255,255,0.5))}"
".menu-icon{width:28px;height:28px;min-width:28px;display:inline-flex;align-items:center;justify-content:center;font-size:var(--text-xl);transition:all var(--transition-base);flex-shrink:0}"
"i.menu-icon:before{display:inline-block;width:100%;text-align:center}"
"/* ===== SECTIONS ===== */"
".section{display:none;background:rgba(255,255,255,0.95);border:1px solid rgba(255,255,255,0.4);border-radius:var(--radius-2xl);padding:var(--space-2xl);margin-bottom:var(--space-2xl);box-shadow:var(--shadow-lg);position:relative;overflow:hidden;animation:fadeInUp 0.3s ease-out;width:100%;box-sizing:border-box}"
".section.active{display:block}"
".section::before{content:'';position:absolute;top:0;left:0;right:0;height:4px;background:linear-gradient(90deg,var(--color-primary) 0%,var(--color-accent) 50%,var(--color-success) 100%);box-shadow:0 2px 8px var(--glow-primary)}"
"@keyframes fadeInUp{from{opacity:0;transform:translateY(30px)}to{opacity:1;transform:translateY(0)}}"
".section-title{font-size:var(--text-3xl);font-weight:var(--weight-extrabold);color:var(--color-primary);font-family:Orbitron,monospace;margin-bottom:var(--space-2xl);display:flex;align-items:center;gap:var(--space-md);padding-bottom:var(--space-lg);border-bottom:3px solid var(--color-border-light);position:relative}"
".section-title::after{content:'';position:absolute;bottom:-3px;left:0;width:80px;height:3px;background:linear-gradient(90deg,var(--color-primary),var(--color-accent));border-radius:var(--radius-full)}"
".section-title i{width:48px;height:48px;display:flex;align-items:center;justify-content:center;background:linear-gradient(135deg,var(--color-primary),var(--color-accent));border-radius:var(--radius-md);color:#fff;font-size:var(--text-xl);box-shadow:var(--shadow-md)}"
"/* ===== SIDEBAR HEADER ===== */"
".sidebar .header{background:rgba(255,255,255,0.98);border-bottom:1px solid rgba(255,255,255,0.4);padding:var(--space-lg) var(--space-md);margin:0;border-radius:0;box-shadow:0 2px 10px rgba(0,0,0,0.05);position:relative;overflow:visible;display:flex;justify-content:center;align-items:center}"
".sidebar .header::before{content:'';position:absolute;top:0;left:0;right:0;height:3px;background:linear-gradient(90deg,#0066cc 0%,#00aaff 100%);box-shadow:0 1px 4px rgba(0,102,204,0.3)}"
".sidebar .logo{height:40px;width:auto}"
"/* ===== STATUS INDICATORS ===== */"
"#heap_usage{color:var(--color-warning);font-weight:var(--weight-bold)}#wifi_status{color:var(--color-success);font-weight:var(--weight-bold)}#uptime{color:var(--color-accent);font-weight:var(--weight-bold)}"
"#networks{scrollbar-width:thin;scrollbar-color:#c1c1c1 #f1f1f1}#networks::-webkit-scrollbar{width:8px}#networks::-webkit-scrollbar-track{background:#f1f1f1;border-radius:4px}#networks::-webkit-scrollbar-thumb{background:#c1c1c1;border-radius:4px}#networks::-webkit-scrollbar-thumb:hover{background:#a8a8a8}"
".wifi-grid{display:grid;grid-template-columns:120px 1fr;gap:15px;align-items:center;margin:20px 0}"
".wifi-input-group{position:relative;display:flex;align-items:center;width:100%}"
".wifi-grid label{text-align:left;font-weight:var(--weight-semibold);color:var(--color-text-primary)}"
".wifi-grid input,.wifi-grid select{width:100%;max-width:100%}"
".scan-button{background:linear-gradient(135deg,var(--color-accent),var(--color-success));transition:all .3s;box-shadow:0 2px 8px rgba(56,178,172,.3);border:none;color:white;font-family:Orbitron,monospace;font-weight:600;padding:var(--space-md) var(--space-lg)}.scan-button:hover{transform:translateY(-1px);box-shadow:0 4px 12px rgba(56,178,172,.4)}"
".network-item{padding:var(--space-md);border-radius:var(--radius-md);border:1px solid var(--color-border-light);margin-bottom:var(--space-sm);transition:all var(--transition-base);background:var(--color-bg-primary)}"
".network-item:hover{transform:translateY(-2px);box-shadow:var(--shadow-sm);border-color:var(--color-primary);background:var(--color-primary-light)}"
"/* ===== UTILITY CLASSES ===== */"
".grid{display:grid;gap:var(--space-lg)}"
".grid-2{grid-template-columns:repeat(auto-fit,minmax(300px,1fr))}"
".grid-3{grid-template-columns:repeat(auto-fit,minmax(250px,1fr))}"
".grid-4{grid-template-columns:repeat(auto-fit,minmax(200px,1fr))}"
".flex{display:flex;gap:var(--space-md)}"
".flex-center{display:flex;align-items:center;justify-content:center}"
".flex-between{display:flex;align-items:center;justify-content:space-between}"
".flex-col{flex-direction:column}"
".gap-sm{gap:var(--space-sm)}.gap-md{gap:var(--space-md)}.gap-lg{gap:var(--space-lg)}"
".text-center{text-align:center}.text-right{text-align:right}"
".mt-0{margin-top:0}.mt-sm{margin-top:var(--space-sm)}.mt-md{margin-top:var(--space-md)}.mt-lg{margin-top:var(--space-lg)}"
".mb-0{margin-bottom:0}.mb-sm{margin-bottom:var(--space-sm)}.mb-md{margin-bottom:var(--space-md)}.mb-lg{margin-bottom:var(--space-lg)}"
".p-sm{padding:var(--space-sm)}.p-md{padding:var(--space-md)}.p-lg{padding:var(--space-lg)}"
".hidden{display:none}.visible{display:block}"
"/* ===== RESPONSIVE DESIGN (Mobile-First) ===== */"
"@media (max-width:360px){"
"*{box-sizing:border-box}"
":root{--space-xs:3px;--space-sm:6px;--space-md:10px;--space-lg:14px;--space-xl:18px;--space-2xl:22px;--space-3xl:26px}"
"body,html{overflow-x:hidden!important;width:100%!important;max-width:100vw!important}"
".container{display:block;width:100%!important;max-width:100vw!important;overflow-x:hidden!important}"
".sidebar{width:100%!important;padding:var(--space-sm)!important}"
".main-content{margin-left:0!important;padding:var(--space-sm)!important;width:100%!important;max-width:100vw!important}"
".menu-item{padding:var(--space-xs) var(--space-sm);font-size:10px;margin:1px}"
".menu-item span{display:none!important}"
".menu-icon{width:18px;height:18px;font-size:14px}"
".card,.sensor-card,.section{padding:var(--space-sm);margin:var(--space-xs) 0}"
".card p:has(strong),.sensor-card p:has(strong){grid-template-columns:1fr;gap:var(--space-xs);text-align:left}"
".card p:has(strong) strong,.sensor-card p:has(strong) strong{font-size:11px;margin-bottom:2px;display:block}"
".card p:has(strong) span,.sensor-card p:has(strong) span{text-align:left;font-size:13px;padding-left:var(--space-sm);display:block}"
".card p:not(:has(strong)),.sensor-card p:not(:has(strong)){font-size:12px;line-height:1.3;word-break:break-word;margin-bottom:6px;letter-spacing:0}"
".form-grid{display:block;width:100%}"
".form-grid label{display:block;margin-bottom:4px;font-size:12px}"
".form-grid input,.form-grid select,.form-grid textarea{width:100%!important;max-width:100%!important;margin-bottom:var(--space-md)}"
".heap-bar{height:20px}"
".heap-bar-fill{font-size:11px;padding-right:var(--space-xs)}"
".section-title{font-size:16px;padding:var(--space-sm) 0;margin-bottom:var(--space-md)}"
".scada-table{font-size:10px;overflow-x:auto;display:block}"
".scada-table th{padding:var(--space-xs) var(--space-sm);font-size:9px}"
".scada-table td{padding:4px var(--space-xs);font-size:10px}"
".scada-breakdown{padding:var(--space-sm);font-size:11px}"
".value-box{padding:4px;font-size:11px}"
"h1{font-size:20px}h2{font-size:16px}h3{font-size:14px}"
"p{font-size:12px}"
"input,select,textarea,button,.btn{font-size:14px;padding:var(--space-sm)}"
".logo{height:28px}"
".header{padding:var(--space-sm)}"
"}"
"@media (max-width:480px){"
"*{box-sizing:border-box}"
":root{--space-xs:4px;--space-sm:8px;--space-md:12px;--space-lg:16px;--space-xl:20px;--space-2xl:24px;--space-3xl:28px}"
"body{overflow-x:hidden;width:100%;max-width:100vw}"
".container{display:block;width:100%;max-width:100vw;overflow-x:hidden}"
".sidebar{width:100%!important;height:auto;position:static;overflow:visible;border-right:none;border-bottom:1px solid rgba(255,255,255,0.3);box-shadow:none}"
".main-content{margin-left:0!important;padding:var(--space-md);width:100%;max-width:100vw;box-sizing:border-box}"
".menu{display:flex;flex-wrap:wrap;justify-content:space-around;padding:var(--space-sm) 0}"
".menu-item{flex:0 0 auto;padding:var(--space-sm) var(--space-md);font-size:11px;white-space:nowrap;margin:2px;border-radius:var(--radius-sm)}"
".menu-item span{display:none}"
".menu-icon{width:20px;height:20px;min-width:20px;font-size:16px;margin:0}"
".section{padding:var(--space-md);margin-bottom:var(--space-md);border-radius:var(--radius-md);width:100%;box-sizing:border-box}"
".section-title{font-size:18px;margin-bottom:var(--space-md);padding-bottom:var(--space-sm);word-wrap:break-word}"
".section-title i{width:28px;height:28px;font-size:16px}"
".card,.sensor-card{padding:var(--space-md);margin:var(--space-sm) 0;border-radius:var(--radius-md);width:100%;box-sizing:border-box;min-height:auto}"
".card h3,.sensor-card h3{font-size:16px;margin-bottom:var(--space-sm);color:var(--primary-700);font-weight:var(--weight-bold);word-wrap:break-word}"
".card p:has(strong),.sensor-card p:has(strong){font-size:13px;line-height:1.5;color:var(--color-text-primary);word-wrap:break-word;grid-template-columns:110px 1fr;gap:var(--space-sm)}"
".card p:has(strong) strong,.sensor-card p:has(strong) strong{font-size:12px;text-align:left;font-weight:var(--weight-semibold)}"
".card p:has(strong) span,.sensor-card p:has(strong) span{font-size:13px;text-align:right;word-break:break-word}"
".card p:not(:has(strong)),.sensor-card p:not(:has(strong)){font-size:13px;line-height:1.35;word-break:break-word;color:var(--color-text-secondary);margin-bottom:8px;letter-spacing:0}"
".card strong,.sensor-card strong{display:block;color:var(--primary-800);font-size:13px;margin:var(--space-xs) 0}"
".card-header{justify-content:flex-start;flex-wrap:wrap}"
".card *,.sensor-card *{max-width:100%}"
".card label,.sensor-card label{display:block;text-align:left;color:var(--primary-800);font-weight:var(--weight-semibold);margin-bottom:var(--space-xs);font-size:13px}"
"input,select,textarea{width:100%!important;max-width:100%!important;padding:var(--space-sm);font-size:16px;border-radius:var(--radius-sm);box-sizing:border-box}"
"button,.btn{padding:var(--space-sm) var(--space-md);width:100%!important;max-width:100%!important;font-size:13px;min-height:44px;white-space:normal;word-wrap:break-word;box-sizing:border-box}"
".input-large,.input-medium,.input-small,.input-tiny{width:100%!important;max-width:100%!important}"
".required-field,.optional-field{width:100%;max-width:100%;box-sizing:border-box;padding:8px}"
".sensor-form-grid{display:block;width:100%}"
".sensor-form-grid>*{width:100%;margin-bottom:var(--space-sm)}"
".sensor-actions{display:flex;flex-direction:column;gap:8px;margin-top:10px;width:100%}"
".sensor-actions button{width:100%!important;min-height:44px;padding:12px;margin:4px 0}"
"table{font-size:11px;width:100%;display:block;overflow-x:auto;-webkit-overflow-scrolling:touch}"
"thead,tbody,tr,th,td{display:block}"
"th,td{padding:var(--space-xs) var(--space-sm);font-size:11px;text-align:left}"
"tr{border-bottom:1px solid var(--color-border-light);padding:var(--space-xs) 0}"
".header{flex-direction:column;align-items:center;justify-content:center;padding:var(--space-md);gap:var(--space-sm);width:100%;box-sizing:border-box}"
".logo{height:32px;max-width:85%;width:auto;object-fit:contain}"
".badge{font-size:10px;padding:3px var(--space-xs);white-space:nowrap}"
".badge::before{width:4px;height:4px}"
".card-metric{gap:6px;padding:var(--space-sm);flex-direction:column;align-items:flex-start}"
".card-metric-label{font-size:11px;line-height:1.3}"
".card-metric-value{font-size:20px;line-height:1.2}"
".status-item{padding:var(--space-sm);gap:6px;flex-direction:column;align-items:flex-start}"
".status-item strong{font-size:11px;margin-bottom:4px}"
".status-item span{font-size:13px;word-wrap:break-word}"
".grid,.grid-2,.grid-3,.grid-4{display:block;width:100%}"
".grid>*,.grid-2>*,.grid-3>*,.grid-4>*{width:100%;margin-bottom:var(--space-md)}"
".flex,.flex-between{flex-direction:column;align-items:stretch;gap:var(--space-sm);width:100%}"
"h1{font-size:22px;line-height:1.3;word-wrap:break-word}h2{font-size:18px;line-height:1.3;word-wrap:break-word}h3{font-size:16px;line-height:1.4;word-wrap:break-word}"
"p{font-size:13px;line-height:1.6;word-wrap:break-word}"
".test-result,.alert{padding:var(--space-md);margin:var(--space-sm) 0;width:100%;box-sizing:border-box;word-wrap:break-word}"
".sidebar .header{padding:var(--space-md);justify-content:center}"
".sidebar .logo{height:32px;max-width:80%}"
".wifi-grid{display:block;width:100%}"
".wifi-grid>*{width:100%;margin-bottom:var(--space-sm)}"
".wifi-grid label{font-size:13px;margin-bottom:6px;display:block}"
".wifi-grid input,.wifi-grid select{width:100%!important}"
".form-grid{display:block;width:100%}"
".form-grid label{display:block;margin-bottom:6px;font-size:13px}"
".form-grid input,.form-grid select,.form-grid textarea{width:100%!important;max-width:100%!important;margin-bottom:var(--space-md)}"
".heap-bar{height:22px}"
".heap-bar-fill{font-size:12px}"
".section-title{font-size:18px;padding:var(--space-md) 0}"
".scada-table{font-size:9px;overflow-x:auto;display:table;width:100%;-webkit-overflow-scrolling:touch;table-layout:fixed}"
".scada-table th{padding:3px 4px;font-size:8px;white-space:normal;word-wrap:break-word}"
".scada-table td{padding:2px 4px;font-size:9px;word-break:break-all;border-bottom:1px solid var(--color-border-light)}"
".scada-table strong{font-size:8px}"
".scada-breakdown{padding:var(--space-sm);font-size:10px}"
".test-result{padding:var(--space-sm);margin:var(--space-sm) 0}"
".test-result h4{font-size:var(--text-sm)}"
"div[style*='grid-template-columns']{display:block!important}"
"div[style*='grid-template-columns']>*{width:100%!important;margin-bottom:var(--space-sm)}"
"}"
"@media (min-width:481px) and (max-width:768px){"
"*{box-sizing:border-box}"
"body{overflow-x:hidden;width:100%;max-width:100vw}"
".container{display:block;width:100%;max-width:100vw}"
".sidebar{width:100%!important;height:auto;position:static;overflow:visible;border-right:none;border-bottom:1px solid rgba(255,255,255,0.3);box-shadow:none}"
".main-content{margin-left:0!important;padding:var(--space-lg);width:100%;max-width:100vw;box-sizing:border-box}"
".menu{display:flex;flex-wrap:wrap;gap:var(--space-sm)}"
".menu-item{flex:1 1 auto;padding:var(--space-md) var(--space-lg);font-size:14px;min-width:120px;text-align:center}"
".section{padding:var(--space-lg);margin-bottom:var(--space-lg);width:100%;box-sizing:border-box}"
".section-title{font-size:24px}"
".card,.sensor-card{padding:var(--space-lg);margin:var(--space-md) 0;width:100%;box-sizing:border-box}"
".card p:has(strong),.sensor-card p:has(strong){grid-template-columns:160px 1fr;gap:var(--space-md)}"
".card p:has(strong) strong,.sensor-card p:has(strong) strong{font-size:14px}"
".card p:has(strong) span,.sensor-card p:has(strong) span{font-size:14px}"
".card p:not(:has(strong)),.sensor-card p:not(:has(strong)){font-size:14px;line-height:1.4;word-break:break-word;margin-bottom:10px;letter-spacing:0}"
"input,select,textarea{width:100%!important;max-width:100%!important;font-size:16px;box-sizing:border-box}"
"button,.btn{padding:var(--space-md) var(--space-lg);width:100%;font-size:14px;min-height:48px;box-sizing:border-box}"
"table{font-size:14px;overflow-x:auto;display:block;width:100%}"
".header{flex-direction:row;text-align:center;padding:var(--space-lg);justify-content:center;width:100%;box-sizing:border-box}"
".logo{height:38px;max-width:200px}"
".company-info h1{font-size:20px}"
".company-info p{font-size:12px}"
".grid-2,.grid-3,.grid-4{grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:var(--space-md)}"
".flex-between{flex-direction:column;align-items:stretch;gap:var(--space-md)}"
".wifi-grid{display:grid;grid-template-columns:150px 1fr;gap:var(--space-md)}"
"h1{font-size:28px}h2{font-size:24px}h3{font-size:20px}"
"div[style*='grid-template-columns']{display:grid!important;grid-template-columns:repeat(auto-fit,minmax(200px,1fr))!important;gap:var(--space-md)}"
"}"
"@media (min-width:769px) and (max-width:1024px){"
".sidebar{width:280px}"
".main-content{margin-left:280px;padding:var(--space-xl)}"
".grid-4{grid-template-columns:repeat(2,1fr)}"
"}"
"@media (min-width:769px){"
".sensor-form-grid{grid-template-columns:160px 1fr;gap:12px}"
".sensor-actions{flex-direction:row;flex-wrap:wrap}"
".sensor-actions button{flex:1 1 auto;min-width:90px;width:auto;margin:2px}"
"}"
"@media (min-width:1025px){"
".sidebar{width:320px}"
".main-content{margin-left:320px;padding:var(--space-2xl)}"
"}"
"@media (min-width:1440px){"
".main-content{margin-left:320px;padding:var(--space-2xl)}"
"}"
"/* ===== MINIMAL MOBILE FIXES - ONLY FOR SPECIFIC ELEMENTS ===== */"
"@media screen and (max-width:768px){"
"/* Fix button sizes on mobile - make them smaller */"
"button[style*='padding:12px 30px'],"
"button[style*='padding:12px 28px'],"
"button[style*='padding:12px 25px'],"
"button[style*='padding:14px 35px']{"
"padding:8px 16px!important;"
"font-size:14px!important"
"}"
"/* Fix save buttons specifically */"
"button[type='submit']{"
"padding:8px 20px!important;"
"font-size:14px!important"
"}"
"/* Fix checkbox sizes - make them normal */"
"input[type='checkbox']{"
"width:16px!important;"
"height:16px!important;"
"margin-right:6px!important"
"}"
"/* Fix menu/navigation buttons - make them bigger */"
".menu-item,.nav-link{"
"padding:10px 15px!important;"
"font-size:14px!important"
"}"
".menu-item button{"
"padding:10px 15px!important;"
"font-size:14px!important"
"}"
"/* Fix Normal to Operation mode button specifically */"
"button[onclick*='switchToOperation'],"
"button[onclick*='rebootSystem']{"
"padding:8px 20px!important;"
"font-size:14px!important"
"}"
"/* Ensure minimum touch target but not too big */"
"button{"
"min-height:36px!important;"
"max-height:44px!important"
"}"
"/* Fix sidebar buttons if needed */"
".sidebar button{"
"padding:8px 12px!important;"
"font-size:13px!important"
"}"
"}"
"</style>"
"<script>"
"function showSection(sectionId){"
"var sections=document.querySelectorAll('.section');"
"var menuItems=document.querySelectorAll('.menu-item');"
"sections.forEach(function(s){s.classList.remove('active');});"
"menuItems.forEach(function(m){m.classList.remove('active');});"
"stopTelemetryAutoRefresh();"
"document.getElementById(sectionId).classList.add('active');"
"var activeBtn=document.querySelector('[onclick*=\"'+sectionId+'\"]'); if(activeBtn) activeBtn.classList.add('active');"
"if(sectionId==='telemetry'){startTelemetryAutoRefresh();}"
"}"
"function showAzureSection(){"
"const password=prompt('Azure IoT Configuration Access\\n\\nPlease enter the admin password to access Azure IoT Hub settings:');"
"if(password===null) return;"
"if(password==='admin123'){"
"showSection('azure');"
"}else{"
"alert('Access Denied\\n\\nIncorrect password. Azure IoT Hub configuration is protected for security reasons.\\n\\nContact your system administrator if you need access.');"
"}}"
"function updateSystemStatus(){"
"fetch('/api/system_status').then(response=>response.json()).then(data=>{"
"document.getElementById('uptime').textContent=data.system.uptime_formatted;"
"document.getElementById('mac_address').textContent=data.system.mac_address;"
"document.getElementById('flash_total').textContent=(data.system.flash_total/1024/1024).toFixed(1)+' MB';"
"document.getElementById('tasks').textContent=data.tasks.count;"
"document.getElementById('heap').textContent=(data.memory.free_heap/1024).toFixed(1)+' KB';"
"const heapUsage=document.getElementById('heap_usage');"
"heapUsage.textContent=data.memory.heap_usage_percent.toFixed(1)+'%';"
"heapUsage.className=data.memory.heap_usage_percent>80?'status-error':(data.memory.heap_usage_percent>60?'status-warning':'status-good');"
"document.getElementById('internal_heap').textContent=(data.memory.internal_heap/1024).toFixed(1)+' KB';"
"document.getElementById('spiram_heap').textContent=data.memory.spiram_heap>0?(data.memory.spiram_heap/1024).toFixed(1)+' KB':'Not Available';"
"document.getElementById('largest_block').textContent=(data.memory.largest_free_block/1024).toFixed(1)+' KB';"
"document.getElementById('app_partition').textContent=(data.partitions.app_partition_size/1024).toFixed(0)+' KB ('+data.partitions.app_usage_percent.toFixed(1)+'% used)';"
"document.getElementById('nvs_partition').textContent=(data.partitions.nvs_partition_size/1024).toFixed(0)+' KB ('+data.partitions.nvs_usage_percent.toFixed(1)+'% used)';"
"const wifiStatus=document.getElementById('wifi_status');"
"wifiStatus.textContent=data.wifi.status;"
"wifiStatus.className=data.wifi.status==='connected'?'status-good':'status-error';"
"const rssi=document.getElementById('rssi');"
"rssi.textContent=data.wifi.rssi+' dBm';"
"rssi.className=data.wifi.rssi>-50?'status-good':(data.wifi.rssi>-70?'status-warning':'status-error');"
"document.getElementById('ssid').textContent=data.wifi.ssid;"
"}).catch(err=>console.log('Status update failed:',err));"
"fetch('/api/modbus/status').then(r=>r.json()).then(data=>{"
"const ids=['modbus_','ov_modbus_'];"
"ids.forEach(prefix=>{"
"const el=document.getElementById(prefix+'total_reads');if(el)el.textContent=data.total_reads;"
"const el2=document.getElementById(prefix+'success');if(el2)el2.textContent=data.successful_reads;"
"const el3=document.getElementById(prefix+'failed');if(el3)el3.textContent=data.failed_reads;"
"const rate=document.getElementById(prefix+'success_rate');"
"if(rate){rate.textContent=data.success_rate.toFixed(1)+'%';rate.className=data.success_rate>95?'status-good':(data.success_rate>80?'status-warning':'status-error');}"
"const el5=document.getElementById(prefix+'crc_errors');if(el5)el5.textContent=data.crc_errors;"
"const el6=document.getElementById(prefix+'timeout_errors');if(el6)el6.textContent=data.timeout_errors;"
"});"
"}).catch(err=>console.log('Modbus status failed:',err));"
"fetch('/api/azure/status').then(r=>r.json()).then(data=>{"
"const ids=['azure_','ov_azure_'];"
"ids.forEach(prefix=>{"
"const conn=document.getElementById(prefix+'connection');"
"if(conn){conn.textContent=data.connection_state;conn.className=data.connection_state==='connected'?'status-good':'status-error';}"
"const hours=Math.floor(data.connection_uptime/3600);"
"const mins=Math.floor((data.connection_uptime%3600)/60);"
"const up=document.getElementById(prefix+'uptime');if(up)up.textContent=hours+'h '+mins+'m';"
"const msg=document.getElementById(prefix+'messages');if(msg)msg.textContent=data.messages_sent;"
"const lastTel=data.last_telemetry_ago;"
"const lt=document.getElementById(prefix+'last_telemetry');if(lt)lt.textContent=lastTel>0?lastTel+'s ago':'Never';"
"const rc=document.getElementById(prefix+'reconnects');if(rc)rc.textContent=data.reconnect_attempts;"
"const did=document.getElementById(prefix+'device_id');if(did)did.textContent=data.device_id;"
"});"
"}).catch(err=>console.log('Azure status failed:',err));}"

"function performWatchdogAction(action){"
"const btn=event.target;"
"const resultDiv=document.getElementById('watchdog-result');"
"if(action==='reset'){"
"if(!confirm('Are you sure you want to restart the system? This will disconnect all clients.'))return;"
"btn.disabled=true;"
"btn.textContent='🔄 Restarting...';"
"resultDiv.innerHTML='<span style=\"color:#ff9500\">⚠️ System restart initiated. Device will reboot in 3 seconds...</span>';"
"resultDiv.style.display='block';"
"resultDiv.style.backgroundColor='#fff3cd';"
"resultDiv.style.borderColor='#ffeaa7';"
"resultDiv.style.color='#856404';"
"}"
"const formData=new URLSearchParams();formData.append('action',action);"
"fetch('/watchdog_control',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData})"
".then(r=>r.json()).then(data=>{"
"if(data.status==='success'){"
"if(action==='reset'){"
"setTimeout(()=>{window.location.reload();},5000);"
"}else{"
"resultDiv.innerHTML='<span style=\"color:#28a745\">✅ '+data.message+'</span>';"
"resultDiv.style.display='block';"
"resultDiv.style.backgroundColor='#d4edda';"
"resultDiv.style.borderColor='#c3e6cb';"
"resultDiv.style.color='#155724';"
"updateWatchdogStatus();"
"}"
"}else{"
"resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Error: '+data.message+'</span>';"
"resultDiv.style.display='block';"
"resultDiv.style.backgroundColor='#f8d7da';"
"resultDiv.style.borderColor='#f5c6cb';"
"resultDiv.style.color='#721c24';"
"}"
"}).catch(e=>{"
"resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Communication Error: '+e.message+'</span>';"
"resultDiv.style.display='block';"
"resultDiv.style.backgroundColor='#f8d7da';"
"resultDiv.style.borderColor='#f5c6cb';"
"resultDiv.style.color='#721c24';"
"}).finally(()=>{if(action!=='reset'){btn.disabled=false;btn.textContent=btn.textContent.replace('...','');}});"
"}"

"function updateWatchdogStatus(){"
"const formData=new URLSearchParams();formData.append('action','status');"
"fetch('/watchdog_control',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData})"
".then(r=>r.json()).then(data=>{"
"if(data.status==='success'){"
"document.getElementById('wd-status').textContent=data.watchdog_enabled==='true'?'Enabled':'Disabled';"
"document.getElementById('wd-timeout').textContent=data.timeout_seconds+' seconds';"
"document.getElementById('wd-cpu0').textContent=data.cpu0_monitored==='true'?'Active':'Inactive';"
"document.getElementById('wd-cpu1').textContent=data.cpu1_monitored==='true'?'Active':'Inactive';"
"const uptimeMs=data.uptime_ms;"
"const hours=Math.floor(uptimeMs/3600000);"
"const minutes=Math.floor((uptimeMs%3600000)/60000);"
"const seconds=Math.floor((uptimeMs%60000)/1000);"
"document.getElementById('wd-uptime').textContent=hours+'h '+minutes+'m '+seconds+'s';"
"}"
"}).catch(err=>console.log('Watchdog status update failed:',err));"
"}"

"function setGPIO(){"
"const pin=document.getElementById('gpio-pin').value;"
"const value=document.getElementById('gpio-value').value;"
"const resultDiv=document.getElementById('gpio-result');"
"if(!pin||pin<0||pin>39){"
"resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Invalid GPIO pin. Use 0-39.</span>';"
"resultDiv.style.display='block';"
"resultDiv.style.backgroundColor='#f8d7da';"
"return;"
"}"
"const formData=new URLSearchParams();"
"formData.append('action','set');"
"formData.append('pin',pin);"
"formData.append('value',value);"
"fetch('/gpio_trigger',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData})"
".then(r=>r.json()).then(data=>{"
"if(data.status==='success'){"
"resultDiv.innerHTML='<span style=\"color:#28a745\">✅ GPIO '+data.pin+' set to '+(data.value==1?'HIGH':'LOW')+'</span>';"
"resultDiv.style.backgroundColor='#d4edda';"
"}else{"
"resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Error: '+data.message+'</span>';"
"resultDiv.style.backgroundColor='#f8d7da';"
"}"
"resultDiv.style.display='block';"
"}).catch(e=>{"
"resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Communication Error: '+e.message+'</span>';"
"resultDiv.style.display='block';"
"resultDiv.style.backgroundColor='#f8d7da';"
"});"
"}"

"function readGPIO(){"
"const pin=document.getElementById('gpio-read-pin').value;"
"const resultDiv=document.getElementById('gpio-result');"
"if(!pin||pin<0||pin>39){"
"resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Invalid GPIO pin. Use 0-39.</span>';"
"resultDiv.style.display='block';"
"resultDiv.style.backgroundColor='#f8d7da';"
"return;"
"}"
"const formData=new URLSearchParams();"
"formData.append('action','read');"
"formData.append('pin',pin);"
"fetch('/gpio_trigger',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData})"
".then(r=>r.json()).then(data=>{"
"if(data.status==='success'){"
"resultDiv.innerHTML='<span style=\"color:#28a745\">📖 GPIO '+data.pin+' is '+(data.value==1?'HIGH (1)':'LOW (0)')+'</span>';"
"resultDiv.style.backgroundColor='#d4edda';"
"}else{"
"resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Error: '+data.message+'</span>';"
"resultDiv.style.backgroundColor='#f8d7da';"
"}"
"resultDiv.style.display='block';"
"}).catch(e=>{"
"resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Communication Error: '+e.message+'</span>';"
"resultDiv.style.display='block';"
"resultDiv.style.backgroundColor='#f8d7da';"
"});"
"}"

"function quickGPIO(pin,value){"
"const formData=new URLSearchParams();"
"formData.append('action','set');"
"formData.append('pin',pin);"
"formData.append('value',value);"
"const resultDiv=document.getElementById('gpio-result');"
"fetch('/gpio_trigger',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData})"
".then(r=>r.json()).then(data=>{"
"if(data.status==='success'){"
"resultDiv.innerHTML='<span style=\"color:#28a745\">✅ Quick Control: GPIO '+pin+' set to '+(value==1?'HIGH':'LOW')+'</span>';"
"resultDiv.style.backgroundColor='#d4edda';"
"}else{"
"resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Error: '+data.message+'</span>';"
"resultDiv.style.backgroundColor='#f8d7da';"
"}"
"resultDiv.style.display='block';"
"}).catch(e=>{"
"resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Communication Error: '+e.message+'</span>';"
"resultDiv.style.display='block';"
"resultDiv.style.backgroundColor='#f8d7da';"
"});"
"}"

"window.onload=function(){const savedSection=sessionStorage.getItem('showSection');if(savedSection){sessionStorage.removeItem('showSection');if(savedSection==='azure'){showAzureSection();}else{showSection(savedSection);}}else{const hash=window.location.hash.substring(1);if(hash&&hash!==''){if(hash==='azure'){showAzureSection();}else{showSection(hash);}}else{showSection('overview');}}toggleNetworkMode();toggleSDOptions();toggleRTCOptions();updateSystemStatus();setInterval(updateSystemStatus,5000);if(document.getElementById('wd-uptime')){updateWatchdogStatus();setInterval(updateWatchdogStatus,30000);}}"
"</script>"
"</head><body>"
"<div class=waves>"
"<div class=wave></div>"
"<div class=wave></div>"
"</div>"
"<div class='container'>"
"<div class='sidebar'>"
"<div class='header' style='padding:var(--space-2xl) var(--space-lg);flex-direction:column;gap:var(--space-md)'>"
"<img src='/logo' class='logo' style='height:80px;max-width:100%;object-fit:contain' alt='FluxGen - Building a Water Positive Future'>"
"</div>"
"<button class='menu-item' onclick='showSection(\"overview\")'>"
"<i class='menu-icon'>📈</i>OVERVIEW"
"</button>"
"<button class='menu-item' onclick='showSection(\"wifi\")'>"
"<i class='menu-icon'>🌐</i>NETWORK CONFIG"
"</button>"
"<button class='menu-item' onclick='showAzureSection()'>"
"<i class='menu-icon'>☁</i>AZURE IOT HUB"
"</button>"
"<button class='menu-item' onclick='showSection(\"telemetry\")'>"
"<i class='menu-icon'>📊</i>TELEMETRY MONITOR"
"</button>"
"<button class='menu-item' onclick='showSection(\"sensors\")'>"
"<i class='menu-icon'>💾</i>MODBUS SENSORS"
"</button>"
"<button class='menu-item' onclick='showSection(\"write_ops\")'>"
"<i class='menu-icon'>✎</i>WRITE OPERATIONS"
"</button>"
"</div>"
"<div class='main-content'>";

static const char* html_footer = "</div></div></body></html>";

// HTML escape function to handle special characters
static void html_escape(char *dest, const char *src, size_t dest_size) {
    size_t i = 0, j = 0;
    while (src[i] && j < dest_size - 6) {  // Leave space for HTML entities
        switch (src[i]) {
            case '<':
                strcpy(&dest[j], "&lt;");
                j += 4;
                break;
            case '>':
                strcpy(&dest[j], "&gt;");
                j += 4;
                break;
            case '&':
                strcpy(&dest[j], "&amp;");
                j += 5;
                break;
            case '"':
                strcpy(&dest[j], "&quot;");
                j += 6;
                break;
            default:
                dest[j] = src[i];
                j++;
                break;
        }
        i++;
    }
    dest[j] = '\0';
}


// Comprehensive Modbus data format conversion (ScadaCore style)
__attribute__((unused)) static void generate_all_format_interpretations(uint16_t* registers, int quantity, char* json_output, size_t json_size) {
    if (!registers || quantity <= 0) {
        snprintf(json_output, json_size, "\"formats\":{}");
        return;
    }
    
    char formats[2048] = "";
    char temp[256];
    
    strcat(formats, "\"formats\":{");
    
    // Raw hex data
    snprintf(temp, sizeof(temp), "\"hex_string\":\"");
    for (int i = 0; i < quantity; i++) {
        char hex_reg[8];
        snprintf(hex_reg, sizeof(hex_reg), "%02X%02X", (registers[i] >> 8) & 0xFF, registers[i] & 0xFF);
        strcat(temp, hex_reg);
    }
    strcat(temp, "\",");
    strcat(formats, temp);
    
    // UINT16 formats
    if (quantity >= 1) {
        snprintf(temp, sizeof(temp), "\"uint16_be\":%" PRIu16 ",\"uint16_le\":%" PRIu16 ",", 
                 registers[0], (uint16_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)));
        strcat(formats, temp);
        
        snprintf(temp, sizeof(temp), "\"int16_be\":%" PRId16 ",\"int16_le\":%" PRId16 ",", 
                 (int16_t)registers[0], (int16_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)));
        strcat(formats, temp);
    }
    
    // 32-bit formats (if we have 2 registers)
    if (quantity >= 2) {
        // UINT32 Big Endian (ABCD)
        uint32_t uint32_abcd = ((uint32_t)registers[0] << 16) | registers[1];
        // UINT32 Little Endian (DCBA) 
        uint32_t uint32_dcba = ((uint32_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) | 
                               (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
        // UINT32 Mid-Big Endian (BADC)
        uint32_t uint32_badc = ((uint32_t)registers[1] << 16) | registers[0];
        // UINT32 Mid-Little Endian (CDAB)
        uint32_t uint32_cdab = ((uint32_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 16) | 
                               (((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF));
        
        snprintf(temp, sizeof(temp), 
                 "\"uint32_abcd\":%" PRIu32 ",\"uint32_dcba\":%" PRIu32 ",\"uint32_badc\":%" PRIu32 ",\"uint32_cdab\":%" PRIu32 ",",
                 uint32_abcd, uint32_dcba, uint32_badc, uint32_cdab);
        strcat(formats, temp);
        
        // INT32 formats
        snprintf(temp, sizeof(temp), 
                 "\"int32_abcd\":%" PRId32 ",\"int32_dcba\":%" PRId32 ",\"int32_badc\":%" PRId32 ",\"int32_cdab\":%" PRId32 ",",
                 (int32_t)uint32_abcd, (int32_t)uint32_dcba, (int32_t)uint32_badc, (int32_t)uint32_cdab);
        strcat(formats, temp);
        
        // FLOAT32 formats
        union { uint32_t i; float f; } converter;
        
        converter.i = uint32_abcd;
        float float_abcd = converter.f;
        converter.i = uint32_dcba;
        float float_dcba = converter.f;
        converter.i = uint32_badc;
        float float_badc = converter.f;
        converter.i = uint32_cdab;
        float float_cdab = converter.f;
        
        snprintf(temp, sizeof(temp), 
                 "\"float32_abcd\":%.6e,\"float32_dcba\":%.6e,\"float32_badc\":%.6e,\"float32_cdab\":%.6e",
                 float_abcd, float_dcba, float_badc, float_cdab);
        strcat(formats, temp);
    }
    
    strcat(formats, "}");
    snprintf(json_output, json_size, "%s", formats);
}

// Enhanced conversion supporting specific byte order formats (deprecated - use sensor_manager.h version)
__attribute__((unused)) static float convert_modbus_data_legacy(uint16_t* registers, int quantity, const char* data_type) {
    if (!registers || quantity <= 0) return 0.0;
    
    // 16-bit formats (single register)
    if (strcmp(data_type, "UINT16_BE") == 0 || strcmp(data_type, "UINT16") == 0) {
        return (float)registers[0];  // Big endian (register as-is)
    }
    else if (strcmp(data_type, "UINT16_LE") == 0) {
        uint16_t swapped = ((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF);
        return (float)swapped;  // Little endian (byte swapped)
    }
    else if (strcmp(data_type, "INT16_BE") == 0 || strcmp(data_type, "INT16") == 0) {
        return (float)((int16_t)registers[0]);  // Big endian signed
    }
    else if (strcmp(data_type, "INT16_LE") == 0) {
        uint16_t swapped = ((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF);
        return (float)((int16_t)swapped);  // Little endian signed
    }
    
    // 32-bit formats (two registers) - need at least 2 registers
    else if (quantity >= 2) {
        // UINT32 formats
        if (strcmp(data_type, "UINT32_ABCD") == 0 || strcmp(data_type, "UINT32") == 0 || strcmp(data_type, "UINT32_1234") == 0) {
            uint32_t value = ((uint32_t)registers[0] << 16) | registers[1];  // ABCD
            return (float)value;
        }
        else if (strcmp(data_type, "UINT32_DCBA") == 0 || strcmp(data_type, "UINT32_SWAP") == 0 || strcmp(data_type, "UINT32_4321") == 0) {
            // DCBA - Complete byte swap
            uint32_t value = ((uint32_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) | 
                           (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
            return (float)value;
        }
        else if (strcmp(data_type, "UINT32_BADC") == 0 || strcmp(data_type, "UINT32_2143") == 0) {
            uint32_t value = ((uint32_t)registers[1] << 16) | registers[0];  // BADC - Word swap
            return (float)value;
        }
        else if (strcmp(data_type, "UINT32_CDAB") == 0 || strcmp(data_type, "UINT32_3412") == 0) {
            // CDAB - Byte swap within words
            uint32_t value = ((uint32_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 16) | 
                           (((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF));
            return (float)value;
        }
        
        // INT32 formats
        else if (strcmp(data_type, "INT32_ABCD") == 0 || strcmp(data_type, "INT32") == 0 || strcmp(data_type, "INT32_1234") == 0) {
            int32_t value = ((int32_t)registers[0] << 16) | registers[1];  // ABCD
            return (float)value;
        }
        else if (strcmp(data_type, "INT32_DCBA") == 0 || strcmp(data_type, "INT32_SWAP") == 0 || strcmp(data_type, "INT32_4321") == 0) {
            // DCBA - Complete byte swap
            uint32_t uvalue = ((uint32_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) | 
                            (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
            return (float)((int32_t)uvalue);
        }
        else if (strcmp(data_type, "INT32_BADC") == 0 || strcmp(data_type, "INT32_2143") == 0) {
            int32_t value = ((int32_t)registers[1] << 16) | registers[0];  // BADC - Word swap
            return (float)value;
        }
        else if (strcmp(data_type, "INT32_CDAB") == 0 || strcmp(data_type, "INT32_3412") == 0) {
            // CDAB - Byte swap within words
            uint32_t uvalue = ((uint32_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 16) | 
                            (((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF));
            return (float)((int32_t)uvalue);
        }
        
        // FLOAT32 formats
        else if (strcmp(data_type, "FLOAT32_ABCD") == 0 || strcmp(data_type, "FLOAT32") == 0 || strcmp(data_type, "FLOAT32_1234") == 0) {
            union { uint32_t i; float f; } converter;
            converter.i = ((uint32_t)registers[0] << 16) | registers[1];  // ABCD
            return converter.f;
        }
        else if (strcmp(data_type, "FLOAT32_DCBA") == 0 || strcmp(data_type, "FLOAT32_SWAP") == 0 || strcmp(data_type, "FLOAT32_4321") == 0) {
            union { uint32_t i; float f; } converter;
            // DCBA - Complete byte swap
            converter.i = ((uint32_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) | 
                        (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
            return converter.f;
        }
        else if (strcmp(data_type, "FLOAT32_BADC") == 0 || strcmp(data_type, "FLOAT32_2143") == 0) {
            union { uint32_t i; float f; } converter;
            converter.i = ((uint32_t)registers[1] << 16) | registers[0];  // BADC - Word swap
            return converter.f;
        }
        else if (strcmp(data_type, "FLOAT32_CDAB") == 0 || strcmp(data_type, "FLOAT32_3412") == 0) {
            union { uint32_t i; float f; } converter;
            // CDAB - Byte swap within words
            converter.i = ((uint32_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 16) | 
                        (((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF));
            return converter.f;
        }
    }
    
    // Default to UINT16 Big Endian if unknown type
    return (float)registers[0];
}

// Test sensor connection function
esp_err_t test_sensor_connection(const sensor_config_t *sensor, char *result_buffer, size_t buffer_size)
{
    if (!sensor || !result_buffer || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Testing sensor connection: %s (Slave ID: %d, Register: %d)", 
             sensor->name, sensor->slave_id, sensor->register_address);
    
    // Validate basic parameters
    if (sensor->slave_id < 1 || sensor->slave_id > 247) {
        snprintf(result_buffer, buffer_size, "[ERROR] Invalid Slave ID: %d (must be 1-247)", sensor->slave_id);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (sensor->register_address < 0 || sensor->register_address > 65535) {
        snprintf(result_buffer, buffer_size, "[ERROR] Invalid Register Address: %d (must be 0-65535)", sensor->register_address);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (sensor->quantity < 1 || sensor->quantity > 125) {
        snprintf(result_buffer, buffer_size, "[ERROR] Invalid Quantity: %d (must be 1-125)", sensor->quantity);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Perform Modbus read operation
    modbus_result_t modbus_result;
    
    // Use holding registers by default, or input registers if specified
    if (sensor->register_type[0] && strcmp(sensor->register_type, "INPUT_REGISTER") == 0) {
        modbus_result = modbus_read_input_registers(sensor->slave_id, sensor->register_address, sensor->quantity);
    } else {
        modbus_result = modbus_read_holding_registers(sensor->slave_id, sensor->register_address, sensor->quantity);
    }
    
    if (modbus_result != MODBUS_SUCCESS) {
        const char* error_description = "";
        switch (modbus_result) {
            case MODBUS_TIMEOUT:
                error_description = "Communication timeout - device not responding";
                break;
            case MODBUS_INVALID_CRC:
                error_description = "Invalid CRC - communication error or interference";
                break;
            case MODBUS_ILLEGAL_FUNCTION:
                error_description = "Illegal function - register type not supported by device";
                break;
            case MODBUS_ILLEGAL_DATA_ADDRESS:
                error_description = "Illegal data address - register not available on device";
                break;
            case MODBUS_ILLEGAL_DATA_VALUE:
                error_description = "Illegal data value - invalid quantity or data format";
                break;
            case MODBUS_SLAVE_DEVICE_FAILURE:
                error_description = "Slave device failure - device internal error";
                break;
            case MODBUS_SLAVE_DEVICE_BUSY:
                error_description = "Slave device busy - try again later";
                break;
            default:
                error_description = "Unknown Modbus error";
                break;
        }
        
        snprintf(result_buffer, buffer_size,
                 "[ERROR] <strong>Modbus Communication Failed</strong><br>"
                 "Error Code: %d (%s)<br>"
                 "Device: Slave ID %d<br>"
                 "Register: %d (%s)<br>"
                 "Quantity: %d registers<br>"
                 "<strong>Troubleshooting:</strong><br>"
                 "* Check RS485 wiring (A+, B-, GND)<br>"
                 "* Verify device is powered and functional<br>"
                 "* Confirm slave ID matches device configuration<br>"
                 "* Check register address is valid for this device<br>"
                 "* Ensure baud rate and parity settings match device",
                 modbus_result, error_description, sensor->slave_id, sensor->register_address,
                 sensor->register_type[0] ? sensor->register_type : "HOLDING", sensor->quantity);
        
        return ESP_FAIL;
    }
    
    // Read successful - get the raw register values
    uint16_t registers[125];
    uint8_t response_length = modbus_get_response_length();
    
    if (response_length < sensor->quantity) {
        snprintf(result_buffer, buffer_size, "[ERROR] Insufficient data received: got %d registers, expected %d", 
                 response_length, sensor->quantity);
        return ESP_FAIL;
    }
    
    // Copy register values
    for (int i = 0; i < sensor->quantity && i < response_length; i++) {
        registers[i] = modbus_get_response_buffer(i);
    }
    
    // Convert data based on sensor data type
    double converted_value = 0.0;
    uint32_t raw_value = 0;
    esp_err_t convert_result = convert_modbus_data(registers, sensor->quantity, 
                                                   sensor->data_type, sensor->byte_order,
                                                   (double)sensor->scale_factor, &converted_value, &raw_value);
    
    if (convert_result != ESP_OK) {
        snprintf(result_buffer, buffer_size, "[ERROR] Data conversion failed for type: %s", sensor->data_type);
        return ESP_FAIL;
    }
    
    // Apply level sensor calculations if this is a Level or Radar Level sensor
    double final_value = converted_value;
    double level_percentage = 0.0;
    bool is_level_sensor = (sensor->sensor_type[0] && strcmp(sensor->sensor_type, "Level") == 0);
    bool is_radar_level_sensor = (sensor->sensor_type[0] && strcmp(sensor->sensor_type, "Radar Level") == 0);
    
    if (is_level_sensor && sensor->max_water_level > 0) {
        // Level sensor calculation: (Sensor Height - Raw Value) / Maximum Water Level * 100
        level_percentage = ((sensor->sensor_height - converted_value) / sensor->max_water_level) * 100.0;
        // Ensure percentage is within reasonable bounds
        if (level_percentage < 0) level_percentage = 0.0;
        if (level_percentage > 100) level_percentage = 100.0;
        final_value = level_percentage;
        ESP_LOGI(TAG, "Level Test: Raw=%.6f, Height=%.2f, MaxLevel=%.2f -> %.2f%%", 
                 converted_value, sensor->sensor_height, sensor->max_water_level, level_percentage);
    } else if (is_radar_level_sensor && sensor->max_water_level > 0) {
        // Radar Level sensor calculation: (Raw Value / Maximum Water Level) * 100
        level_percentage = (converted_value / sensor->max_water_level) * 100.0;
        // Ensure percentage is not negative (but allow over 100% to show overflow)
        if (level_percentage < 0) level_percentage = 0.0;
        final_value = level_percentage;
        ESP_LOGI(TAG, "Radar Level Test: Raw=%.6f, MaxLevel=%.2f -> %.2f%%", 
                 converted_value, sensor->max_water_level, level_percentage);
    }
    
    // Format successful result
    char raw_hex[256] = "";
    char hex_part[8];
    for (int i = 0; i < sensor->quantity; i++) {
        snprintf(hex_part, sizeof(hex_part), "%04X ", registers[i]);
        strncat(raw_hex, hex_part, sizeof(raw_hex) - strlen(raw_hex) - 1);
    }
    
    // Get current timestamp
    time_t now;
    time(&now);
    char timestamp[32];
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    if (is_level_sensor && sensor->max_water_level > 0) {
        snprintf(result_buffer, buffer_size,
                 "[OK] <strong>Level Sensor Test Successful</strong><br>"
                 "Timestamp: %s<br>"
                 "Device: Slave ID %d<br>"
                 "Register: %d (%s)<br>"
                 "Quantity: %d registers<br>"
                 "Data Type: %s<br>"
                 "Sensor Height: %.2f<br>"
                 "Max Water Level: %.2f<br>"
                 "<strong>Results:</strong><br>"
                 "Water Level: <span style='color:#155724;font-weight:bold'>%.2f%%</span><br>"
                 "Raw Sensor Reading: %.6f<br>"
                 "Operation Comparison: %.0f → %.1f%% ✅<br>"
                 "Calculation: (%.2f - %.6f) / %.2f × 100 = %.2f%%<br>"
                 "Raw Numeric: %" PRIu32 " (0x%08" PRIX32 ")<br>"
                 "Raw Registers: %s<br>"
                 "<strong>Communication:</strong><br>"
                 "[OK] RS485 communication successful<br>"
                 "[OK] Device responding normally<br>"
                 "[OK] Level calculation applied",
                 timestamp, sensor->slave_id, sensor->register_address,
                 sensor->register_type[0] ? sensor->register_type : "HOLDING",
                 sensor->quantity, sensor->data_type, 
                 sensor->sensor_height, sensor->max_water_level,
                 level_percentage, converted_value,
                 converted_value, level_percentage,
                 sensor->sensor_height, converted_value, sensor->max_water_level, level_percentage,
                 raw_value, raw_value, raw_hex);
    } else if (is_level_sensor && sensor->max_water_level <= 0) {
        snprintf(result_buffer, buffer_size,
                 "[WARNING] <strong>Level Sensor Configuration Incomplete</strong><br>"
                 "Timestamp: %s<br>"
                 "Device: Slave ID %d<br>"
                 "Register: %d (%s)<br>"
                 "Quantity: %d registers<br>"
                 "Data Type: %s<br>"
                 "Sensor Height: %.2f<br>"
                 "Max Water Level: %.2f<br>"
                 "<strong>Raw Results:</strong><br>"
                 "Raw Sensor Reading: <span style='color:#856404;font-weight:bold'>%.6f</span><br>"
                 "Raw Numeric: %" PRIu32 " (0x%08" PRIX32 ")<br>"
                 "Raw Registers: %s<br>"
                 "<strong>Communication:</strong><br>"
                 "[OK] RS485 communication successful<br>"
                 "[WARNING] Max Water Level not configured (%.2f)<br>"
                 "[INFO] Please set Max Water Level > 0 for level calculation",
                 timestamp, sensor->slave_id, sensor->register_address,
                 sensor->register_type[0] ? sensor->register_type : "HOLDING",
                 sensor->quantity, sensor->data_type, 
                 sensor->sensor_height, sensor->max_water_level,
                 converted_value, raw_value, raw_value, raw_hex, sensor->max_water_level);
    } else if (is_radar_level_sensor && sensor->max_water_level > 0) {
        snprintf(result_buffer, buffer_size,
                 "[OK] <strong>Radar Level Sensor Test Successful</strong><br>"
                 "Timestamp: %s<br>"
                 "Device: Slave ID %d<br>"
                 "Register: %d (%s)<br>"
                 "Quantity: %d registers<br>"
                 "Data Type: %s<br>"
                 "Max Water Level: %.2f<br>"
                 "<strong>Results:</strong><br>"
                 "Water Level: <span style='color:#155724;font-weight:bold'>%.2f%%</span><br>"
                 "Raw Sensor Reading: %.6f<br>"
                 "Operation Comparison: %.0f → %.1f%% ✅<br>"
                 "Calculation: %.6f / %.2f × 100 = %.2f%%<br>"
                 "Raw Numeric: %" PRIu32 " (0x%08" PRIX32 ")<br>"
                 "Raw Registers: %s<br>"
                 "<strong>Communication:</strong><br>"
                 "[OK] RS485 communication successful<br>"
                 "[OK] Device responding normally<br>"
                 "[OK] Radar level calculation applied",
                 timestamp, sensor->slave_id, sensor->register_address,
                 sensor->register_type[0] ? sensor->register_type : "HOLDING",
                 sensor->quantity, sensor->data_type, 
                 sensor->max_water_level,
                 level_percentage, converted_value,
                 converted_value, level_percentage,
                 converted_value, sensor->max_water_level, level_percentage,
                 raw_value, raw_value, raw_hex);
    } else if (is_radar_level_sensor && sensor->max_water_level <= 0) {
        snprintf(result_buffer, buffer_size,
                 "[WARNING] <strong>Radar Level Sensor Configuration Incomplete</strong><br>"
                 "Timestamp: %s<br>"
                 "Device: Slave ID %d<br>"
                 "Register: %d (%s)<br>"
                 "Quantity: %d registers<br>"
                 "Data Type: %s<br>"
                 "Max Water Level: %.2f<br>"
                 "<strong>Raw Results:</strong><br>"
                 "Raw Sensor Reading: <span style='color:#856404;font-weight:bold'>%.6f</span><br>"
                 "Raw Numeric: %" PRIu32 " (0x%08" PRIX32 ")<br>"
                 "Raw Registers: %s<br>"
                 "<strong>Communication:</strong><br>"
                 "[OK] RS485 communication successful<br>"
                 "[WARNING] Max Water Level not configured (%.2f)<br>"
                 "[INFO] Please set Max Water Level > 0 for radar level calculation",
                 timestamp, sensor->slave_id, sensor->register_address,
                 sensor->register_type[0] ? sensor->register_type : "HOLDING",
                 sensor->quantity, sensor->data_type, 
                 sensor->max_water_level,
                 converted_value, raw_value, raw_value, raw_hex, sensor->max_water_level);
    } else {
        snprintf(result_buffer, buffer_size,
                 "[OK] <strong>Sensor Test Successful</strong><br>"
                 "Timestamp: %s<br>"
                 "Device: Slave ID %d<br>"
                 "Register: %d (%s)<br>"
                 "Quantity: %d registers<br>"
                 "Data Type: %s<br>"
                 "Scale Factor: %.3f<br>"
                 "<strong>Results:</strong><br>"
                 "Converted Value: <span style='color:#155724;font-weight:bold'>%.6f</span><br>"
                 "Raw Numeric: %" PRIu32 " (0x%08" PRIX32 ")<br>"
                 "Raw Registers: %s<br>"
                 "<strong>Communication:</strong><br>"
                 "[OK] RS485 communication successful<br>"
                 "[OK] Device responding normally<br>"
                 "[OK] Data format valid",
                 timestamp, sensor->slave_id, sensor->register_address,
                 sensor->register_type[0] ? sensor->register_type : "HOLDING",
                 sensor->quantity, sensor->data_type, sensor->scale_factor,
                 converted_value, raw_value, raw_value, raw_hex);
    }
    
    ESP_LOGI(TAG, "Sensor test successful: %s = %.6f%s", sensor->name, final_value, is_level_sensor ? "%" : "");
    return ESP_OK;
}

// Forward declarations for new REST API handlers
static esp_err_t save_network_mode_handler(httpd_req_t *req);
static esp_err_t save_sim_config_handler(httpd_req_t *req);
static esp_err_t save_sd_config_handler(httpd_req_t *req);
static esp_err_t save_rtc_config_handler(httpd_req_t *req);
static esp_err_t save_telegram_config_handler(httpd_req_t *req);
static esp_err_t api_telegram_test_handler(httpd_req_t *req);
static esp_err_t api_modbus_poll_handler(httpd_req_t *req);
static esp_err_t api_sim_test_handler(httpd_req_t *req);
static esp_err_t api_sim_test_status_handler(httpd_req_t *req);
static esp_err_t api_sd_status_handler(httpd_req_t *req);
static esp_err_t api_sd_clear_handler(httpd_req_t *req);
static esp_err_t api_sd_replay_handler(httpd_req_t *req);
static esp_err_t api_rtc_time_handler(httpd_req_t *req);
static esp_err_t api_rtc_sync_handler(httpd_req_t *req);
static esp_err_t api_rtc_set_handler(httpd_req_t *req);
static esp_err_t api_modbus_status_handler(httpd_req_t *req);
static esp_err_t api_azure_status_handler(httpd_req_t *req);
static esp_err_t api_telemetry_history_handler(httpd_req_t *req);
static esp_err_t modbus_scan_handler(httpd_req_t *req);
static esp_err_t modbus_read_live_handler(httpd_req_t *req);

// WiFi scan handler
// Global scan state management
static bool scan_in_progress = false;
static SemaphoreHandle_t scan_mutex = NULL;


static esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "=== Simple WiFi scan requested ===");
    
    // Initialize mutex if needed
    if (scan_mutex == NULL) {
        scan_mutex = xSemaphoreCreateMutex();
    }
    
    // Check if scan already running
    if (scan_in_progress) {
        const char* busy_response = "{\"error\":\"Scan already in progress. Please wait.\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, busy_response, strlen(busy_response));
        return ESP_OK;
    }
    
    scan_in_progress = true;
    
    // Simple approach: Just do the scan without complex mode switching
    ESP_LOGI(TAG, "Starting simple WiFi scan...");
    
    // Basic scan configuration
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,        // Scan all channels
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE
    };
    
    // Start the scan (blocking)
    esp_err_t scan_err = esp_wifi_scan_start(&scan_config, true);
    ESP_LOGI(TAG, "Scan result: %s", esp_err_to_name(scan_err));
    
    if (scan_err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(scan_err));
        
        char error_response[100];
        snprintf(error_response, sizeof(error_response), 
                "{\"error\":\"Scan failed: %s\"}", esp_err_to_name(scan_err));
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, error_response, strlen(error_response));
        scan_in_progress = false;
        return ESP_OK;
    }
    
    // Get scan results
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    ESP_LOGI(TAG, "Found %d networks", ap_count);
    
    if (ap_count == 0) {
        const char* empty_response = "{\"count\":0,\"networks\":[]}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, empty_response, strlen(empty_response));
        scan_in_progress = false;
        return ESP_OK;
    }
    
    // Limit networks to prevent memory issues
    if (ap_count > 10) ap_count = 10;
    
    wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_list) {
        const char* error_response = "{\"error\":\"Memory allocation failed\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, error_response, strlen(error_response));
        scan_in_progress = false;
        return ESP_OK;
    }
    
    esp_wifi_scan_get_ap_records(&ap_count, ap_list);
    
    // Build simple JSON response
    char *response = malloc(1024);
    if (!response) {
        free(ap_list);
        httpd_resp_send_500(req);
        scan_in_progress = false;
        return ESP_FAIL;
    }
    
    snprintf(response, 1024, "{\"count\":%d,\"networks\":[", ap_count);
    
    for (int i = 0; i < ap_count; i++) {
        char network_item[150];
        const char* security = (ap_list[i].authmode == WIFI_AUTH_OPEN) ? "Open" : "Secured";
        const char* strength = (ap_list[i].rssi > -70) ? "Good" : "Weak";
        
        snprintf(network_item, sizeof(network_item),
            "%s{\"ssid\":\"%.32s\",\"rssi\":%d,\"auth\":\"%s\",\"signal_strength\":\"%s\",\"channel\":%d}",
            (i > 0) ? "," : "",
            ap_list[i].ssid,
            ap_list[i].rssi,
            security,
            strength,
            ap_list[i].primary
        );
        strcat(response, network_item);
    }
    strcat(response, "]}");
    
    free(ap_list);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    free(response);
    
    scan_in_progress = false;
    ESP_LOGI(TAG, "=== WiFi scan completed ===");
    
    return ESP_OK;
}

// Live data handler
static esp_err_t live_data_handler(httpd_req_t *req)
{
    char response[1024];
    time_t now = time(NULL);
    struct tm timeinfo;
    char timestamp[64];
    
    gmtime_r(&now, &timeinfo);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    
    // Get system configuration for real sensor data
    system_config_t* config = get_system_config();
    
    // Start JSON response
    snprintf(response, sizeof(response), 
        "{\"timestamp\":\"%s\",\"sensors\":[", timestamp);
    
    // Add configured sensors with simulated or real data
    for (int i = 0; i < config->sensor_count && i < 8; i++) {
        if (config->sensors[i].enabled) {
            char sensor_data[200];
            float simulated_value = 100.0 + (esp_random() % 5000) / 100.0;
            
            snprintf(sensor_data, sizeof(sensor_data),
                "%s{\"name\":\"%s\",\"unit_id\":\"%s\",\"value\":%.2f,\"slave_id\":%d,\"register\":%d,\"status\":\"simulated\"}",
                (strlen(response) > 50) ? "," : "",
                config->sensors[i].name,
                config->sensors[i].unit_id,
                simulated_value,
                config->sensors[i].slave_id,
                config->sensors[i].register_address
            );
            strncat(response, sensor_data, sizeof(response) - strlen(response) - 1);
        }
    }
    
    // If no sensors configured, add placeholder
    if (config->sensor_count == 0) {
        strncat(response, "{\"name\":\"No sensors configured\",\"value\":0,\"status\":\"none\"}", 
                sizeof(response) - strlen(response) - 1);
    }
    
    strncat(response, "]}", sizeof(response) - strlen(response) - 1);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}


// Company logo handler - FluxGen branding with water droplet
static esp_err_t logo_handler(httpd_req_t *req)
{
    // FluxGen logo PNG with transparent background - base64 encoded
    static const char* logo_base64 =
        "iVBORw0KGgoAAAANSUhEUgAAASYAAAByCAYAAAD+rHyPAAAQAElEQVR4Aex9B4BVxfX3mZlbX3/bWUA6SrFiCZb8saGgoqBr1KhRY9AUEzWaYmKC6c0kxiQm"
        "thhrdC0ICPYQY0NFbKAoUgSW7a/ed/vMd+Yti4ALgpGSL2+4c2fu3JkzZ84585szc3cXCpVQkUBFAhUJ7GYSqADTbqaQCjsVCVQkAFABpooVVCRQkcBuJ4EK"
        "MO12Ktn9GapwWJHAjpZABZh2tIQr9CsSqEhguyVQAabtFlmlQUUCFQnsaAlUgGlHS7hCvyKBigQAtlMGFWDaToFVqlckUJHAjpdABZh2vIwrPVQkUJHAdkqg"
        "AkzbKbBK9YoEKhLY8RKoANOOl/Hu30OFw4oEdjMJVIBpN1NIhZ2KBCoSqPzkd8UGKhKoSGA3lEDFY9oNlVJhqSKB3V8CO5bDCjDtWPlWqFckUJHAJ5BABZg+"
        "gdAqTXaOBL567Y0jL/nZb0bvnN4qvexOEqgA0+6kjQovGyTQ9Ncn9iBDDj5f6zds3IbCSuZ/RgIVYPr/UtX/3YOa8c9/KtAwfGpWT3/OURL6f/doKtx/EglU"
        "gOmTSK3SZodKYEl3+ggejfzMTNUM8Wm0YqM7VNq7J/GK0ndPvfzPcnX2A6/0EzX9rglNI1ISXAQQc/5nhfE/PPD/cmBqYgAz5BgIgMxvrEksnzBBgdFN2sal"
        "G+dj+x43JrrP1OMje58+OTJ22gmRUadOjow6eXJk5KRJ0eFHTawecsTBG9ev5HesBOQWrhCv+15GixxhCQ6WY5NQU3dspxXqvRLYrVI5qXcqQ9G6sfVHnXrJ"
        "jSd8Yca9R5757VtP+cqP7zvjit+8evZV1/3rc1f89v7JX5rxp0Omff1HI44653v9x5929YBDp/2wcfzJ1ww4/PQZif1P+kFk7+OuTu435bvKyGOvSo4j34oc"
        "8O53YPRp343sp387vt/ZV6UPOPN76QPP/J6x75KrUv6wqwwz9q2a8V84cz2AfThWBC1dr744WTdiXrx+xCOJ+pFzEg3DMR3+SLrfyLl1jSMfqx444q6aPQ+L"
        "f9ioktuRElhjN56l1vT/Us4ToKg6RGM6gAIGVML/nATozh7xxONPSnbm7aZXliw9feE7K89/auG7TY88v3j/mf9c9NlZ/3r11CcXLv3Ka6s6r24p8J/khf6j"
        "vDBndDrKD3KB8kNipq7RE40/cpX4zxJ1Q36ac8XPSgr9qVlX/VNXU3/qaPSntkJ+4jD4iVpV9eNCKK6hkeiPqWFeOOzQ12s2GWt7LRVC0Tsy3dCe7Ya2XA7a"
        "C1nosgqQsUuQdRwouG68M0crE2MTwe2Yh0vmvjnMTddd017wNF2JQCGbAe5aoFEv2DE9VqjuzhLY6cC05/5jAyVipvK2C6FqQjFUyrEUquCAAR4xy9HhGlg+"
        "w0jAAw18jDk7gJzjY16BjOUCiySA4Mpq53OYMghR0gEI4JRBIVcAYAa4PkAQMqXbLbj4+sPLTIuSIwRnBAQLQdAA23HwCQcXAvAB8xB4sdiHTSq5HSOBpvvu"
        "Y3aq5lc50AeHqHfwKSQicWC4nTOoG9sxvVao7s4S2OnA9P7yVQU56bnGwOEhEKqAqmrAfR+oouAzA03XESgIQgMguDCgqorgwoEwDQRRsVwBwVQIQwDh4o1o"
        "wB0PSMCBhgTAJ8CUCAgPgCDwCY8UVTfyEc/HC4kQAtszrKjINABAgBLUR5BzwQtsX4QehV0f/r/moC4x7hIXktNsVwMSUFADAgwXKor61LiVg0r4n5PATp90"
        "+VJHKChBsEEQUhjwwIXQD4AoCDSYiiCAAEGKEALRKB7vCAoEGISEImgwEBQjUQCwDASefYMChDAg+EwR5AjmhawrCABQoJQC5z4Nor6NBR9edoZoZlRQgbWw"
        "D9gkMuBYjqhFCWWY+7BZJffpSuCrs14bAzUDv1viJvjoIUdx+eABgFOwIRWJeEkRvvPp9lih9t8gAZztO5fN91sCKxDo6zAEDCHAjEQhxDMdBZ8NXQOKYBV6"
        "PnpQAVjZLCA4oIckQACFMuAg+CCKAaCXBFQDwC2eAAQ1BCwfwcwlAB62CHEbIMAHpgTgBgUhAh/fYPWNLs/No8tEcZXWgIQ4I2TECUK4jq4Yem1ctYBUgGkj"
        "kX2q2Sbcwlmpht+uKHh1XTlEIwGAO3zQTYBYNArUDW74wSlHLfhUO60Q+6+QAN3ZXCYVl3men+Po0fCQg10sgmKawDkHJ18AQ9VA11VQFQqReBQMTQFNZYAJ"
        "KOjeMCJ6sIIBoDsEhFKMCgCjGAkQTCmCF6MEVKzLQhtTR2Dtj1w8sAGRCQgXgFV73qPnJAFQcAKCU02EGhLueVW5f7oSqK854Jt5PToxCwyMuAK4TgGuKRDY"
        "NtTFtde8jq5ffLo9/m9R+28e7U6fdI4XGq7ru4IzAKEANSIQ4PYNCAUlYkLJskCgLx+LGCvjpvKeTvnbOviLNe4+r/HSS2pYeFn1Cy8oQRaj9aLK+QtUwPNI"
        "7HlEl9cQYF5Dqq9pQBYYXCyIK2Qhd6y3ADKweYink4QSG4CWMFoAxAVgGCHEvIxCEOZxqIRPXQLnz3zp0EIkfXWH6wExdZBLBxcCFyQfBtUanUH7+9/98+lj"
        "Wj/1jisE/ysksNOBCb+AaTykMUIQPtA7wpkPTDWAoucjAYqKAKpTscXdnWuPbVv+7kGisPxQ3tZxJHR+cArtWnMiK7ZMUQqtp2lW+6mavWaaUlhzmp5f0wSl"
        "NZ8Dp+0UHnSdTN2uqXqh83OK097kW61T9Bj9aWbhwtwmGsGvclYmBxSRjCCyyS0jkPU10NsSGDm6UtzHQ6z1xZXk05HApQ/9MxXfY/SMVofHFC0CVrEAuLmH"
        "eIRAlSaguHrp9becMPzRT6e3CpX/RgnsfGAKA6IrOgBHFBAUhOMAUwj4LqYISomYCVZXWwYW6ytg+ZO57Gvzs8Vl8zqKy/5djtaS+a2lpU+0lN5+bF05Lp3V"
        "UsIIb8xbA4vmroKFcz5wXpu5Mvv2g6u633p4deeiWS3dC+blP6qchRCNRAQBDXlBfoSMKlbDL3y4tQAETgK04i3Bpx+06hGXry56x1oc5W37EMMtfQTF75Yy"
        "kAjy82u9wo1b6/Wbd/+z5ox7XhrYdN/S/lPuebVx2h2v9JNxyt1v1k+8/fW6Sfe9Wnvm3a/UfPHm56um//Wv2MnWqFXe7Y4SoDubqdCnlGmmFuISyTEy3QAP"
        "zxQUVQVCCDhOCapSSRUmdCBy7UDu4nHhuS4HooJAECr3hGdN5RQocHSUBChM6Pj9uqfwU73/rxL75sNLPpuByFW+YgJlKhhEAwMXKN+1oF/a6GDda6/+8+kH"
        "t25NPmE8/htzwF6PBo3Dn4E99v+XN2TcP/3B454OB4x9Whu8z3ytfswzYeNezwRDhv3Lrzn4uq3R+g/e0QEDxps1ex4Wl3H48EkIrf8BtR3RdNw4NTFmfFVi"
        "wPiq1OAJqcZxJ0V2RDc7gibdEUS3RtNmiuCaSRWFgoLTXwQ+EKoCD0XZUBG1wHIQMGAHh/m1gmiq6hECIW7nmPCBEg8IeEAhAIHdC3TlqB8JMLvVq36fiXXK"
        "0COPjo6Zcrw+dsoJbK8TT4IhE0/SRx0/WRl23PGJkUd9ot+5MxsPGqgMPnwCDJ5wPBt01CQ24rgTZIQ9jzteGTP5+MjYYw/sgzGS3H/KgWzs1BMjB3zuxMSo"
        "E06IjJw4Sce2+qgTJ7PRJ01S9jz2MNjK7xD2QRPMfSYNSB7YdCzb++TJEYyJ/U44ITEWae/XdEL8M1+Yoo8/67gBxzVV9dW2t+wbD79Zn0sP/k2GpZnv+0B9"
        "D6FfB8J0iBgMqN316xun7fdsb/0tpaoRP6DLdkfnOB3aZvPh2QD2LHrBXp7vjfZEOMoFslcxpKMyPhsr0o2fyt9zSow5rsoYPfXwyNhTL9b2OvHn0dGT7ihU"
        "VT1Y1KrutaO1D7bHjXsjY07+mzbyxB9qI6ecYe518qE1e06Jb2kMH1euDv/s/jD8mOPYiGNOYEOPncyGHTVJGfLZicoe449WBh06HhAU+6KhDz12hDZ6SpO2"
        "9+k/1oI976GxUQ/Fhu03W20Y9KDLYn+P7XPK7/TRky+K7zftMNhnYrQvGrtDGd3ZTISuYAEQRZ7pCBECQQ4IAoPAKHd38kw8JPidvx2MHcpbE4BglHNCyt3I"
        "cyYFkB88gC3zhsXIkxCag6f05SpbvFmhfsSe+45/jKT6zYvWDZ8TqR00q2bomFl6etAj6YHD5+HzvelRE8dukcAWXthq7IzaYXs/VTdkzLzqwWPmVjXuNScx"
        "YK85scaR88zaIfNiNYN+O2jccf02bt447iQTlNjnBozcb7Yw62dH+42Yk2wYPrdq4Kg50YZhjyQaR87tP+rA+Ybqnr5xu63nZ1Chp6+iqQGPm9WDH1HSAx8x"
        "03vMYcnGOVpy8BxfrXo41X/YPQ6LD90aHSea/m6emgd1lAKw3QBiBn7syBdBwS18iornVGf1LVtr3/su1CPc5gxsuZjhAocfYQH35OCj7vLocedtBwKiQCAY"
        "+LbngpBve1tvX1q734kj4vueenlywOh/xxuH/Jum+t1AEg3fKdHEWbaaPt7TkpN8veYYR0mdbNPoeTRWNyPZb8g9kdo9nrGiqXui+596ZnTs0fXb02tizHFV"
        "SOcq1NmjNUP2mRPrN+yRVOOIucl+Ix6L1g97ctCe+z/GmDlhY5rxvQ4fqY08+ruRugEPRKsH3MfN9PcDNXpq3ief7SqFh3bb4shCoJ0mzJpLtao9/qJVD/p3"
        "dWrQH+JykdqY0G6SR1jYuZxwitYCQOTkB8IhJD5w6oGEK5kPIISQ4KED1HqwI0NHB0GA3KQHQhCN1peU+RMbFawv7yshHFIt7W0sCALo7u4GmXZ2dID0CvL5"
        "PKiGnnC8INFX262VxVOpKsuyaLFUgmKhBJZlQ7FYxNSCEpaV7JKS9cUmAN6ycHYpotFZrpV/Pwg86Mxkobtowbp1LWDjIXMxl4X21jbFMBMXJfY7dsTW+u99"
        "l9z/lSMbGwd8GXkB13XBQU+naLsQoJqsYhZUCMAtdNzVWF31dm+bzdOLHnnzWJqq+mqmlAMjokA8loZCpgTJiAqNcfV9t2XFd245/tDuzdv19ZzhIq3gF1yF"
        "EtwGeuBkOkF64NTQIFEbh1Rt3C+5pbVVETNbJdx1IFc+2L4gveDGw8/6drxh8NPEjF/b1t4xOtvVCZ5rowwCMKsbwRMmcBIBz6PgcQJCMyCkHIqlAriexZjK"
        "Toin6++KJQbe23jIaRO3lQNmhyFTtXihWIJu/ECTL9lg4Vmsi/IGPJ/1OI1qkWRyPT1ijDrhmECr/XN14/CfZXKFvUM8IlHQKDVcUiOaAgrlZRHIH052cT9g"
        "BwAFJyAFR1xQP3TEA7X7T7oAJkxQ1tPbLRK6K7gghHzYrZAsYBQCFzYBnHNMCYOwRf+w0g7I1dbibg37RdJlEMJUXoT08EYIAcrotimL8TLffhgAwYP8JBeP"
        "wwAAEABJREFUEJA0jsfDZ455nMyOwnBMsH3BKrosRFJBWSYC5SJwIlA0MgIUWcMzGsWMRLAGbBLWZdwFbjFzbwxPlHE7CpwySKTTIAFT8ABs24Z4uuZwosSP"
        "2qRhHw8Dxh9XFU/VfKG9qxsnoAsc63iOBxZ+5sfRQsg9wG46TBHc9sYdv7Hw9Ueuyx98bSSJ1f2+tWgrBGeLg5OsUPQhHk1AWMwAK3Tc0GBoL3+k4ZYKlKjt"
        "2rjlxmMAxkNIRKIof4AQhZXJ5PPt7e3XE6f0ta5Vy8711qz+9ZbIbKl80GdOGKVU9b+zEGq/WNWeH8Bxq8kFAVVVgeDXY8CvyaWiA6BHAZgBQFWQgEF1HQJK"
        "wfZK6BHKH0OhkC+6JO+R/yOJ/o9F9j75IuyTYNzqFdYzxtEopR0FQpI2QNVNBDwHch3duHBTXMq5BoPPM2Bs0zGxhsF/CtXY0V2WB5FUGkqoGxcXEI5246KM"
        "fC6AIM+gqIBZtIMQdemDj2Na25mvr+4/7JZ+dvUZW2VqJ7+kO7k/EIFHCSE9k4soQNElpyhjGmqgch0jAyWkXi1TxQ7lrRlAwVmG+gec8Xht2h0hBBgGGuA+"
        "A7YeItGoIIyCoiggPA+8QgGiicQGmoQRRTHQgGH7gqopuMgxIEwpR6CkTEDyLI2OhyK0LZ+WCze+LWn2FK/QrKniTdXQwceV1g3QzBEsdTTOmBmD1avWAjHi"
        "0+vHn7z3xk03z3u+eSQ1ouf42J7hxKQ48ShOQGbEwEHDj8R0YNS/5fBk7rXN28rn6X99JeLXDv1lSUuMLjgI07h9j5gq2AhOuspgRH36AW/te3+6fvIIV9bf"
        "lhi4zIsEKmjYgocMwVEBBhRcdAXSsYRhKlAfEc6KeeeOnf3Xi498ZVto9tZp2Pe4I5XUwLkdFj+2hPZI9DgUu9HrNSPghD6gFEHTdcAblFEaFx5QGOBKARxB"
        "H+c6SG+O6RrYCA6+QL5CBQoOhWjNoL/E9z/l8t6+tpTqAdOYwlTu+mhDAAHqr1C0QTUioKaroOh4YSSZphArHgBUu7rok5E+NcDzOdgYAwRoIRGIaYCP4IUE"
        "QqohuwwIVYBKe0IvSlUVCJgJH3QUpQd4Xe34U/ffEk87u5zu7A6DAJef9Z0STBU0dAW1yQgBJjhGTEEwESbka9hxYYzAE3c05767IYQA4hIR3Oy7AnwYctmc"
        "AFS7XKW0eAzMdDVYuSwQRrGUgoPWojIt+LDFtuU0XaMh9HSPIkJaRE4DjBTkM3ZKI4oe9kWt4+Xm14q5zlsICUHRdRCyEvIjvaWiXYJkwwDcktEDSg6cADCj"
        "TzvA86qaSKrmwpbWLuwb++R4+GcVUWy43fYCMEwTBPVe5WHhjubm5j758Af3/8Jah57SWghAjyWAor5D14HqRBQiPFxrf/DOD287/0gHtiMERFEVRQVdV4Gi"
        "J+ATBriLhpipgW05ms6MU6Lx2F8+d9fLJ28HWeg37rgD8Izmj11WMFjgV8MQF66AEIBIFCc3B6po2E8AnpWHiMmAMh8XVhsXuBIoKGfKsC4uHgEuAB6CNqcM"
        "5UYA2YV8yYXufAlqGof8puHgaSjzLXOmK1GPYKA4Hj1igggDrEyAaTr4jgsBEHlOOw6AnmOmq49w5O+Yeh7okQgQ9CBVjUAsEQcvnwUddQSainpiEOL8kgsa"
        "owIAQdYPXACmQskn0J5zqtAJPBt1HoHdINCdzQMJUDrYqVz15WQGdEgI8YFAiBFFLp9FyAnLo/Sw4g67ZgigniAEOyjfMMVLoFHJiFkAQlRheB8rI9VQIcR9"
        "PVAKAc4QG8+VQNOAMYaTR4dkVVrLu3kFtic0NTHL8Vngh+AFHL2eEKRRIXtlKpJHjF7GLskRlMs2vyUV/6GYqTytqBQYGmeAdGLJBBCqQA7PnuQfZqgdOORC"
        "c5/nD9q8bfnZjKHXwI43EFB85AP7AzUahUg8DoC92r4NKvPvaH3qrj7Pls57aNFgSDd838eJzTWzPDldpENRx9U4eeJ+9w//dNp+i2F7ghBEmGaQwfMzByXq"
        "YSyF7r9UM/yXEG7Rtl0nJEbE0WMHe/UDrz/9wVcv3hbycsvKjdTl+ZCNzlq4zcGvhgp6PeB7CCwUuBBgoNcbQ1031MRCkWt5KeK0P5gS+eYEzz0YJaUXEjrJ"
        "GRqDsptDKKi6gWagQYgH8rpG0RY0WLO2DWgs+eOa/Y9thC2EXFcHVRieEqFNuaUSaAg4oCjlX9lCJsr6Q5v4Wryq+mJpawQ9NTNigCo83NYySJkKmCyEun41"
        "QBCAFFyWFKXHjAXCmvyhYoZASnAq+rjAgGqAizuXxkHDzyhxZZvOHWEHhx5ud3Anm5CnfhlwpJHLcqnwEFcXGX0pKHzrCuJ1eOaO5k2AIAohRLJRjpInGcuG"
        "VS7ZxpsQZV4lEJVbIEm5hfJQ6Q5+FEKwIjE9Ua5Tfr8tt+XLKVMURhWGNqkgyCkg84RRIIQAAZwAOGWSKhZA36HlueYPwlL273IySVBTzAjk80XQMEVioOG5"
        "RXvGGqYYqSkDxjeZG1MZdMS0frgzOtfBQ44A+yJUQR4Y+Hj4W8JDYNU0IB2PLCDUexBwKmLc5Gq67z6m1Ozx/bZi0JjHhVlqnVNWXtUlMEWczC0d3fNvg+0N"
        "hIhMvjuI10TAQqD1QgERhTyZ+eDtc8LM6pvTUdMLgpAX3TBAcOpvVw+4ctLdCz72LA1I8lRqJj+fLThA0TMBSiGwLZQzB01RyilDQA2d4v2lrg/OSZPMuWo2"
        "cwFkV10Yc9ZdqNlt54aF1slKUPpxPGJ260jDQ1CxkQbDrWsQuhByFxRcINyQ7R+Y9cdtaej19bWEh75CQADaAHpoVk9VxkBRVbCyWRCEQQE/hFAEHI0JiBnU"
        "Em5hthoUfkbtzJfc7tWfF6WOC/H5W1HqzUwhUhkaKY8jRLAKEfQA9wuAtiSjPMfM5IqNiqp9pqezXXunO7t7RHqOQVBUvEDBCKZCSLVy5MwErkXBJWZVNF0/"
        "NHrQeQ3mvk39Zawae/LAyKhp/cx9pg0wRk0eZOKzOXzSAHPsxIHyZ2x6Y9XYMwbWjJvWLz36xD3iex1dvbXxEWIGuBCWq5T5wQdCCBDJG+Z1TVOAG6RcYWs3"
        "dJ8JIRCit8SlwtGAyopHOgzzSjlsjUAf7+yhxMfTXEEJbgckhgIEPkfMROTG6hz7Yoxyh0i/HAu2cEVE8GTo5mfjmo9zjQJFMPLxvAhECJLHEoKnFqv6kgds"
        "/41JWCU4MpqoPr6I6OR6IQg5RtkVpmY8AhFdkRP3hsyTzR9s3K43r7HBBwRabBpH4MC5Awy9No5esY2eTk0i8oHa9v41zaefHvbW3540xoQf+C4Q6oPOCNBi"
        "rvDymXuvTmXXXGf61usMQEQQAXyP0rwfGRqtH/nlqTc8VgdbCI2fPWOglqy5srO7AEAVoARljPSl+gHlxNEzrE7GoFTovF4PM1fmF82+p+WVR5dmlj+Z6162"
        "IP/Bm89mOl59bFnh5YefLzq5X5Bc9zdU4N0GeopEwgvhQMFDGeKZEerTcgMwU/3OhHHT1b5YQtAx0Py4QFsS6A0B8kNAIG8EAjy3AgXNEhdzDUHK0ChEdfZO"
        "qXvdxUnmfrU2L65pe+n+m/Nvzr2744V/3FJ45Z5fQ3HN5aTU9fWERnBPHqI9EQA1AoIqgEYBYOVBoQzkRwnL9/9HPSaVE1QGOkooaJlBgQtUHKDCZCQoIGD6"
        "waBF/0KU6Bwj2TDLjDf8gycb79FTDQ9pscZmPTXkdiXR/261etDf9Pige/T4gNu11MA71eQe94hk7d3EqG82qmvvSdU1/DQ+ckINdtPnJXhIMHzknUCrkBHQ"
        "E+I07NN4PtKot4Dg8BgFQggQTFH7IMqD6q2wjWmpgybSaSiDN9ByI0KQJsbyA4IEBUEMwUn5eQu31c/9o4WXCn/vX1/tq4gQvJAHw4yComkQILhxqoLLobbg"
        "BlNh3EkRSaZ2vxNHgB79ct5ygCkazk3UFVPQhikYugqONGTu/itOnUdk/c3jjBkzqEPMC/BYKR2CAJybYBcKICd3XMd+C+1f/vOZB6/evN22PieVQAM/DxCW"
        "wC9meYL6mmw78/wjVzrZD36lEaeVcp9ASHxKI1AikWlOtOY8WafPqMWP7cxZI+K4RQ0RkAie6ZimXv7RABPHqykMirnOe2OUXJ197dGVfdLoLVw4u6SohVnM"
        "c38bIqBRirpDXQUIcCF6XAGCDR5UQCC0Q5Kq3+eHB9sqkphhUIagS7AdQRp4AcVn2Y181lAfOj7j7vVZP9f2Rev1mXe2vPzw6iVLmj1ZZ+OYe+PxFe22cZNX"
        "yl4Xxa0oR1ATgGYjieJ4jWQSOA9B8ppMJEfU7HkY7tU3prDz8yi1ndupGgAuaOUIgIBESQCE+AC4P5aRCxeV5oDP3UM8cMfl7MIBOSd/eN6xDsv71iF5z/pM"
        "3i99tujZh+cD55iC7xxWCNwjC773f3nPPTxTsg7PlqzDCp5zqM/9I9WkZkIfoREnoe/k8I3AuJXrYzyScksCIeA6BGiAOJj1RXKY5SwWIZpQsl2yHq6YIp/J"
        "ckLQgHrIbLgTQqD8j1LFIXKTtOFVn5l0LHyqu2X1XQwnhpbGz8mZTmCMApWGiR5T0XIhVdPv7JQWOwjwbAvPGSarZuxw/PiDWzcXmK4DOn3g5vNAcGLtgWcX"
        "JvVubJl/T2dfHa4cceiw+pGjTsnh+QxFMMS5DUlDA7uzE6pUMr+FrHi8r3bbWhYJi8sTipOri4li/7SRTfIiLv09rZNCfdxUik+LoBAoJFQJF4DoRMPG2lMn"
        "3PDQ4J5aH94HNF1mWkI7A60Q8rkMqHgGIye8bVmgo8djuz6k0kkQbummzMLm3Ictt5yTv5vpFzLzGqqq10m74EBBoM5CNHtUKFoKld5qAoAMhz6CYZjyPSkv"
        "jvieEQEoxh59IR1CCCgqRcDnluZ0/7Hw2sPPw8eFhTf6KrhzogZdBr4DJi5MwnFAVxgCvAuh5+FXRAdQXHVOSI2PI7ej32/XZPk0mAmo8FDgvqSFKeCCgLoj"
        "OKc5pgKfOYT4lcHHw0cPhUUIAcGQTRSgTCkuv70Rd4EA8h1BxVMUMG4JiaKCjKqqgappoeCcQB9B/iBiMhEVBM2EkJ4qhBAg5MMISJeGiJx9tN+8iJKeduVy"
        "AVAe2/quCQbG0ecuv9y22zItJqKJGA4KgAMSlM0QyGUieoQGlOKAZcHHxJXzZ2YZL91bVxUNPTzz0PCLjY9fZHjoATXRBlFmHZlcQwj6OenVxmSqRb8UhARC"
        "DkA1DRRFgQAnqhFPoLg55vNzeFDaIrjoqT0mr+3ON7iCQxHPpIr5Eh7MEhhSVQ2ko+VX8488MvgYlrf6mnSu/XM023ah0rL8S0b78m/q+XXynKvcpvn0MZ5q"
        "d95OiFir6FHUA8FxoPzM2H6hYUwoV9roxgt8j2iq9v98PEsjhAEjFBgjgIMGaX9R/Mq6ds3a+8aq3y4AABAASURBVOKBul1/sE6PmO93tLc8JWc6oJUBUQFU"
        "jISVe/ddB4AHfQITYZ4ASlHNvFxX3qSXI1McECB3wBgD4Tv3x4Lsk+Xybbgxp7TGymTf1HV0MKUNUQq+h06A60E0HgcdPWlOlLgRR7dqG+jtyCp0RxLvkzZX"
        "pVzL7iYRFJ0mCixUgYGGUQVFqKASBfMMVwQFIAQAOcERyonAvBQorvyUB0A5x/YC9StQSQTrCijTDClAgGAVcCasIj5guz4uYbuedHIkiCB2wOaRUsLQOpBw"
        "H403KRLYlICk1VdlQgAZLXMP2xyW4MgFhJI3aYzlVDZGOchnhkQZZUwoBpPFHxc7Xn7o0Xz76t/2q02XjRGFCmjdwAsFYFQBPZqAgKhf9EC9lhrRMZ1dWZxH"
        "KsoTwO3OgJFMYrchmCoD18rctCVvSW7jWKpuvEAvK2QEzHgENEMHFnCIC7Eg2rn6xY/j9ePeX/+5I5/820kH3X//tIP+cceUMbfdes5RSzduE2vNPWcaiRe7"
        "Soh/lKBeVWDc0Ab2H/6RX8PxfWe/dS2tGqAMEgicgqpQwC0sYQoA2qdlWVCVTj/RsaS5uHEfH5eX3hX3gg8Y6okCQVoM7yqmSBcbh+i1MAof8eAAg+My4eOi"
        "rKo6PmETTiBE4JQPhBCQoKQwDXjoPr72nZe6YBsDpYqFhFbhYSXYRQs0ZCAWMUCCnu244LghAFWVoo3Et5HmjqpGdxThLdKl5a2H2jvRCEIQIQQIIUApKg9T"
        "gYoAkKwhuJTLsBwNRRoLwfpUEOgNjDFs16NsioBG8D0hDIUNIELhGUYKthR8z0dN9LwVAsENo3wihAAhRGa3LXLBN24vGxFCkC8KPYFQ3BHynvy23ptD27U3"
        "tJH0e1qKngTvOG8I2YatHFYtX4rv3sNCZ4WGawNhFJimAo1FIUSXvmR74PhIWzNHFG0f1PJ2QoCULzFNCHELJw9jDV27tyaZeq5MsI9b176nDnFYZFymUATD"
        "MECqysNJFoYCStnMbTd8/ohMH80+1aLbzj/S8Sx/AdMVIKgCwT0I8CANSOSQpt/fsefGnVHB99dVRa4c0NWZhZAhiBp4xNIjXDDRuxChtw72nBKvOWxKPD3u"
        "mGTVIZMSMl87oSlWv8/EaP0+55Rj7eimmCwfPnySnjz8rLRqaJ0MzYgB3tCmBadolyhjZEDypOusGrMfuQhlgjCdCpDMY8QaEjxAPuOACKWA+JGhIV+Cr7b5"
        "arGNQNc0NxpLADAESUUDy3Z7dIw7DI7gLA/mtUhc2WaiO6hiz6h3EPG+yIoQZwSAL9/hbAbOOIQKRwcnBA+LPRJAgGUcV7pyxIohIeg+qHj2RDHFbQV6VaFQ"
        "IETjCdHyOZaWdQ+ibGByfgWYD4BSBxyksIVL1+iHE76njnzeKBJmKGVee95u4U4FEYLjyx6jw0z5EqLnmWAoF2zXbQblcsnerE0vKZkyiuPrkedmtfp+bHll"
        "1qJCtuuvddXVKBmUZckBjltmtEz0hggIxGkXPRuiqBDK4SAZHoQoWg6+60L//v2gkMvc8M5Tt29xle7mpMpR9OEKLrq+bYNf8sDQDEikIoAexEIkuVMuq5hb"
        "YOrgcF4CEBipQI9QrwqVRH0vAxMmTFDMqHGIY2WAox2xRBX4LAI+foUkjJXHrWK7bC7/pUgidp0dRq931drruFb7R2b2uyEIzBuNuqE3GfXJm7y4diOvq/1L"
        "qDX+pb1//5u9gP4hUZU+n3IBlIeAHYCCFAEoEMLQOjkoKq0a3ddfecCFwA+4CNB8OKFoBQyAYWuCOuIcONJEGqs9wtt7x7It6eC6DuohYQd1DHocPBd500wA"
        "5CdEDwoog1R1nZHPZgns4kB3ev89X+UMIYdOpJB9FDRGVIIgPUpgeKah4Wpn4oGpoSkQwRjTGcTwFLUnVSCOzwmMMfR2y2Uagbgm6wBENQoxTYMYnp/gREOr"
        "gD4DD0PBSc8rgSAiI8itIipf4FYRi3BaelpPjS3fGTAgyDsAGg42wlmOY+JQpgUgt0OK4CGF7QozeNSMASBAy2YCWxMC2AMHlBIIfCBU0WE7Q1wRD7pW9pnQ"
        "DUE1I0hPIJ8cCE5EGoujl486wSJFoegl+YASKo9FRx3ku1uvS0WjWz1rUSLxRBEP1A0jAhQlr+LKLHlFJww3SG7ndrL7iatHDe1d/Li2WjLBkYqGolKohiuI"
        "XouP668JQIkyAP0TBGUcKzYAgbUZBaoyXORCKCG4GtHYyT5RzrcC9oWAxb5QDNRz2rLW5y2fnrm20zqzPeucmfPEWRnH+3ymaJ2F4z9bNcyzu7oy5V+oBUR5"
        "itQYUYChnGVEbAFCme55RdTqenbWJ6XOItFxKyztkRAClCI/GAHzkr8Av6aKMFhXCstfb2Bbw8raWp8wFWFSAdy3Abq04CMt6TmDhqaESJgvWFyPxz7C07b2"
        "8WnVQ3P/tEhtGx0hFCIE4QIRQUaVMNAUAyAAjARUPQaKapaE79+C35iv09zc77VS+x80r/s63em6jmfX/V73Mr8lVttvqdVxLS20X6uXuq41rO7fqYW238e8"
        "3HWa3fVbVrJ+lWtZ94cENbqRcp8XJSGRBkMIAWkEDFMNvS8aOoCfcwBICDSIBPAxwQf5ky8oSqpgTsHaBFMCDAj+C0BlgqIfCNsbgiAUoWDYjAFiBQAJcPVF"
        "Bw5Bk2M5cu+jOcH2hDULHniPlPJ/HdTQCKGLvBFszX0cf4gAFABTCPIOCFAuKLhAKLjdwweoq050GcK/d+X827biggLg/DEB5RhIHKYGTkcCjocS0sBC78yH"
        "nRQCZhUCcNttlBNoceAegYiiCVUVG/b2KwevVHxBTQW3NIQKYHh2CdLDYh4OuQhCAfw6LIAzFc1TAVWN4YE4B5zLOFADQqpDqEbARcABtGMBIVBGAEIfbNsC"
        "YBTbiXJKUTAc9RYGAnXJgGkxfKcK6OOnhiJRJrC+ZAkIx+aMIf8eSMUoUbOsK9vCw8E8unbbI8/lacpxrnFceKmKDf0iyE68ANUiONorgMZUEHZA8e0uvXY6"
        "AzQE3JkRQMGXI0cl+nYJmKqBhgewPh42Qhi+QSD4ifXy0MvzsXVXZheOvaz7hbsv61pwz6XWGzMv63zp3m9mF836ZveiWVd0L5pzRefCmVd0vvrA5d2v3X/Z"
        "updvv6zz9Xu/2fLqXd/pXDL3r21vPI4WAn0GLuQHQtHzDl0SDj3ikHciAETIQ0uRfm9PlS3dEdSIfIepTNZHtChpVT1PArcEWNDzsI13wqmc4hSAMMzILjhy"
        "KEDyBhgEgEIYLseY354L2610bScbxS0D4PqpyskkCQiCRo9vMc9wMggIIUBQIaoKpUJhUWDbrfhqq5eikBr5RdXGz+xANPCRHFWYbFNjGEhU5nZCtM01IYRe"
        "Xscx2jgGqZtCIUfxvGsDMHldGkXpMcEZEFwoUeFS6chdr6ooqHoE3EwRVAWXAIGDwQYGehcKIxAiXUJQLxgJIbhoANBQoI4AQLpEmERNA2Xqg+AulrkQxW8V"
        "nl1EgPNAocxdNu8QRAWsuNFll2xUS4hbYOwTy12cE0YiCr5TAtQB8qQCEai4wYht+H57Lk4ZEcgrQRAmqF/ZFkcl6QEBDsA5rnd4tiJf7MJId3bfaAMM++SE"
        "EGDYu6HhSoQrE6CgvEwbxGImhG6OOd1dRYAZHObPD8op4EID2xSknGXF3lTm+44UGCe8TJgTiilF1SjliPaFOkJFBR5y2Xfz3lIGIQERokFifUx7ywkhQAgB"
        "ToAGAu+wPWEGNlxv3Uijp6VkhZSzcqIJLmyg9OPHWW6x/tbUxAhhk5iqpAq5HEg6FFdzAKSLE4+jhyF7LZdhhkAImsrAjCeOwS9tB8DHBEOlRR56uDMwoYQe"
        "Gcd1WK7IqgIUsfBjWn96r1PZVCytx/v7VglMnQJXEAiiKhA9UurtJRJYCuWEAfJICBohukgEsA7XADBSjrbgc6jp1whBsQRKGEI6YqBHUQAdF1SToWyIDyrx"
        "QEU5qYSCRhXQiAoa00BTKFjFbqDCgVSCQVQPwCt1QDLKoF9VHLhdyqBti15+elOCOg181/cDF6TqtUgE1+oQUFmgx2JAsGLoI2PluYEP23iNtjNoU4L3Vpe6"
        "J4QAIaRcVH4GKYjy4y69SUvfqQyQgHMUAMWIchbg+15ZMCgxAF3BVSGPLjfwVIQFO5wxwT9UEhAI0UZDNNIQDQwERWMQfkzRNtTZEj+McHRuQqSw3sZETyqQ"
        "HpelnARCsB7tb4lIH+Xo9FNCsBlGQjDdUIciVQBkX7g+LuEbyj8+06/FOzReW39FZ6YTFDwd7mlBQZ6DAGCKUUpFYArAwUREcYt5aG3vhIBEzmo4ommjM5qe"
        "1hvfC5mOXBy9BAc/P6uoT1Q2KDhBi/iVjjAlsXHdHZnXIVaNM7teowRA+OAENm69OI5BbPD6Al3FPbjA2c8AqAKkPCcJprQncgGKIFDKZ56NKPBCQlde1IW3"
        "aGB1Ykn/qug7QxrSywY1JD/A9IPBDdUYa9cMbmhYPaRf46qh/futHdq/f9cBe4/KHnLgGG/owFp/xJB6e79Rg7J7Da5rNbn9ngbOHJRBj7FgpvciSiDFJggh"
        "CG5q2bsKERQVwwDpPQW4l9Q0TVYn8ratccmSMQEh0mXqaUGBACGbdo/zMgCBC21PlV12pzu7Z6FQSjDIfgUBQBWAdPU5+isqbhmEwBz3UFpZWeXTjB+hJTdp"
        "VIIIISB5kZORS+PEGAKWCRFCiSEvH2m6SQHSoBTrb/BdkB4gKMlKghOc3gSRCbFLFmxrHL1YYZTFheRPRqTfQwt7IgTK5WEQ6iGj20pyKH7qDkX07I6unOHi"
        "Qa80SkolAIcAOAkB+wCkLdPyRMB3pWIeUukqCNBWhRadRo3aY2ArIRGLW3H8xO67AVC1p6KklUjFQDUSff4KRk+tT/nOjJEhofWGoUGpmAMFP4wI9I6546zr"
        "7SmStQI/8LsD9NbR6lAEAqSMARclglqX+kxE9RXCsa6g3P+Ck209W+H5053i6pODUuuUfNuyyX7X2mO97pZj/FzL0V5Xy9FhZ9uxvLsFy9qPLnW3frarbdUR"
        "nWtWHhzmWg908u2HWF1rDrdalx+lFFqP3YOsuaOXl41TrlAGPKCKggu1/GqKa4/80QsuVwwEefnrMnaphLMHxMbtPi4/Gm2qtw4hBFVNyo+EkA15IFhEPt7m"
        "sdYOvbbZqD8tLuQUJYRIcAICDJjcw4cAAq3Yt13w5E+hRqOaznuWBNiBAc+QytRpWb/yThBEZEoBeQSCs19oNi6n5WpbuZVnNSAGldv1VERaiHacID2kgx4J"
        "7ynftnvcdBKapjWWjRGBSUiD2aipJMm5CAlDq92ofGtZT00dGq1umO5w5Anp+bjlCpE/CdBMIUDwEBxECJRRkHMAlQLRaBTy+TwAHtra+LXN4eyyxmPO2WNL"
        "/Vhd7e2Mh63pZBQ8G0BFupQSaG3rBo+NSresAAAQAElEQVSq42fct7i81G+p/adV7oN6OMFJXHIdSMg/2oc2xjh02CWnpbePJfObi4Fnr+ChC0IEaAVYCTAi"
        "gMk66E4BD70qHoZd+dfufi/31sz317zQvGzNC7OWLX/mgffWLJjz3vJn7313+TN3v/f+/DuXLX/29neXPnfr0refue29pf+8demyp/625P3H73hr8ZxbX391"
        "1u1vvPnwbW8ueeyexW89ee/bbz//4Kr5W9iKEc/npmEGbskCXddBLtgl3HZz9JSQUSiVShCJRgTySDBu8yU9Jg5CwdmGbT5qjtLmKVowvtzlF93pHPhUCrQs"
        "lRAIbuVCYEYChINPkQSuqjHI5kuBSSPejuYtHk8zuTYQyZGMskOCIkFvhyFv6AkJFf1tWbz12AMbUrEAsj0BXi4iIMsIAjAham8PsC1BU5VaTTdGcYkQCBYC"
        "wUkCn2wraUopipBz4klYkaVbj8NxC8biNRd05myQ33KogiNEABI4Xkkvaih4EGa9DIHzSMRQQSCYSNAKcQsBmI9XVYGD5yycaAcFnjYJeyMYP3IxsDqo57RI"
        "9EEnBQLPwS1xCGY8CXq6Yco60xj0kUafcsHFs5f2J7HUafmiBwEQcH0OqtCh2J1/fWg6s8Fjkt0ycN8G6gMOHgQJAYhUE0bMC9wCqixMKkppGOzE4LLAKxQK"
        "XHpMXvmHU0NQTBOiiSjgAgcRwwS7ZMk5JLaHLekxMcF9aVPSnjZvK+0ACKKyCMnm73b2M86indylygkKBY9HOEgBUT0KfsECMGO4GPQAlWlEaF53peB3KHNO"
        "3kb44DjvBADqA3oDznqpJEqIwn08Oe0t30LKAYcEKEqyPgpMsa5YX4adoD0gOmHZtl6cK0N13RiMSzagYNY3o2g3rCcvQhA8YFy6/T0lW713572jVDN1mo9f"
        "HwSe7Pf6WTKlFCCCuMm4/Ws1KPwoqsIKdHTQSeLgo40a6NUWurqA6DpkMxmoqu83Y+hnT+3z97xuPP3YnOqV5nLXBgXpauj4EoVBvuhA1gn2KBmpE7fK6Kfw"
        "Mq8mpjpMG6nhlpIJBRiCkqkYIFx3wY0XXYQo9GEnyONCogCUVSdFS0jZ8wUMqDcoyi/GjI3Hx098DZ5wSmrcFv7ESV9EDaGrQnChaQbqm4DUDyEErM5OwAK0"
        "Kg6UIXLC9oUlS5r9kEsDEEC4KDcWoiclZAMWESGFVn67625oOrukc4UQAhQlzv0AQMX1NcDVCvOaqoPwg1AE/gZJwQ4KTCFApUfAPWAMrRL5gdAHCDDixA9D"
        "34VtCEwzOcfDU4rGH+LZDSgqyHEpigJS8a7rEaprA7aBVLlKzZ6HxRHUjs/jFkrKCNB2CCFlHjkaUih5BgASCnADjoKDrYZB46b1S6Qavrp6bTuEchYKApSx"
        "Mm9ASZmuX8z83QztJ/3Xm18qZdtuSCciAEBBjsWVusEZjBfIidHa3tXgQuJzsIVgt6x5OKmyvEqgfHCrajoA9iM9FxFJfvX8uatGb6Hpf1x8/sxlA6Gm4ZIC"
        "ykz+qEKIjkVa9i9Epqur/f7NO8Bzpzfr66ocETggt024WgKgXBW0wxDlJICCohgX1B94wlj4BGH4+Cn7m0ry3naW+dZeE87o83fjNifr8FKAtkOlt8Q5LuAE"
        "UOx4U1VgFMpbOdQefIIgTNPwpP3IsQq09bLdIyFKKQ6dg+CIWFThWLRLLxzmzu1fCEQDVH1vrwQ4EMEBEAhQKuU8TmaHFSNY2Ftrx6SMBJQSgaZHsF+ByheA"
        "GQAIQbrxqCzGmVCwYKsXfhYvUvQKXAcPVZgKhKFYceIHAQd8gM7uLLiOvx9MmPCxtACDUdV4mBFPnukhUFMgWIJ00F44GinKBqQxEUIAj4qCCKXINFbZyuVy"
        "fgrRo0fo6JX6GTwvMgzgjoNnFww05LsmnQA70/ZQ7s1H8PM1qsIpzmEQvKYimEji2DVo6C352AYwUATeQDW/OnLi9L3w8SPXHktnvRoB717508UGgoJlofck"
        "QRrpdTvOMJqo/d702a9I5PtI2/+kYPpfX1G9ZOrbrTl7ZMgIeHguVp00weouAnEKb2kd8Nbm9FM17gruewsVRQMHvUFAawDC8HzMAyAaAj8BLRIfmLP9zw8Y"
        "P96E7QiNB00cuK7gXtJthRPBTP4kY9M/DTzsjCkfZwc6V/FTsBCcALJCgUh+uruBoQ4IIZCIx8BzbKka2N6Qz+ZUTVMgDDzUv4p2LjZESUsI7ExmdnGkO73/"
        "MJB9osgBqOCg4IKv8gDzAbrcASgiAMoDxnSH72jePNfyUM8bKQa7JKhvIYAgl4RRkymRgR/HRzwaLxg42anKQGEETRvbIwGBno0s9xGgzFj8uNp89GO3BP0P"
        "njo+ZwdftUpONUhKyIv0mACDkHlMAW2HUGQQ81yh6OphZgtX/4NO2a924JBL17a1AdIEEosBHuxBTX0D+DkEKfQW7WLX7XFdfaGXRPGtR94OrO5bq1NRoNgP"
        "UxX0fFyQX7gILiTy/85zfGhoLfin9zXJZsyYwYstq25KR9RQAQIqUwDFgjhVAhd1XQByBo8P/cKMGev3vL0d/wdpEx6ql4bVfiVSVf0VuSCgyIGiy5bFc6bq"
        "ZBS0XNuN82d89M+tLGlu9nzLuqsaJztwAppq4Jh1IGoMxa6BoCYUHAFV9YO+011MXxD/mL+K2juEmn2O39MS8UtqBo06PycUaHdDyBFtsojWPFzt9P9hYsyU"
        "PrfCsr0Dig9C5gDl7oEpz5fq6yF0PRAoPwcXCLSr9TV66m3TvamJ6YaGXpNZpovgBhtsCgnIPCEiJAGuzPi8Ky+60zsndINACXbOiACKcmAiBEo4AAIV54Gj"
        "dhfwASts5fpPX8UiRgdjDAS67NK9JQJZQ35w3pdJo53WFVz/hOTeZ6WhfEYwo095uaHbQUF0S6MJPK9sQDgQ0AwNSjmchoqKExL28oV6df3+xx3Tf3PjnjBB"
        "GTC+aXjygJNOd4g5I1bdcCLaMUgBCPSSAK2USmEhV9J4MClfmPec/FblRDyhTc7b/kim4IRDgBGhB7hcQue6dWAkE1CfjlthIfePtjceby8TXX8z/Pxc4lkL"
        "5d+TljJCISGe+aAj+OqaCfmSC9F0zfQB2phx65tsknBKXldL2YdN8IHhZBK+B5qmAWUMXKbSNk/58bvj117UdN992iYNP8HDeX/7p1HVr/4yHq3/2er2AlHQ"
        "O9NRXsQP8aCYABX5Z9S25bNgC4E4hTksDBfFo+jnyQN+xweBh2+4bkKIB+cB1SFvh1A/YNgfA8X4UfVB045Jjz5mDxg8wUCS2BPe8ZJ/fLB63xP7p8dNO8E3"
        "qn+oxeqvXLWmHRxA/VMVHIIAlStByCLfj6Xq/lq7BXAKuatxQqXWQVU1sG0bLMsCgrKTdqoxCtz3N/SLXW/b1dwcIvAIH8//CCHlL65oQ6jaHq9JEhECrV8r"
        "72rk4y6LfU60ncANIkBvL3L6cRAoZllYToWgiqLLx95KOyQNPHelgqDBmFqmL5WE9gBoycCFABu3ArFk3SWeFl4DxbYv1xzyzuWpMUdOKVfe6KZR3uLZxVc1"
        "SoEgnJimBgIno/xj9GokgrQAoVcBrkaOBT3156Ju/kTZ6/jL4/ufeoE+evJFqUL1VSVhXBev6n+HFbCJXfkSEKoCpQr2wss0AemiUQEnktZ6QyKEGckklmC1"
        "Pq7UwVP30ZI1Z3fkLQgpID2sFPigImBq+GWH4HmYne36Bwn8j/yNpNUL573v5NtuSUQ0kD/Ux3Cyh+iGUKIgHYr8MVjX1tU/Y/vTkCrBuMnVfPoYTy+s+1GK"
        "uKtTOgVT1cBzA3DwvMrlAiepVk1r+/9GH3jYVWc98tygTRpvx8OlzyzpRwYN+4GjpK4sukrEDpAVQtHn8CDBAojGwkK+c8Xv7vr6ZHQPoc/Q/fLDq51M6x+T"
        "+DUyGTdwbBxUXQOQpIwIfgAAkH/lc01HDmi0+ivMqLrDoolfGsmaK2DvE89T9z/li+aBp3ytvcS/44L5x2TVwPtKHj2zJLHDTAIABfySgAnqU1HBcl2ghjY2"
        "BB87wdebXYzqHiEMSwmoeK4U2kgVvSbpgVO0MWmnaF8EK2znNYMqlHEJbmUaaOMfIUA4BYKG/5EXO7eA7tzuNvRWBh15kxONE4rAJKMCIeYDNI2VkVq+ofYO"
        "yriB9w7FyU8wUoqeE/Yj0HPDBELAyYOTKG+5NcJIX6Km666DSNWvQ5Y4bfCE8koJvUHlbXkS+v/SqAADFxu7kAeC7XUEJd/1gSkGFD0BAdEhZ4sRPotcHK8b"
        "dG1oJm+JVff/S6hErimFZHJb1tE4eiMeZ8BU00cX3lHRMAn50AYJQcpoUNKwCAbHlhLs5WSjFN12x9emucBGCYr2Twj48lwBJ1/o+wgSLtRX13heKfdA79nS"
        "Rq3LWR1Kj5DAfUZHIAuDAIxYDOQ2wi6VgCkaaDhZjHj84rpjv3houcFmt1unjX096Fz+LSMoQTGfAy0Sx6OWJHTk0IvEid+SsSM5Pf090W/0/Rf9a/Xkb9/3"
        "ipzFm1Hp+/HKRxY3fOnR905vV+rv7FAS3+10oNrxKRiRGDrdIUQVBhHicK9z5W8M/t7svql8WBoLwodK2dZbPacACkXIcIr4koOgHOn5oFdVgS9lCAp0l/wG"
        "I1F/BjGrfpysHXKrz2I3U736eqInrw6YfkprZ3dEi0Rx62wDRSACwYCYcQAEJM8pQToVgVyu87ruxfP6/G+vAtc3cW4QIfsLBbCICQ62VXUVFBwXD30wI+aH"
        "RoGcbts1gzNNJRp6royR8iH6R9rJAYfrJ8FHXu68Arrzuuq7p5AoOGEZrujK+qhCQFUO5nLRd4tPr5Qp0VV4vvyCQB4IxdUMwQRdHcAvNQAUAQDhRUmkwAkB"
        "fK5B1kYjVSK1hUwMz38+5KNl4cKS79lPxQytNfQdMNCAVGwvv6oAYwBonGheUPIBDTQGoRaHTCnAZwpdBRc4M3HMBgKHADOawtU6AiXbvSOXy/4GwUkoFCDE"
        "8yrAQCkFQggIBEE8kKMADpZ+9Eq/Lw5t3GPoBd15G2w8m/A8FxQCOLwAhyYgEYtDe+u6Oxi1t/j3olsWPvlBMdtxX1UqAYD9eugxcdziyEknD0+9YlGeVSQK"
        "lt00eMJ5xke5ANBy3TOTKn8wmUyjBwqQKTpQ2z8NxVIRCFNwi6Qo6zLKgXlSd08wZNxdlzzVed70R97f+yv/XNHw+bnvJZruW22e988VxnQ8LJ/+6Mp+X36y"
        "5YgLnl53bldVw52Fuv43rAXzqKKehIIgKE8fQhdZFRRsvwgqL91Vv67rd82nb/S/sfTFJJZ9gAf/NLB/VZXQnlXVEBRFABJAgXkApgpudwcQTQEPPWE5/jxu"
        "r5yQQC4fIkhXgQTFeDKJY0JijEPglUA3deAOtgcKAj8AmNE4xLAs29lye+AVbsea2AneN7sYaB6hsCHIhUQ+cNzWe9hveVGCjWvIt9sWS4Wis7VvdwAAEABJ"
        "REFULulIO4rgwvmRVgRXPUbYR8p3csFGw985PWuCEybwIAm748BATliOwIDmBDjjgADID2MCFg7lsIND4Z2nujzXvouisVEisDeM0gFBvVCMjDEI0FPA2QeA"
        "KxVhFLSI0T8SVdNYeZMrEcksEkHhhnREBwgdwKogcKskK4W46kk7Uo0oTk6KQKwCuhsATAMFtwq2E8hqoOg6FHLdoCnkZSh134h7wNejukYoggJHoxSCACEM"
        "5DMDhjtOIgzjo3gg/6oiNcym9u7MAILj0tGzYbhKCkrK45FAF1GJE1XF/ZmFT+bKnW/hphHyYCnX8UwMjZijx6WYEZBfC5miAegGFLIWxNP9Pu8yOKgvEred"
        "f6QTti/7RoL498RVCoaqQHdnCaiKMlAU8DwKoMYg56uJlUU4oUutvtlKDZ2VTQx+xG8cfj8bO+CuTHzQ3XzYuDutqkH3Z6J1d+X0qhtaPHZ0SzGscqgJGYdD"
        "SJA2enbyTCzBBFRrfD4rtH7n1i8eXuiLr77KOt94dGk+0/KdmALP6fKLuWsDSB2ihwnxJADKHoCC/GsFBBcbyhgQlEOIugk9H/IlrI915DNQimNzANC2dBw3"
        "RS+MuwVImfAM8Pyv7TfmrYEtBGEi4oFgjGBzbE9RTpQqyAqHSDwBHGeNL41hC+23VqxoBE06gAC7sFwOnKIe0JaAYmdIGYgv8OUOn3tb41G+o/K2MyMVNGA8"
        "EIygoFE4nKBgvBAUFAxFwze4D7rwGUBzuDP4ionu+0Kn634VPARE1IcfAgkZ5hW0KQGqEAAMI7r4LLAhoir1MQ33C5sxt3L+fIcUO/5ugHNPTCfYJASVAYDw"
        "QdMUNHCBXhcBYAaEBJ/R2AlBkJK/7MoUHHMIhvCgIcre47nWn0Nd10IWlHwmQpQVqkkQAKxPgIIIBOATsJCHuPzJLGwclGjiSMWMn1WUWy4RgPAskCCLzg5W"
        "o6BQBkGh42+i29rin8iF9aFz4YPrWCn39wQNABeVnlI0Zh9FBQJ1p9ZAZ5HWFIVx7vBJ59f2VNj0fvOpn1kjVr73pdqgcEs1E8WoqiE4K4AHKTgkgRMYvUaU"
        "WQd6GW2uyzq9cPCaTusAPBo7trtTTM3n/Kmr1vlT23PeoVkbBtq+GvFcHfVkgo9fzEyVAiECGPUgofmQCLvuS2bXfeEfJx/QsiknH/9UeG3ec16h/auK59xS"
        "nUx6Co4VOCoywLYCU1BAAjxTCQJFCQiUAIQNKp7FhWgrdiiAGjFwUUeMMcAjHSBhERJGAEqQvdNat+IS7435H/mxBaS+4Yr7IlSJwIXHRfkASDoS+AiCU4CC"
        "F7QsPzSiDU22OaMxQjiCHWc6Eo4CcEwVAzhFXokPJvOxxN1mejuqIt1RhLdEN3SLGrctUCEEOV8pTlymUeCuhecCHFT8ahRlqP0tEfiUy1sXzetIqN5PI9R/"
        "Oh0zQMVtGNoESA8FEAR83K5EDBPMCAKK74GhKbXZfBaXz48ysm7R06sot67RhX9HVVxHQCjhbssHL98FFCcPQICAFwANQzRYAOHbEE3EgCN4aPg+YSivWN2d"
        "37WV5XPkn3uJRRTVD1wi5JafEBCCY90QO+ZotBxEGBqayjbR4V4HT622/HCa5fjVmmbgCARwPPD2bAe444IZTcivMa22lZ/VuXRWAYl97BWF8FEWWE8a4AMJ"
        "HKQZgPQAEH0BcAy8hGQC/kU07iO2ROyOc/e1tOK7X0+V2q5KhVZrHLdKcmtp4IJkKBQKVhGIrkKA3kGIiB4QFT0TBC1OQK7uAVGAokdU9AKw8BDdQH0I3Icb"
        "uJhF0WOJihLEwqIfddtvheKar9w4ZcwHW+Ll48qzi+a8HhRXf98vtF+pC3tB0iAgzw/Bd3GRYSA/GgRoFyoaMMfxa2i/BHVDCANdNxFoA9DQo1LQs4nrDCIK"
        "/8Dpbr+G+7nvZN566g34mBAyoeg0RMmEENhFFHER4vEoAG40KHo1BspJ1xHJ8Wvux5Da5PW4ceNUivqLoHxFIQ9AAABtC00L5OKlMZRtdh0zkWF8s0svurN7"
        "V5lia+C/W5MweEOVGfSviQYDayMwqC4O1REFatNGqNOgdWfy1fHyzNfcfOtX862rfoN8tRo6A1Q8GBET9HQ1lFwfbDwjUM0oKIbpadG4AlsIK56btVRxs992"
        "Cx2XDmxILmqsMqC+ygQDbNC5DRovgS5KEFc4pEyGn6MtqKpKZP3Q/rMT2pfk3njgAVi40Jfkfd9yTFNtTaViflV1rJRKx+xkMlpKp2K5dFXCiSfjrX6oh7Ju"
        "byzpbL+6+sGHxGIJJxKJWTU1tX59fYNXXV3tDh4x3E9EI0HbmpY79Vh6i2dLvbR609WLZrV4Vvbm2oTWUhfX3JoIcRtTqt8YBz6kmofDq6nfP6EJ4vFj95l4"
        "Ds6g3pabpjeedGDptmMHXt9gtVwYK7U9WsU8n6E3p4IPsYgG3PeBKQS6MnkQTAEXPRALywJVgRIe+NoI6NJp8YULAeZV8KCa+VAd5qEhzL2WLq27MFy97Ku3"
        "Tzuka9Oet//JWjK/Nf/yP/5gis5zc61LL9f8rplVhujQuctrESSSqRQuMASi0RR4dgC+E0IMFzCOW7oUbp1rU3ErythSUrL+WMp0fsHR1/3Ufv3ptdvCiWGk"
        "HPRq8smo7jbUpe1+dVWWTvwi2ouF2++iSYM8t7PtMH8C3xZ6vXUWol3RsJSrjlB7jwE1+cakYjekdacxpTnVUWo1JAy7IRVd43e3+71tdlW604GprRS04KT9"
        "cq5jzaTcupUndK97Z1Km9b2J3S3Ljw7s9gmF1g8+6zrt39/ZAim8+fg7QZ31Xavrg1MUN/d17mRv5nbuwbCUfVwVwROJhDlHZ/zGzrZ1F8U18srW+Fu18LF1"
        "bQseuM7Pt5xe7Pigyc+0XhMh9t0mLz6SUP1HqZeZx7zs/WGh88Y4c77h5T84UfXdq7qfu2uTz/ZVuvd8vu3ds5zuVSeGudWnlLpXnex0rZjqZVc2+dlVJ9nZ"
        "Vd83UtlNQFzhznu5jpWXlDpWn0LtrmmFjg9OKHauPlEU26d0rHrzhGLru8enzfD67gV35bc2hs3f+ao3z862fk6UMidBvu1kXmw7URRaTiB26xQ3u+q00Gqb"
        "YhJxxxuP31HavO3mzzecOPqRYaT7tEhm5WUDovSJGlP1hJWDKA1B2CVoqEqgyxyid8lBR1DScdukoKUK7kNEB4ioAgi3oC6hFOuI80Yk2/pNvXPlaTefNPZ2"
        "eaa1eX//yXPnS3PfhXf/+buaoOt8Ueg4Ocx3Xmx1t/2y0NF6i8qD+9xCfnZUpfN0Gs6lnvVATAn/Uupc963cupXnB/nVJ2TzmSuct+bM711stoWXNS+MycaI"
        "95Ow2Dat1LnqtFL3iql+dt1Ur3vtaazUNY1YnadFqXsTwAy+LfQ2rpPQyb1WdvUUUWw9vdj2wVS7c/XJ2VVvT4Fi65RS+5rJmQ9WfqP7/ZdXb9xmV+RR3Tu5"
        "22Xz3FUvPfRC9+szH89htBY98mTulVlP5N6Y/fSaFx76V8tbjz/f8sZzm/w/YTuNw/nzg/zChxfkX7rn+gjNfLNW8b5UFw0urFeK55t+8YtRP3dp14v33P7W"
        "U/e0bQtP8s9jdC+ac3/3qw/OiBltX4zZhXO10Dk7ycS5EZa/KGmySzuevfsPmRcffE7+P2Sb01zzwmPdnQse+Wf3ApTVgoeecF956AlrIeZRXl0vP/xkxyvz"
        "Fq3Es62N2y1/bs4HbS899HT+1fse637+74/nXml+QkYEosctzFuL7n8K+/pg4zbbku9eMC/fsnDOsy0Lmp9of/Xhx1qeuffxdS889OjyZx6Yu/rFR2a998/m"
        "R956/G/PIy2B8WOv3xy3r3XH8Xv+Saxbdrq7bsWXaii/uZryhf3jhh1nQpjoIVXhVsnELT/zPKiLa1Cl0SBG/e6aKHmBFLv+VPjg3QvFuqWTbjt51G9vPOOw"
        "9z+20/+gwsrX5me7F815obR4zk3Wovu+ywn7StQNLoiJ3LmM5s/WzeDsbMeaL2bypW+4bz/06+KbDzXn3pr3PqC9b3+3M/iKF2a+1PHKnLn51+bMLc+PRQ8/"
        "aS1+7NF21H3LgllPrH1pzuvbTxfgvWcfWd728mNPrn7hvsfyrz/wWG7h/Y9bb819ouvVeU+3vvbo/PZ3n/1EdD8JL1trs/OBaWvc7Ebv5ERc80Jzd8sz/1iN"
        "6dq25+9ox9RGFrdp4mG9Ta6VCCBrFj/Wvfalh7paFs7ulKCznt4m9f7XHm6bun/2/lP2vP3uiQ1f8lYuOtFa8uJE572FpwyA3PeMrvd+2uB2/LTWbrlGrF4y"
        "w1/xyhlG+3snZl5/7tS4/co35py+3723nnn4dh9wfwoyFrCk2Wt74w4r+9rMbB7tJPfs3RlYjl84sfxToP8/T4L+z0ugIoDdRgLN5x/Z+tAXj3h21jkHzbp1"
        "YuPPHjhl5PfvOK7x+/84YciMmaeOuWbuOYc9cM+0A1547KLj1zVvw88m7TYDqzCy3RKoANN2i6zSoCKBigR2tAQqwLSjJVyhX5FARQLbLYEdDEzbzU+lQUUC"
        "FQlUJAAVYKoYQUUCFQnsdhKoANNup5IKQxUJVCRQAaaKDexuEqjwU5FAZStXsYGKBCoS2P0k8P+lxzS+qckcP77J/Dhxj276Smx00wzt4+pV3lckUJHAzpXA"
        "LgGm4ZMm6YMPPWW/of/3uYNGYpT5vT578t77T2qq/STD3/fIU8fsd+xZ+5XbzphBu4q1Z7t19Wfgs/z9aUw+eh0y5cJ66oofuH5hb/n2sAu+FT+46cKjJky7"
        "cJv/myXZbidEMurQyYP2PvyE9GZ9kb0mnDJ4n4lT62CzsNfRU6uHT5g2AFAWm73aZY/DDp1aV3/oCWP7o95lHDzupL1GTjipBj7lMGD8cVXVBx1/EHzMb943"
        "HHjcwY0HHr/np9z9JuSGH3rKsKGHTjto6KGfO2ggpmOPPedAaefjjmlKblKx8vARCWwvMH2EwCcpcDuVffR0/79ZNDbDjtR8t8Ci38uS+BUdvnH5gCPPOGS7"
        "aOLkCyLpaX6keurwSZfoAxbndDCjk0tAj5lw3nn6lmj5IVRF4okvOYINARCky2L9WKL+h0Ulss+W2uyK8oGHTenH6wd9v5OmJw9ouszsBZtRJ1403NHrf9fu"
        "VV259wlf3gBa0gsMWezzRDHO22/+awnYhjB4wnmpcSed+amDxMZdd3n8846v/8in7CtdxdL0vJm8bJ2lX9E4/rQ+/yzvxm23J2873v7M0M5PuHrv2EnNYVMa"
        "N6Yx/JBJCRJLTacR4/CNyz/N/FAEnxzo3/C05HUWNa/0zNqrsjT2XRGvvdpV9P23oy8y8sRz+zeedFJkO9r811fdJcDkgZ7wiDIsELTZsq1fmUbkj0RRnwiE"
        "PiZeO+jHwyaeP3B7JOupsdmuGZ257JBqnztJwpkaBkQB10pvyWMinY5NSgF1FM0IJ4SUEisAABAASURBVEy4hjmm3t6ad37iqVVvjJs+XZ2BgAcbhaamJoaP"
        "W6KHr3bMxVRayllhjEdS43CZlcDEx42brlou+Yweqz4hmm44IePABnk5dlAVUmOCx2I21+Lb8ucrSED0o7os/bRDJn2+dzJvPpj/aNySX0WPDxRUs7jv35OM"
        "x27PFQqPhpSlSpReM/DQqZ/oP5PcnEn5bFSlXvOpdk8+lyjK56oJF/a3RPTymiOm9ZPPMi5bMC8fqrG/5V32jHzeEbHkgsaiVSOzXvCmK/gNHNzfEuFdZxD+"
        "B5Mq72xrn8MnnV+TKWlfjmiD0bubIefrf6SLbe13V9eTA935PJBQ/lmdvML9hd3zb3txzRM3/WvdU3+7U/Gs+zRmfoZoyfJ26pBplww4dNpXDpJAsTGT+575"
        "1THDT71k/17vIVTjISNGiM/lPwMhANSQML4umsQsgDxH2vfUy8YMPf7CyUMmf/X0QSd8fYKa7F9vITI6XiGYX7tYxE2dmHrE0dVIsLCxMWxeDKm9z7x6/D6f"
        "nzFgr6bvjl8ZOeCMPU/9dtN+Z12537iTpkewL4o8kdFNM2L7TfvmQWMnX3z6nsd84fRRE887ZciRZ0wZM+n8o8ZNO3vDZMC6G659Jp4THTft0gOGH3n+yXv+"
        "3zlNY4+/cMLIk6b36bEcVKsWdB68qvNwuN1d7KkzFCJaGOwbFHLPEhF4VFNGjps+XZXgGTWTQ/RkzR5Fob1fYqlgPHpZyMvoIUd9fuLQ4y+a1jjxkpOGn3Ll"
        "sAkTZiiAYFt/zGVjtXTDWUKvPq5DVB2z/9RLx0uvS45PjnPI8Rfus+fki6bItoMnTj/kkM/P2ABe+x1/3mCcOKMbJ5xZM+Swk4/Y84jJJ48+7Jg9YLNgD80Q"
        "L/BpNKJ3GFR/se25B18MX3/4oSqN31oTi48llB3Q26QRPYPa8afuX/OZU09KH3Li5NSBk/avnzgx2vtepo0TTqoZ+H+nHVF38EmnNB5+8kl7Hdk0Dr0+Q77z"
        "wqQa0CSH08cE6WOmJ7M2TI4k6k+zQ/PouqPPO3bQEdP7yXG7YdyGRH9oRF02HNV08OAJTQ2y/cZxr8827T180ueGrS8j/Y65cFTkwFMmDTz2gimDJ3/1kD0O"
        "P2uDp7q+zoYkkUoLTg0FFOWDbKP9TMdTt/179eO3PPP6nFuffXnuba1SX0NP+spBe065IL6hEWZQX1V7n3DhuH3QRgajJ+vx9KR4tObcri5n4ohpxaNqj71o"
        "uPScxzRdMnrPieegt4+N1l8TzzknOn7KufsfcuaF9aObmrRDz7mi7uCpF+2LNPuPb7rk4H2mXnzGqCkXfRZtVhuH9rL/lK81DjvuK0cOOfqi0/Y86esnDzj2"
        "vBHShtaT26WJnFw7nQFFAVBUqjKNYe7D7jXNFAGHDhZPWbK02wmOhWj693oH3WAAcrtWZNEvuLG6S2rbITJgcUInqvJFoqjnTpCTDeTGjClAWVjdBVQqQIvS"
        "fS2q/URLNV6KXsQpXI9/viNrNyl6RDMY49DcHPqWW8up9ucu2z1wXEs/5uvaGNWsvr1o86+EQr+wM1ucxJlxic9i17sNg48avRiUcdNnmJyyiaGW/Lkwk5Mc"
        "ahzoR9KXaNUDb86E9LScBX0CkxJL7Y1ezhU02fA5YaZOEkr0B5ZFL9rvlEtTyP4mVzPyphJYGo2Z1dQ0BsuXgSXqGxpq91MV9kxgl96J6Pp4r1gVW1RsUIqu"
        "t7dlu048nWxNDnR5h6805kPtfLNm4IWYTnSV6JfyRP/x22lvr3pjkEHjscPUSGxSMQz3Dpg6LR+SqRpL1OzxclvS1hLTIql+P3JY/GRPi58Zrxvws5Wt65rW"
        "gxOxhHmoy6PfDll0OkvWXCKMxFSuGP0ljxtHr1gkiVhcYcBZQgfW+87h7ho0BItrEXQGAWonNMV8K31BLlQuE1pyMlWSxxvJ6m+JonLRfhNOKctmyNEX1rte"
        "4hpHr77Q0pJHOKF+AmjRi0IvW+43JPSwRLrmV40LC1U8ICPUiDFR1Y1BjhdO8FxxnBdhwxP5RFIxte9yQo8pKKoJkeqr8y58buNJOWziOXUWi81od3R5tECM"
        "Iy86KojU/kitGzGtC1LHdpbU74SRuovHHPfFqt7xbJxm2wssIMwNFBNgzJjyArnxeysf6Z+o7fdnVYuM6y0f3dSkZRxyKE0M/LUTHzAQUrWDEbSnokUPNBV2"
        "uOXax0ciVSPAjlRxNXG5b0Y/N+G8D/8TiJacPgzidb/Xo/UTStEodX0YosYbftaRdy7x1MRVSqzuq2qs6mCmOlErU79XXk1+myaqr2LVtac7LPq1eLrxB2+4"
        "9duzzexl/VNP6adOcRsIMgIMYw1hkaMGnvAtXH2uOHnYCVdeUgy1Y2xOb1r6bsvbgKs5xMxqV2UDixFVA4CyC5scWM2yXYWamJGogw6AZAIQHGgjJzCkvRao"
        "hAKsSPGZ+akoccLGeHeu8EUhaJoT8iMaiX1XEfR6TdNU13VTnNHyRBE+GIyqe1CFpuzMOiJcx3BLmeHCze1jstJNdTH1OyYvXaXyoEN1g/NLYFVZeahTNP9i"
        "z8+2cbs4I5aM/KIE4mZb0ywSS73gJGrfhj5CKZd7LxSFXytx7Tssol2dB/YASaZPLoIiD/CR/U0bMROWcoVmqamPlSu80M2hoa5UhUS8nEzEXqqvq5nALauf"
        "HlOiEVPbWyHBqoSirll4440+DXkuna6+J3T41cmI+lOThddGFbIX8e0JereiMSqeau1a87hqsmfi6eivdV37YzSS7qqqqx/lcn5xe6brSS8U1+gR/fLW7s4n"
        "1GT8ioxX2gs5FC4JY8mY0RS6zoBCLvNbCMPvRROkz7/n44eeIMCYZ2obbM7XasaTaDLuoncHGMJATEqnqy5urEo+Faf8l9U6+yXxneZEqubUPBeTsQpkHOeQ"
        "SE3/yQWP/C1aVf2raG3djwIQ1+lJBDmsoOkkFvjWoFCxaK2pvUl49hGAUmu/fuk7avolfplOdC5IKZpgLOinKKK64NCiQszF8WTD8a/mjGokUb5UbhwRqvpw"
        "O4BlqeMvHmQa5pUdnW2LXRL8nIvg10Yy0kwixtltgdHnGVmkX70bKrrpkHjCeMkf2H/atwYMx3jImd+onzTpEt0q8JhTKAwNi6V0uUO8mWn0slQt6jFtWEAV"
        "1Y6L94ji38eEtVZVvX/ozP21GtOf1cyAhYFo0LVo1Upsh1ePzUTqY6GRGN2es5LVmhaGAGbedg+LpqqOD8OwWRHu12LMv51pBpp3YYpp6vuEof9L1/Eu9bjz"
        "XU540g3tC/ZBUEaau/Siu6J3QSklTKFA6IlGJHZRJF13MVW1b9T1axxQcqzlcFJjiF4MR8ly18d1JzQ58ikwgpbJiX7pKuhqWevvke7HbeaFPAxcLkB4DVUk"
        "HpiMKColVGGuyik40JBKRP9PcH/mu3uS51fc/f1VIsmWRqLGLF1XS8QJuHRtcUVlApTQ46G3BEbjPHaprrNAY/zBN+791YsL7pqxxtSd14UXPMtD0WiqiTjV"
        "aZQx1lB07eVvz/3TqsXNv+sOArqEgsI9t0THwxoP+gjvPHV7V81A9hYvepYXCu76YWvImAh1pRarl8eJ6YbLpLzND8J1XkhG1lU31rlE3cfjpDNelVxe8pw3"
        "PM/vB3p0lM9pHRewZ8QwllAVihLc3519becb/L3XQ51121aRKKGf861Mq6rQeqo6gupBZzxpFlWVdVvFwqolzb/8oG3dGi/XXZgQCqLpZuQtrUr3Mt02hBAu"
        "jMdi/XG8Y6Xn6nMXAsey4yab2fbcgwvefbp57cLZs0sbGF+fSbYP5Lbnh1SL7OkVxBl7HHXeOdXHTr9KiyWmZ3L5B4y48dpo9Bb0aHxirqvzTcfKzF05/7aV"
        "7z59+9pGq2p2yQ4X2RA9AfDcL1ZV7/uUaaCoQz3PNVm+2L3ssb8vXjZvniu7Cx2bxA3TBcuCZfPQua2qWmcoNOS+27qs+fqOJc3NHg88oqsq4rcQgHXCUuER"
        "oij9PG6UvYXheNbWWXKPVozEi/FY8p2SG45TmDoiGUs/ZxItMGJV1C3mVjie31Ii5DPjxk1XZd8bR8fNqb5rBYzSI0NBv+NC6hcWNX7l6OlfLI/pjbFYTEtE"
        "TJaKo0e1vmFutU6prlGHcy63DG3OKsdUWGddTQLiOl27YtbNbcvumpEXruPqjIaEMohYgxXZvKnpPuYwlfsBb9PQ9Bfe+NcAB81TqRQON5gfYzD3pTt/+frz"
        "d/ym3bO6E/G4eXSukFvsClL+o4FRM5rJF4qLq6uqD4eoVidp7spId0XnjJlEVXQwNPpXoMUriNf2zZjpntPVvWZBPGb+cPjSULrPgnHBkqpGFAhYL596NCMC"
        "K8eSBmV2Zl0Qs1lAOPCAc2/Am91hQbHDkKKuMJIg1FQS1EVoOChBwxVo2BLgYOVg8NBjWKMRwApAlzTP8BlVA9WM6kxjMLz4byWeTFBPhD4If2Vv30G0OsCJ"
        "nxHU0BiLKdlSWLCs4N1IpHb/IVO/ve9APLtJR5OHphm+9b0VzbgN6227IUVPUJ7bFNrMr/HQvUZh7Aca8HNSpjnczRf71EcqO7gY0423FEU7JJMp7h8INs5x"
        "xAt+GLSmqqqX5n1YY3FlrEujo4gWS9l+sMympXA0jDFHnfr9Ef0LDefYucw3aqurrmTC/XJdOn54KdulWKoXBAXDdQu2ApyjCPQyKEZq9lBwRR6RiCbHlAr2"
        "RVZ74RrTUL5PuHIW+CFBvWhOTKE6VUEloiMaOu8DQLktpn1eZiTGfFBGhnrsSGHEJoVU7xcK8g9NkJ+vnvXHltUd6UQ8Vr1vRFPfb/13M/rCPWQWLrzR9zxl"
        "ZbxhyMGp+dmEZRee9337b3ENmqKa8eswlvrKqFO/PqKnNkAyUU1JIEyc0FSWecUijxkRPcqM8rMsU8w4V4WioLbLdtUYoQv1CFvq6uqxEnCLSmqgWtNwdMby"
        "5nXPu74QMaIDSaAMpUI51y7537ezXd9BCzsP7aqfLhxtTbWtSbobR0MXvhLYYZwX10aD4jsKdxapCnu9O5N7GcHH0QgTdj4fEba7oa0EUu6XhI5CVWXpmDFC"
        "MI84JYuGwsMue3qIJ9MC/ABC11YHNQCdcN4MfTEsYQhe6MTRWp2KEICAytGCXcdSCXnNjnUXYX1QQqdG0/WxQNnxBOgPiGb+vlAMf5aI1xwPQkV20R5g1wa6"
        "K7qnmqoouu4xXV3+3r2/emdx87VLFt3zmxdUTdxDGW9E1/IwaSA0DCBwPWEoRtDLZ61l+a4T6rqqhktgcdjtMAQbHycWePPn/zCMByYTqHRKBPHsXOC4lsCX"
        "OJdKWG89lRkzeDJp+IK7WIA6xElFDCZKrlPKlWwuv+65HhdhKBxBVR8rrb9afEVjVNeVsBjYgkKyA7Hzbl03RnGm/VRQ5VLHc47M57O/7V9T++b6RpskY+yq"
        "sSFTfsUF2Y8AvIjL2X3c95/M2tCXAAAQAElEQVQUjtueMA22SeX1D/Pnzwj8kvOWoRlcCPIZRpVaHgSL3rz7F9l8Vz7LIJzvus4+pXzhkND12nTGlrJCxBVh"
        "odoH+xupdOocTTPWFQv5RwLfm9XZ2b4okkzyZJFRorhmVSKuaIQQ1RXrZdQGDITiWfbSqlj8KVNRHlFFOA+xaJZj5S9IKuYTa5p/ZwcBdkFYd6CZZSGuZ7fP"
        "JOSEuaF40gXxQ0GDK1Mmu7p7zu9ua3n8z+W/L92YCPCV8P3gw0Wol5AARQ04KUbTJss8eWNOp6WfxZnzA+aUFuqqdjIP2I3jTj1Pbi8hdEP0JUCJ4T5Nto8l"
        "Ykro+0Rj+gY9xiMuCQKfxnVNlXUWzr6xFDj2XC0SPyZjsKF5oRyqRsxcVIXXAG1Dd0uQjpod3LP/HdXIo3XxyLwY4/N4qfvqtBo0tx06zMZ6m1y80yM1aSMR"
        "BfvVYd3Wn/vXtf5haKu49jPamr+Odd9r90NHRJKJUgAQbtyQMaah7HUd7UsupApOCiMS1WtSNaK3nu5YVNE1HRdPJcwWw/m3zXBxcfUEJ7oQQgm5kOMShqYR"
        "EKEDFPyhmWN4b3ss83L5bBaX8+eEbz+uEz5TZe7s0Cv8ppTtvESLahsW4w1tdnJmwyqyM/v1PBF4vq8Bznzsd4PAceumEmABEZxKcDA1zQLFYOhKS0GDDItg"
        "eFWktv/IQskDud1bMybvakQoDDw0PCLCqOmhN+RQwXGlDANF4RkR+t0kFINkexnHTZ+ulvL5/ioLNQhKsgiI43OVAY0bOocZP8T1VAdDi1Lmc8SPchUYmmkk"
        "Gi7HPLAilJTCNc2XOaGwHBHY7zLPeURn7JFQ4T8mscRtz440cj2tPrzLfjn4h6f0yJ6Uh39aOuu3d74367onVCVYgj5YGHjBhr4+bNWTMzVYme9u7zRVdqTj"
        "WBzX/3fxjVg661cFmm973uR2Q1wLDgvsjtcCJ7dGGqpKnAaNKkcHQXD3ijm///P7c//8aFTTFwqiMjsgWqBX01g69ITvCh74NCYi5Umyp7PKMRhboavgq0Hw"
        "4uq5v3t45exrZ66a87uHVsy6rvmtmb9+H/sGXBwgjKRN9NSEfN5aDANBKKVFEqota+b8Ze3Kmb/PYv0N7ZbOurXAvdISP2R7bPK1a8YMqkcjwzXga02niDoG"
        "aEEgWT77ppc/ePzGX/DAvUIx9D1yudIkpAeodt9AtAqYHsjn0HM0UzcinIcbZOsV44GhqsACtDhZCWNMaK8ooCtFT5/AQT0i39n1SOujvy9P0PqIv4b4GTfK"
        "gre6Hr32wdWzf/nw6sdvmLnm6dsfWv7obS9LAEESm1zxKO4KAg40LHDp9cnzvvnzZwTSi5Yx4NwpUTUsKarR23ACfrwJfBhBXEfXoCcYEAU3UJW8T5SeEoCA"
        "RSlEdNyri1iXyWS5ABCkGJD+nlCqgPVMFw/Hp+kmQ7DaMHZJIxpPZDSFrjCoaGsgxfvfa/7x3WvQFpc9dO1db8/569zXZt4mdSOr7rJId0XPhPoc5P9wC3To"
        "UHTDR0z7ztDhUy/7P6DG2dz3rURMf0kqWwj3faaptd1eMHHfc3/Sf8+m7w8RijHZ9sMxChEO1iGjF4MChNpMcEWeFTFrjcYgRL9GgGrGRDQabbM88aieqDp7"
        "n9MuO+KgMy4fWOwyRwlKTws5142YoUgZKFrIFEYY7s3UCTOuYb7jM8KFEU/Gy+8Bw2JYDAgKoCgEPSeFjW66Ro0Y0ZFoTKOZqgnbLWlaNDbQoeGovZZ3pLHJ"
        "JtfQTIaTQASEoJ0wtWaPE76cRs9wmKbpR7quVacwIZpwq7dJo/UParG9Pa7Bu+BbB+ngvksdr339K9AQ2HScZXGdHg5e6f13Z9/YKd/pisIJEcJx7DQevPYf"
        "dvLlA3Ohf2wsldqLaTrzwoJS6IiElEJ7Mh7bN68V997vlEtTzbiFUE06Nxozq6ghpu176mVj9m26rP+4z10ybNy0rx4wGs+DJH0QISl4HqWJ1IercfnFpjc5"
        "MTUKAQWPqHGcrZu+3vDk5nL364nE8CAamyR/BEF+8er3TOvJumEc5BSzd+FWJz9kyoX1Q44/e89xTdOT406aHrFC6gjCcooRLYMWUEEIo44GIZOEo8lkVwCc"
        "4Aqy715Tv1st5Rv6NuEElxCOi5ushNEKgmVWPvskCcPTU9HYqNooexSLy1cUii/EI+oaqogL9jj5otFjkYeRJ57bf/+pF+x78NRzq8uVNrsVLFswEfimJvqU"
        "jZYwrKLtrPV08//2O+vKESPP/GbNiqr8EbquT4zqGlO9Ht58ouWYkQxCoR+4/xe/N+iY6b9I0gR4jusv1yLxoywbPjPu67/YY+8vX3sACDFZUTVww7Dcp6Iq"
        "4Ds2JdwVzc2nh70svjA0uY569tPpCJvIdXXK6Kbpe+wz9eK6faZ+cex+J541orferkx3DTAh+AROYaXgoglE1bcdrn8vFLHLEvHafW0rf80od+m/pVASlLya"
        "K7T/Pd1Qfa4TelcHTFyOMj9ABPZDul9c27R4MRkDi0M/DDpCHkgvwXd5xKeBszb0vdaFN/7QXjBA68gE2k1FmmwtaenLsyR1uc+S04ElcoHQFvgByci+CM5f"
        "AP5aXU1dZj5mCA1KQMI30Hi65XsZ68aM4YZh4gci8QFRmJtVIFHymBowUw+oeraqxS63s+6VzIVfFvLq9LH4BUa2641ypQx954Wc7TzvEXN6CMlv+pRfYIeO"
        "H4TO7FA4HbJOb/2N0wXz7sp7+bbnTWa/HWGl5xfioXbv+2rHWCV8/99h4L8Fgi3uLXc9d5XjePcTphzr8eAr+aL1VVU1R7m+mOnjuVQkEfdaZs8oOaE3mysK"
        "NiWXWKr2pXFLoaqUtxfhLvHnRKGHMEO92rKL30HL/j7mL9XdeI3sw/Hd7oihriNeHldsWbLlqHrF1czJfNBiZNwt1Yo3+P/MZYt/42ri+FxgfnOdp32DR2Kn"
        "50rW7fXV7BHZLqbHhtbVDfpeayG8YpUL3wJD/ZoVhIvdwH9CvheamrdJsESYxJfPhVB9t8TFv0jMOFeN0SsWulX718YUn+r62lAN22QdGZfhIXhMdx+pS+oN"
        "YbH7dTzM2vBDkC/MaV67tr3th7Gq+vowMK7M+PqVAmqv9IPYF62sW/6ZO0lj49i/X72Ty+cX4zLUunF5b35U8d0Wg/E/KErkoC4n/KbHjSsj8fSphaK1wPec"
        "RVSn5TMhDuy90IjNgWjyNGDxH7Q7zrE5vargBbxZpWpB1c2rit35q4KSf2nEiFj5XO4FBLayt66QsBhR+NJqg21YxMr9z5jB+8XYfZrwngpA+4Jm9rvaprEr"
        "tWj9t/VYv8+U6+zi2y4BJoPZS6mf/wItZG5Wg9z9ZlB6IMKt63im62ur5/zhtt7J+e/m6ztihH/Pz7b8nNpdz2l+YZ7idf8hoRR+kDL9O2U9GaPEvTUqnDtR"
        "lmLlbTOcRFi4LslLNwEQgV4VX3X/T5/387nLfad0O3cLrwsn97DGi9eqgf2lfkwtnwURX1lVallxGTgdC2WbOlO8zp22SxvV0gpYHyRgeWHpedfK/8wuWVkS"
        "eIcIHh7th6UfFu3Cdzj4V0cN/Zqg5D0ZjVZ93i15e69vuiFZ+tQ9b6iufbWXa79VFdZihINHqmPqLbzU/fOY7r22oWIfGU3NPUadwgXUtx/b+PW8ede7KZ3f"
        "ZIb+BVHCN9B446G/tGtu6Qadl35BS5nXayP0XxScv4hM54+TWvDAMm+xJemsfOC6x93utkv0wJmpBNZruWxXQU7UfvHuu1lQ+laQ73g4rvgv8UJmTrFj7W8X"
        "zbq1RbZLR2P/DrvXfTu5yt0wwWV5X1GF3L0G+A/j9hvxra8aAEuam71cQ+GmQnvrtaYIX+BBYVFgd/7WMIM/L7jr+rxspS0vvZJrX3dDUCy8FDfYYhZY9/v5"
        "tqtWPHpn+b/8iqje86HV+c03R9aXJ+eKe37Wplili1XPut3LtC2sjWutL+D5GAty36PCmStp9kbma8+IzJqLaqn9u4V4ltVbLtOWp//+uLCKX49p9F5u24u4"
        "a/0LfP9m16jbAGCyXm+UfZiK9ycC1uO9ZRun0m6Jq90e9UuXxYQ73wysF3S/6+aGBPuVCsVLU3ul1sj6b9793YyXWfY9zS/+nHn556p0WLbs+q+779/9o/mG"
        "nTnP8Io3xdXgrYTKb6dWx8/iIrgkAsGLsm3otr7rFlq/o2VbF8jnjePjd/xxhVLK/9gg8As7n38icKyX8tncbYW8XQb4jevuivwuAaZ358/ufHPu359Z+tQt"
        "jy6d9btH353z67mLZ/32n0sfvU4al9hYEAvuua7tzXv/OO/tB/90x9sP/GHuspnXv//6vX969/nmmzZMwLfu/f3bbzSX25abvvLQX95a+MAfNzGY9x78xfJV"
        "D/7iofcf/PVteE7yxNsP/n7dm82/e3P+PdeWtz1Lmmd4y2Zdv+id239e/l9cX7hlRvdr//jD808231g28DLhGTP4on/8seWNf/xmgZaxwrSu7qvSfOhkWmd2"
        "zLn22dX3//zfy/5x9b8C6r+hUaaYSvTDb8FlAj239566dXnLP2+atXLu9fe8/8ifn3+9+S9r33/szmWLmv/W0VOj7/vixx7rfnXevS8umvfhV6vemi/OvG3l"
        "y7NvevmNx+8og01v+duP3bhu6aw//3Pl43+7d8nsG+Ytn3PjByuevmXVigevXbURSOBZ1Q1vLH7w+vvffuhPT0hQku3n33ab8/oDf1r8xuy/3bNo5m13vDH3"
        "zgeWPNH8odxn3dy24rFbX1+IX85k/a3FFc/PXbX8mQfe21qd8rvm5tB66c7XO5/7+6zsv++c2fXUbS+34JlS+R3eZF/vPHrrC23P3jd7xawbmltm/emJlsf/"
        "Vj5Ax9fw9oM3rls265ZFcnGRzzK+M/P3K5c+/Of7l865+X4EuDWy7O17f//q2w/eskrme6Mc9+rH//7Mktk3bfA6e9/JdOns36xY+tDvHl03/9a73n/iLw+9"
        "9egNb8g28l1fcdlTzUvefXrO2r7eyTJpcwvv++0/337oT/94+4Hfz3zt7t+/tuCuX61Bm39j/owZgawj4xtY9urff3zfK7f97Nb5N83olb947tafvvLy7b+5"
        "85W//eqPC27+3hOv/eMXKxfc/YuF82/7ddlLe6G52X7mgVsWzUNvW9LZPM6feVv25ebf/WvpzN/dt+KRG5rfnfOnp5bM/XO57eZ1d/bzLgGmnT3IHdGfFqt2"
        "sus++EBXlYFMDU/uf/y540ec/NVDhk795lTu2id5xdyrrue8tSP6rtCsSOC/TwLbx3EFmLZPXhtqy9WuPqI+mul2/mhD/NCApL7kC/UL2Wx2Yiqi4SF17pdL"
        "Z/9xwzZwQ8NKpiKBigQ+VgIVYPpYEW25wsLZN3aui6+7JeZ53+NW58+tbPvvEiKcYbe2/PHdOX/t81cztkyt8qYigYoEeiVQAaZeSw5Y6AAAASFJREFUSXzS"
        "FM9E1jz9l7Udz9z9nowr59/WuuaFZvuTkqu0q0igIgGACjBVrAAAKkKoSGD3kkAFmHYvfVS4qUigIgGUQAWYUAiVqyKBigR2LwlUgGn30keFm4oE/lsksEP5"
        "rADTDhVvhXhFAhUJfBIJVIDpk0it0qYigYoEdqgEKsC0Q8VbIV6RQEUCn0QCFWD6JFLb/dtUOKxI4L9aAhVg+q9WX4X5igT+/5RABZj+/9RrZVQVCfxXS6AC"
        "TP/V6qswX5HApyeB3YlSBZh2J21UeKlIoCKBsgQqwFQWQ+VWkUBFAruTBCrAtDtpo8JLRQIVCZQlUAGmshh2/1uFw4oE/pckUAGm/yVtV8ZakcB/iQQqwPRf"
        "oqgKmxUJ/C9J4P8BAAD//xMEzwcAAAAGSURBVAMARPJ14ggYDV0AAAAASUVORK5CYII=";

    // Decode base64 and send as actual PNG binary
    // For ESP32, we'll send the data URI that can be used directly in HTML
    // The HTML img tag will use: <img src="/logo">

    // Send actual decoded PNG binary data
    size_t base64_len = strlen(logo_base64);
    size_t output_len = 0;

    // Calculate decoded size (base64 is ~4/3 of binary size)
    size_t decoded_size = (base64_len * 3) / 4;
    uint8_t *decoded_data = (uint8_t *)malloc(decoded_size);

    if (decoded_data == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }

    // Simple base64 decode
    static const uint8_t base64_decode_table[256] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
        64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
        64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
    };

    size_t i = 0, j = 0;
    uint32_t sextet_a, sextet_b, sextet_c, sextet_d, triple;

    while (i < base64_len) {
        sextet_a = logo_base64[i] == '=' ? 0 & i++ : base64_decode_table[(uint8_t)logo_base64[i++]];
        sextet_b = logo_base64[i] == '=' ? 0 & i++ : base64_decode_table[(uint8_t)logo_base64[i++]];
        sextet_c = logo_base64[i] == '=' ? 0 & i++ : base64_decode_table[(uint8_t)logo_base64[i++]];
        sextet_d = logo_base64[i] == '=' ? 0 & i++ : base64_decode_table[(uint8_t)logo_base64[i++]];

        triple = (sextet_a << 18) + (sextet_b << 12) + (sextet_c << 6) + sextet_d;

        if (j < decoded_size) decoded_data[j++] = (triple >> 16) & 0xFF;
        if (j < decoded_size) decoded_data[j++] = (triple >> 8) & 0xFF;
        if (j < decoded_size) decoded_data[j++] = triple & 0xFF;
    }

    // Adjust for padding
    output_len = j;
    if (base64_len > 0 && logo_base64[base64_len - 1] == '=') output_len--;
    if (base64_len > 1 && logo_base64[base64_len - 2] == '=') output_len--;

    httpd_resp_set_type(req, "image/png");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    httpd_resp_send(req, (const char *)decoded_data, output_len);

    free(decoded_data);
    return ESP_OK;
}

// Favicon handler - improved with actual favicon support
static esp_err_t favicon_handler(httpd_req_t *req)
{
    // Simple ICO favicon (16x16 blue square with 'M' for Modbus)
    static const unsigned char favicon_ico[] = {
        0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x10, 0x10, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x68, 0x05,
        0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x20, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x7B, 0xFF, 0x00
    };
    
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000");
    httpd_resp_send(req, (const char*)favicon_ico, sizeof(favicon_ico));
    return ESP_OK;
}

// Configuration page HTML
static esp_err_t config_page_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=UTF-8");

    // Generate escaped values for HTML safety
    char escaped_ssid[64], escaped_password[128];
    html_escape(escaped_ssid, g_system_config.wifi_ssid, sizeof(escaped_ssid));
    html_escape(escaped_password, g_system_config.wifi_password, sizeof(escaped_password));

    // Send HTML header
    httpd_resp_sendstr_chunk(req, html_header);
    
    // Send main form HTML in chunks
    char chunk[5120];  // Increased buffer size to accommodate larger HTML content
    
    // Overview Section with Real-time System Resources
    httpd_resp_sendstr_chunk(req,
        "<div id='overview' class='section active'>"
        "<h2 class='section-title'><i>📊</i>System Overview</h2>");

    // Modbus and Azure status cards at the top for immediate visibility
    httpd_resp_sendstr_chunk(req,
        "<div class='sensor-card'>"
        "<h3>Modbus Communication</h3>"
        "<p><strong>Total Reads:</strong> <span id='ov_modbus_total_reads'>Loading...</span></p>"
        "<p><strong>Successful:</strong> <span id='ov_modbus_success'>Loading...</span></p>"
        "<p><strong>Failed:</strong> <span id='ov_modbus_failed'>Loading...</span></p>"
        "<p><strong>Success Rate:</strong> <span id='ov_modbus_success_rate'>Loading...</span></p>"
        "<p><strong>CRC Errors:</strong> <span id='ov_modbus_crc_errors'>Loading...</span></p>"
        "<p><strong>Timeout Errors:</strong> <span id='ov_modbus_timeout_errors'>Loading...</span></p>"
        "</div>"

        "<div class='sensor-card'>"
        "<h3>Azure IoT Hub</h3>"
        "<p><strong>Connection:</strong> <span id='ov_azure_connection'>Loading...</span></p>"
        "<p><strong>Uptime:</strong> <span id='ov_azure_uptime'>Loading...</span></p>"
        "<p><strong>Messages Sent:</strong> <span id='ov_azure_messages'>Loading...</span></p>"
        "<p><strong>Last Telemetry:</strong> <span id='ov_azure_last_telemetry'>Loading...</span></p>"
        "<p><strong>Reconnects:</strong> <span id='ov_azure_reconnects'>Loading...</span></p>"
        "<p><strong>Device ID:</strong> <span id='ov_azure_device_id'>Loading...</span></p>"
        "</div>"

        "<div class='sensor-card'>"
        "<h3>System Status</h3>"
        "<p><strong>Firmware:</strong> <span>v1.1.0-final</span></p>"
        "<p><strong>MAC Address:</strong> <span id='mac_address'>Loading...</span></p>"
        "<p><strong>Uptime:</strong> <span id='uptime'>Loading...</span></p>"
        "<p><strong>Flash Memory:</strong> <span id='flash_total'>Loading...</span></p>"
        "<p><strong>Active Tasks:</strong> <span id='tasks'>Loading...</span></p>"
        "</div>"

        "<div class='sensor-card'>"
        "<h3>Memory Usage</h3>"
        "<p><strong>Heap Usage:</strong> <span id='heap_usage'>Loading...</span></p>"
        "<p><strong>Free Heap:</strong> <span id='heap'>Loading...</span></p>"
        "<p><strong>Internal RAM:</strong> <span id='internal_heap'>Loading...</span></p>"
        "<p><strong>SPIRAM:</strong> <span id='spiram_heap'>Loading...</span></p>"
        "<p><strong>Largest Block:</strong> <span id='largest_block'>Loading...</span></p>"
        "</div>"

        "<div class='sensor-card'>"
        "<h3>Storage Partitions</h3>"
        "<p><strong>App Partition:</strong> <span id='app_partition'>Loading...</span></p>"
        "<p><strong>NVS Partition:</strong> <span id='nvs_partition'>Loading...</span></p>"
        "</div>");

    // Network Status - show WiFi or SIM based on network mode
    snprintf(chunk, sizeof(chunk),
        "<div class='sensor-card' id='wifi-network-status' style='display:%s'>"
        "<h3>WiFi Network Status</h3>"
        "<p><strong>WiFi Status:</strong> <span id='wifi_status'>Loading...</span></p>"
        "<p><strong>WiFi RSSI:</strong> <span id='rssi'>Loading...</span></p>"
        "<p><strong>SSID:</strong> <span id='ssid'>Loading...</span></p>"
        "</div>"

        "<div class='sensor-card' id='sim-network-status' style='display:%s'>"
        "<h3>SIM Network Status</h3>"
        "<p><strong>SIM Status:</strong> <span id='sim_status'>Loading...</span></p>"
        "<p><strong>Signal Quality:</strong> <span id='sim_signal'>Loading...</span></p>"
        "<p><strong>Network:</strong> <span id='sim_network'>Loading...</span></p>"
        "<p><strong>IP Address:</strong> <span id='sim_ip'>Loading...</span></p>"
        "</div>",
        g_system_config.network_mode == 0 ? "block" : "none",
        g_system_config.network_mode == 1 ? "block" : "none");
    httpd_resp_sendstr_chunk(req, chunk);

    httpd_resp_sendstr_chunk(req,
        "<div class='sensor-card'>"
        "<h3>Industrial Configuration</h3>");

    snprintf(chunk, sizeof(chunk),
        "<p><strong>Sensors Configured:</strong> <span>%d</span></p>"
        "<p><strong>RS485 Interface:</strong> <span>GPIO16(RX), GPIO17(TX), GPIO4(RTS)</span></p>"
        "<p><strong>Modbus Protocol:</strong> <span>RTU over RS485</span></p>"
        "<p><strong>Supported Baud Rates:</strong> <span>2400-230400 bps</span></p>"
        "<p><strong>Supported Data Types:</strong> <span>UINT16, INT16, UINT32, INT32, FLOAT32 (All byte orders)</span></p>"
        "</div>",
        g_system_config.sensor_count);
    httpd_resp_sendstr_chunk(req, chunk);

    httpd_resp_sendstr_chunk(req, "</div>");

    // Network Mode Selection Section (main WiFi config section)
    snprintf(chunk, sizeof(chunk),
        "<div id='wifi' class='section'>"
        "<h2 class='section-title'><i>🌐</i>Network Configuration</h2>"
        "<div class='sensor-card' style='padding:25px'>"
        "<h3 style='text-align:center;margin-top:0;margin-bottom:20px;color:#007bff;font-size:20px'>Network Connectivity Mode</h3>"
        "<p style='color:#666;margin-bottom:25px;text-align:center;font-size:14px'>Choose your network connectivity method</p>"
        "<form id='network_mode_form' onsubmit='return saveNetworkMode(event)'>"
        "<div style='display:flex;justify-content:center;gap:20px;margin-bottom:25px'>"
        "<label style='display:flex;align-items:center;cursor:pointer;padding:15px 25px;border:2px solid #e0e0e0;border-radius:8px;background:#f8f9fa;transition:all 0.3s'>"
        "<input type='radio' name='network_mode' value='0' id='mode_wifi' %s onchange='toggleNetworkMode()' style='margin-right:10px;width:18px;height:18px;cursor:pointer'>"
        "<span style='font-weight:600;font-size:15px'>WiFi</span>"
        "</label>"
        "<label style='display:flex;align-items:center;cursor:pointer;padding:15px 25px;border:2px solid #e0e0e0;border-radius:8px;background:#f8f9fa;transition:all 0.3s'>"
        "<input type='radio' name='network_mode' value='1' id='mode_sim' %s onchange='toggleNetworkMode()' style='margin-right:10px;width:18px;height:18px;cursor:pointer'>"
        "<span style='font-weight:600;font-size:15px'>SIM Module (4G)</span>"
        "</label>"
        "</div>"
        "<div id='network_mode_result' style='display:none;padding:12px;margin:15px 0;border-radius:6px'></div>"
        "<div style='margin-top:25px;padding:20px;background:#f8f9fa;border-radius:8px;text-align:center'>"
        "<button type='submit' style='background:#28a745;color:white;padding:12px 30px;border:none;border-radius:6px;font-weight:bold;font-size:16px;cursor:pointer'>Save Network Mode</button>"
        "</div>"
        "</form>"
        "</div>",  // Close sensor-card only, NOT the main id='wifi' section
        g_system_config.network_mode == 0 ? "checked" : "",
        g_system_config.network_mode == 1 ? "checked" : "");
    httpd_resp_sendstr_chunk(req, chunk);

    // WiFi Configuration Section with Enhanced Layout (wrapped for conditional display)
    snprintf(chunk, sizeof(chunk),
        "<div id='wifi_panel' style='display:%s'>"
        "<div>"
        "<h2 class='section-title'><i>📡</i>WiFi Settings</h2>"
        "<form method='POST' action='/save_config'>"
        "<div class='sensor-card' style='padding:25px'>"
        "<h3 style='text-align:center;margin-top:0;margin-bottom:20px;color:#007bff;font-size:20px'>WiFi Network Configuration</h3>"
        "<div style='text-align:center;margin-bottom:20px'>"
        "<button type='button' class='scan-button' onclick='scanWiFi()' style='background:linear-gradient(135deg,#38b2ac,#48bb78);color:white;padding:12px 25px;border:none;border-radius:6px;font-weight:bold;cursor:pointer;font-size:15px;box-shadow:0 2px 8px rgba(56,178,172,0.3)'>📡 Scan WiFi Networks</button>"
        "</div>"
        "<div id='scan-status' style='color:#666;font-size:13px;margin-bottom:10px;text-align:center'></div>"
        "<div id='networks' style='display:none;border:1px solid #e0e0e0;border-radius:8px;margin-bottom:20px;background:#f8f9fa;max-height:200px;overflow-y:auto'></div>"
        "<div style='display:grid;grid-template-columns:120px 1fr;gap:20px;align-items:start;margin-bottom:20px'>"
        "<label style='font-weight:600;padding-top:10px'>Network:</label>"
        "<input type='text' id='wifi_ssid' name='wifi_ssid' value='%s' required style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<label style='font-weight:600;padding-top:10px'>Password:</label>"
        "<div style='position:relative'>"
        "<input type='password' id='wifi_password' name='wifi_password' value='%s' style='width:100%%;padding:10px;padding-right:60px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<span onclick='togglePassword()' style='position:absolute;right:10px;top:50%%;transform:translateY(-50%%);cursor:pointer;color:#007bff;font-size:12px;font-weight:600;user-select:none;padding:4px 8px;background:#f0f8ff;border-radius:4px'>SHOW</span>"
        "</div>"
        "</div>"
        "<div style='background:#e8f4f8;padding:var(--space-sm);border-radius:var(--radius-sm);margin-top:var(--space-md);border-left:4px solid #17a2b8'>"
        "<small style='color:#0c5460'><strong>💡 Tip:</strong> Click on any scanned network to auto-fill the SSID field.</small>"
        "</div>"
        "<div style='margin-top:25px;padding-top:20px;border-top:1px solid #e0e0e0;text-align:center'>"
        "<button type='submit' style='background:#28a745;color:white;padding:12px 30px;border:none;border-radius:6px;font-weight:600;cursor:pointer;font-size:16px;box-shadow:0 2px 4px rgba(0,0,0,0.1)'>Save WiFi Settings</button>"
        "<p style='color:#666;font-size:12px;margin-top:10px'>This saves WiFi settings only. Azure and sensors are configured separately.</p>"
        "</div>"
        "</div>",
        g_system_config.network_mode == 0 ? "block" : "none",
        escaped_ssid, escaped_password);
    httpd_resp_sendstr_chunk(req, chunk);

    // Close WiFi form section
    snprintf(chunk, sizeof(chunk),
        "</form>"
        "</div>"
        "</div>");  // Close wifi section and wifi_panel wrapper
    httpd_resp_sendstr_chunk(req, chunk);

    // SIM Configuration Panel (shown when SIM mode selected)
    snprintf(chunk, sizeof(chunk),
        "<div id='sim_panel' style='display:%s'>"
        "<div>"
        "<h2 class='section-title'><i>📱</i>SIM Module Configuration (A7670C)</h2>"
        "<form onsubmit='return saveSIMConfig()'>"
        "<div class='sensor-card' style='padding:25px'>"
        "<h3 style='text-align:center;margin-top:0;margin-bottom:20px;color:#007bff;font-size:20px'>Cellular Network Settings</h3>"
        "<p style='color:#666;margin-bottom:25px;text-align:center;font-size:14px'>Configure 4G cellular connectivity</p>"
        "<div style='display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-bottom:20px'>"
        "<label style='font-weight:600;padding-top:10px'>APN (Access Point Name):</label>"
        "<div>"
        "<input type='text' id='sim_apn' name='sim_apn' value='%s' placeholder='airteliot' maxlength='63' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>Default: airteliot (Airtel) | Use jionet for Jio SIM</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>APN Username:</label>"
        "<div>"
        "<input type='text' id='sim_apn_user' name='sim_apn_user' value='%s' placeholder='username' maxlength='63' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>Leave blank if not required by carrier</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>APN Password:</label>"
        "<div>"
        "<input type='password' id='sim_apn_pass' name='sim_apn_pass' value='%s' maxlength='63' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>Leave blank if not required by carrier</small>"
        "</div>"
        "</div>"
        "</div>"
        "<div class='sensor-card' style='padding:25px'>"
        "<h3 style='text-align:center;margin-bottom:20px;color:#17a2b8'>Hardware Configuration</h3>"
        "<div style='display:grid;grid-template-columns:120px 1fr;gap:20px;align-items:start;margin-bottom:20px'>"
        "<label style='font-weight:600;padding-top:10px'>UART Port:</label>"
        "<div>"
        "<select id='sim_uart' name='sim_uart' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px;background:white'>"
        "<option value='1' %s>UART1</option>"
        "<option value='2' %s>UART2</option>"
        "</select>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>TX Pin:</label>"
        "<div>"
        "<input type='number' id='sim_tx_pin' name='sim_tx_pin' value='%d' min='0' max='39' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>GPIO for UART TX</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>RX Pin:</label>"
        "<div>"
        "<input type='number' id='sim_rx_pin' name='sim_rx_pin' value='%d' min='0' max='39' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>GPIO for UART RX</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>Power Pin:</label>"
        "<div>"
        "<input type='number' id='sim_pwr_pin' name='sim_pwr_pin' value='%d' min='0' max='39' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>GPIO for module power</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>Reset Pin:</label>"
        "<div>"
        "<input type='number' id='sim_reset_pin' name='sim_reset_pin' value='%d' min='-1' max='39' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>-1 to disable</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>Baud Rate:</label>"
        "<select id='sim_baud' name='sim_baud' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px;background:white'>"
        "<option value='9600' %s>9600</option>"
        "<option value='19200' %s>19200</option>"
        "<option value='38400' %s>38400</option>"
        "<option value='57600' %s>57600</option>"
        "<option value='115200' %s>115200</option>"
        "</select>"
        "</div>"
        "<div style='text-align:center;margin-top:25px'>"
        "<button type='button' onclick='testSIMConnection()' style='background:#17a2b8;color:white;padding:12px 25px;border:none;border-radius:6px;font-weight:bold;cursor:pointer;font-size:15px'>Test SIM Connection</button>"
        "</div>"
        "<div id='sim_test_result' style='margin-top:15px;padding:10px;border-radius:6px;display:none'></div>"
        "<div style='margin-top:25px;padding:20px;background:#f8f9fa;border-radius:8px;text-align:center'>"
        "<button type='submit' style='background:#28a745;color:white;padding:12px 30px;border:none;border-radius:6px;font-weight:bold;font-size:16px;cursor:pointer'>Save SIM Configuration</button>"
        "<p style='color:#666;font-size:12px;margin:10px 0 0 0'>This saves SIM module settings.</p>"
        "</div>"
        "<div id='sim_save_result' style='margin-top:10px;padding:10px;border-radius:4px;display:none;border:1px solid'></div>"
        "</div>"
        "</form>"
        "</div>"
        "</div>",
        g_system_config.network_mode == 1 ? "block" : "none",
        g_system_config.sim_config.apn,
        g_system_config.sim_config.apn_user,
        g_system_config.sim_config.apn_pass,
        g_system_config.sim_config.uart_num == 1 ? "selected" : "",
        g_system_config.sim_config.uart_num == 2 ? "selected" : "",
        g_system_config.sim_config.uart_tx_pin,
        g_system_config.sim_config.uart_rx_pin,
        g_system_config.sim_config.pwr_pin,
        g_system_config.sim_config.reset_pin,
        g_system_config.sim_config.uart_baud_rate == 9600 ? "selected" : "",
        g_system_config.sim_config.uart_baud_rate == 19200 ? "selected" : "",
        g_system_config.sim_config.uart_baud_rate == 38400 ? "selected" : "",
        g_system_config.sim_config.uart_baud_rate == 57600 ? "selected" : "",
        g_system_config.sim_config.uart_baud_rate == 115200 ? "selected" : "");
    httpd_resp_sendstr_chunk(req, chunk);

    // SD Card Configuration Panel
    snprintf(chunk, sizeof(chunk),
        "<div id='sd_panel'>"
        "<h2 class='section-title'><i>💾</i>SD Card Configuration</h2>"
        "<form onsubmit='return saveSDConfig()'>"
        "<div class='sensor-card' style='padding:25px'>"
        "<h3 style='text-align:center;margin-top:0;margin-bottom:20px;color:#007bff;font-size:20px'>Offline Message Caching</h3>"
        "<p style='color:#666;margin-bottom:25px;text-align:center;font-size:14px'>Enable offline message caching during network outages</p>"
        "<div style='display:flex;align-items:center;justify-content:center;padding:15px;background:#f0f8ff;border-radius:8px;margin-bottom:15px'>"
        "<label for='sd_enabled' style='display:flex;align-items:center;justify-content:center;font-weight:600;font-size:16px;cursor:pointer;margin:0'>"
        "<input type='checkbox' id='sd_enabled' name='sd_enabled' value='1' %s onchange='toggleSDOptions()' style='margin-right:10px;width:18px;height:18px;cursor:pointer'>"
        "Enable SD Card</label>"
        "</div>"
        "<div id='sd_options' style='display:%s;margin-top:15px'>"
        "<div style='display:flex;align-items:center;justify-content:center;padding:15px;background:#e8f4f8;border-radius:8px'>"
        "<label for='sd_cache_on_failure' style='display:flex;align-items:center;justify-content:center;font-size:15px;cursor:pointer;margin:0'>"
        "<input type='checkbox' id='sd_cache_on_failure' name='sd_cache_on_failure' value='1' %s style='margin-right:10px;width:18px;height:18px;cursor:pointer'>"
        "Cache Messages When Network Unavailable</label>"
        "</div>"
        "</div>"
        "</div>"
        "<div id='sd_hw_options' style='display:%s'>"
        "<div class='sensor-card' style='padding:25px'>"
        "<h3 style='text-align:center;margin-bottom:20px;color:#17a2b8'>SPI Pin Configuration</h3>"
        "<div style='display:grid;grid-template-columns:120px 1fr;gap:20px;align-items:start;margin-bottom:20px'>"
        "<label style='font-weight:600;padding-top:10px'>MOSI Pin:</label>"
        "<div>"
        "<input type='number' id='sd_mosi' name='sd_mosi' value='%d' min='0' max='39' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>SPI Master Out Slave In</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>MISO Pin:</label>"
        "<div>"
        "<input type='number' id='sd_miso' name='sd_miso' value='%d' min='0' max='39' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>SPI Master In Slave Out</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>CLK Pin:</label>"
        "<div>"
        "<input type='number' id='sd_clk' name='sd_clk' value='%d' min='0' max='39' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>SPI Clock</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>CS Pin:</label>"
        "<div>"
        "<input type='number' id='sd_cs' name='sd_cs' value='%d' min='0' max='39' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>Chip Select</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>SPI Host:</label>"
        "<div>"
        "<select id='sd_spi_host' name='sd_spi_host' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px;background:white'>"
        "<option value='1' %s>HSPI (SPI2)</option>"
        "<option value='2' %s>VSPI (SPI3)</option>"
        "</select>"
        "</div>"
        "</div>"
        "<div style='display:flex;gap:10px;margin-top:20px;flex-wrap:wrap;justify-content:center'>"
        "<button type='button' onclick='checkSDStatus()' style='background:#17a2b8;color:white;padding:12px 20px;border:none;border-radius:6px;font-weight:bold;min-width:150px;cursor:pointer'>Check SD Card Status</button>"
        "<button type='button' onclick='replayCachedMessages()' style='background:#ffc107;color:#333;padding:12px 20px;border:none;border-radius:6px;font-weight:bold;min-width:150px;cursor:pointer'>Replay Cached Messages</button>"
        "<button type='button' onclick='clearCachedMessages()' style='background:#dc3545;color:white;padding:12px 20px;border:none;border-radius:6px;font-weight:bold;min-width:150px;cursor:pointer'>Clear All Cached Messages</button>"
        "</div>"
        "<div id='sd_status_result' style='margin-top:15px;padding:10px;border-radius:6px;display:none'></div>"
        "<div style='margin-top:25px;padding:20px;background:#f8f9fa;border-radius:8px;text-align:center'>"
        "<button type='submit' style='background:#28a745;color:white;padding:12px 30px;border:none;border-radius:6px;font-weight:bold;font-size:16px;cursor:pointer'>Save SD Card Configuration</button>"
        "<div id='sd_save_result' style='margin-top:15px;padding:10px;border-radius:6px;display:none'></div>"
        "</div>"
        "</div>"
        "</div>"
        "</form>"
        "</div>",
        g_system_config.sd_config.enabled ? "checked" : "",
        g_system_config.sd_config.enabled ? "block" : "none",
        g_system_config.sd_config.cache_on_failure ? "checked" : "",
        g_system_config.sd_config.enabled ? "block" : "none",
        g_system_config.sd_config.mosi_pin,
        g_system_config.sd_config.miso_pin,
        g_system_config.sd_config.clk_pin,
        g_system_config.sd_config.cs_pin,
        g_system_config.sd_config.spi_host == 1 ? "selected" : "",
        g_system_config.sd_config.spi_host == 2 ? "selected" : "");
    httpd_resp_sendstr_chunk(req, chunk);

    // RTC Configuration Panel
    snprintf(chunk, sizeof(chunk),
        "<div id='rtc_panel'>"
        "<h2 class='section-title'><i>🕐</i>Real-Time Clock (DS3231)</h2>"
        "<form onsubmit='return saveRTCConfig()'>"
        "<div class='sensor-card' style='padding:25px'>"
        "<h3 style='text-align:center;margin-top:0;margin-bottom:20px;color:#007bff;font-size:20px'>Accurate Timekeeping</h3>"
        "<p style='color:#666;margin-bottom:25px;text-align:center;font-size:14px'>Maintain accurate time even during network outages</p>"
        "<div style='display:flex;align-items:center;justify-content:center;padding:15px;background:#f0f8ff;border-radius:8px;margin-bottom:15px'>"
        "<label for='rtc_enabled' style='display:flex;align-items:center;justify-content:center;font-weight:600;font-size:16px;cursor:pointer;margin:0'>"
        "<input type='checkbox' id='rtc_enabled' name='rtc_enabled' value='1' %s onchange='toggleRTCOptions()' style='margin-right:10px;width:18px;height:18px;cursor:pointer'>"
        "Enable RTC</label>"
        "</div>"
        "<div id='rtc_options' style='display:%s;margin-top:15px'>"
        "<div style='display:flex;align-items:center;justify-content:center;padding:15px;background:#e8f4f8;border-radius:8px;margin-bottom:10px'>"
        "<label for='rtc_sync_on_boot' style='display:flex;align-items:center;justify-content:center;font-size:15px;cursor:pointer;margin:0'>"
        "<input type='checkbox' id='rtc_sync_on_boot' name='rtc_sync_on_boot' value='1' %s style='margin-right:10px;width:18px;height:18px;cursor:pointer'>"
        "Sync System Time from RTC on Boot</label>"
        "</div>"
        "<div style='display:flex;align-items:center;justify-content:center;padding:15px;background:#e8f4f8;border-radius:8px'>"
        "<label for='rtc_update_from_ntp' style='display:flex;align-items:center;justify-content:center;font-size:15px;cursor:pointer;margin:0'>"
        "<input type='checkbox' id='rtc_update_from_ntp' name='rtc_update_from_ntp' value='1' %s style='margin-right:10px;width:18px;height:18px;cursor:pointer'>"
        "Update RTC from NTP When Online</label>"
        "</div>"
        "</div>"
        "</div>"
        "<div id='rtc_hw_options' style='display:%s'>"
        "<div class='sensor-card' style='padding:25px'>"
        "<h3 style='text-align:center;margin-bottom:20px;color:#17a2b8'>I2C Pin Configuration</h3>"
        "<div style='display:grid;grid-template-columns:120px 1fr;gap:20px;align-items:start;margin-bottom:20px'>"
        "<label style='font-weight:600;padding-top:10px'>SDA Pin:</label>"
        "<div>"
        "<input type='number' id='rtc_sda' name='rtc_sda' value='%d' min='0' max='39' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>I2C Data Line</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>SCL Pin:</label>"
        "<div>"
        "<input type='number' id='rtc_scl' name='rtc_scl' value='%d' min='0' max='39' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>I2C Clock Line</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>I2C Port:</label>"
        "<div>"
        "<select id='rtc_i2c_num' name='rtc_i2c_num' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px;background:white'>"
        "<option value='0' %s>I2C_NUM_0</option>"
        "<option value='1' %s>I2C_NUM_1</option>"
        "</select>"
        "</div>"
        "</div>"
        "<div style='display:flex;gap:10px;margin-top:20px;flex-wrap:wrap;justify-content:center'>"
        "<button type='button' onclick='getRTCTime()' style='background:#17a2b8;color:white;padding:12px 20px;border:none;border-radius:6px;font-weight:bold;min-width:150px;cursor:pointer'>Get RTC Time</button>"
        "<button type='button' onclick='syncRTCFromNTP()' style='background:#ffc107;color:#333;padding:12px 20px;border:none;border-radius:6px;font-weight:bold;min-width:150px;cursor:pointer'>Sync RTC from NTP Now</button>"
        "<button type='button' onclick='syncSystemFromRTC()' style='background:#6c757d;color:white;padding:12px 20px;border:none;border-radius:6px;font-weight:bold;min-width:150px;cursor:pointer'>Sync System Time from RTC</button>"
        "</div>"
        "<div id='rtc_time_result' style='margin-top:15px;padding:10px;border-radius:6px;display:none'></div>"
        "<div style='margin-top:25px;padding:20px;background:#f8f9fa;border-radius:8px;text-align:center'>"
        "<button type='submit' style='background:#28a745;color:white;padding:12px 30px;border:none;border-radius:6px;font-weight:bold;font-size:16px;cursor:pointer'>Save RTC Configuration</button>"
        "<div id='rtc_save_result' style='margin-top:15px;padding:10px;border-radius:6px;display:none'></div>"
        "</div>"
        "</div>"
        "</div>"
        "</form>"
        "</div>",
        g_system_config.rtc_config.enabled ? "checked" : "",
        g_system_config.rtc_config.enabled ? "block" : "none",
        g_system_config.rtc_config.sync_on_boot ? "checked" : "",
        g_system_config.rtc_config.update_from_ntp ? "checked" : "",
        g_system_config.rtc_config.enabled ? "block" : "none",
        g_system_config.rtc_config.sda_pin,
        g_system_config.rtc_config.scl_pin,
        g_system_config.rtc_config.i2c_num == 0 ? "selected" : "",
        g_system_config.rtc_config.i2c_num == 1 ? "selected" : "");
    httpd_resp_sendstr_chunk(req, chunk);

    // Telegram Bot Configuration Section
    snprintf(chunk, sizeof(chunk),
        "<div class='sensor-card' style='padding:25px;margin-top:30px'>"
        "<form id='telegram_config_form' onsubmit='return saveTelegramConfig()'>"
        "<h2 class='section-title'><i>🤖</i>Telegram Bot Configuration</h2>"
        "<div style='background:#e3f2fd;border:1px solid #90caf9;padding:15px;margin:15px 0;border-radius:6px'>"
        "<p style='margin:0;color:#1565c0'><strong>ℹ️ Remote Monitoring & Control:</strong> Control your ESP32 gateway via Telegram messenger. Get alerts, check status, and send commands remotely!</p>"
        "</div>"
        "<div class='sensor-card' style='padding:25px'>"
        "<h3 style='text-align:center;margin-top:0;margin-bottom:20px;color:#007bff;font-size:20px'>Bot Settings</h3>"
        "<p style='color:#666;margin-bottom:25px;text-align:center;font-size:14px'>Configure Telegram integration for remote access</p>"
        "<div style='display:flex;align-items:center;justify-content:center;padding:15px;background:#f0f8ff;border-radius:8px;margin-bottom:15px'>"
        "<label for='telegram_enabled' style='display:flex;align-items:center;justify-content:center;font-weight:600;font-size:16px;cursor:pointer;margin:0'>"
        "<input type='checkbox' id='telegram_enabled' name='telegram_enabled' value='1' %s onchange='toggleTelegramOptions()' style='margin-right:10px;width:18px;height:18px;cursor:pointer'>"
        "Enable Telegram Bot</label>"
        "</div>"
        "<div id='telegram_options' style='display:%s;margin-top:15px'>"
        "<div style='display:grid;grid-template-columns:150px 1fr;gap:20px;align-items:start;margin-bottom:20px'>"
        "<label style='font-weight:600;padding-top:10px'>Bot Token:</label>"
        "<div>"
        "<input type='text' id='telegram_bot_token' name='telegram_bot_token' value='%s' placeholder='123456:ABC-DEF...' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>Get from @BotFather on Telegram</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>Chat ID:</label>"
        "<div>"
        "<input type='text' id='telegram_chat_id' name='telegram_chat_id' value='%s' placeholder='123456789' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>Your Telegram user ID or group chat ID</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>Poll Interval:</label>"
        "<div>"
        "<input type='number' id='telegram_poll_interval' name='telegram_poll_interval' value='%d' min='5' max='60' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>How often to check for commands (5-60 seconds)</small>"
        "</div>"
        "</div>"
        "<div style='display:flex;align-items:center;justify-content:center;padding:15px;background:#e8f4f8;border-radius:8px;margin-top:10px'>"
        "<label for='telegram_alerts' style='display:flex;align-items:center;justify-content:center;font-size:15px;cursor:pointer;margin:0'>"
        "<input type='checkbox' id='telegram_alerts' name='telegram_alerts' value='1' %s style='margin-right:10px;width:18px;height:18px;cursor:pointer'>"
        "Send Automatic Alerts</label>"
        "</div>"
        "<div style='display:flex;align-items:center;justify-content:center;padding:15px;background:#e8f4f8;border-radius:8px;margin-top:10px'>"
        "<label for='telegram_startup' style='display:flex;align-items:center;justify-content:center;font-size:15px;cursor:pointer;margin:0'>"
        "<input type='checkbox' id='telegram_startup' name='telegram_startup' value='1' %s style='margin-right:10px;width:18px;height:18px;cursor:pointer'>"
        "Send Startup Notification</label>"
        "</div>"
        "</div>"
        "<div style='background:#fff3cd;border:1px solid #ffc107;padding:15px;margin:15px 0;border-radius:6px'>"
        "<p style='margin:0;color:#856404;font-size:14px'><strong>📱 Setup Instructions:</strong></p>"
        "<ol style='margin:10px 0 0 20px;color:#856404;font-size:13px'>"
        "<li>Open Telegram and search for <code>@BotFather</code></li>"
        "<li>Send <code>/newbot</code> and follow instructions to create your bot</li>"
        "<li>Copy the Bot Token and paste above</li>"
        "<li>Search for <code>@userinfobot</code> on Telegram and send <code>/start</code></li>"
        "<li>Copy your Chat ID and paste above</li>"
        "<li>Click Save and your bot will start!</li>"
        "</ol>"
        "</div>"
        "<div style='display:flex;gap:10px;margin-top:20px;flex-wrap:wrap;justify-content:center'>"
        "<button type='button' onclick='testTelegramBot()' style='background:#17a2b8;color:white;padding:12px 20px;border:none;border-radius:6px;font-weight:bold;min-width:150px;cursor:pointer'>🧪 Test Bot</button>"
        "</div>"
        "<div id='telegram_test_result' style='margin-top:15px;padding:10px;border-radius:6px;display:none'></div>"
        "</div>"
        "<div style='margin-top:25px;padding:20px;background:#f8f9fa;border-radius:8px;text-align:center'>"
        "<button type='submit' style='background:#28a745;color:white;padding:12px 30px;border:none;border-radius:6px;font-weight:bold;font-size:16px;cursor:pointer'>Save Telegram Configuration</button>"
        "<div id='telegram_save_result' style='margin-top:15px;padding:10px;border-radius:6px;display:none'></div>"
        "</div>"
        "</form>"
        "</div>",
        g_system_config.telegram_config.enabled ? "checked" : "",
        g_system_config.telegram_config.enabled ? "block" : "none",
        g_system_config.telegram_config.bot_token,
        g_system_config.telegram_config.chat_id,
        g_system_config.telegram_config.poll_interval,
        g_system_config.telegram_config.alerts_enabled ? "checked" : "",
        g_system_config.telegram_config.startup_notification ? "checked" : "");
    httpd_resp_sendstr_chunk(req, chunk);

    // Configuration Trigger Section
    snprintf(chunk, sizeof(chunk),
        "<div class='sensor-card' style='padding:25px'>"
        "<h3 style='text-align:center;margin-bottom:20px;color:#17a2b8'>Configuration Trigger</h3>"
        "<form id='system-control-form' onsubmit='return saveSystemConfig()'>"
        "<div style='display:grid;grid-template-columns:150px 1fr;gap:20px;align-items:start;margin-bottom:20px'>"
        "<label style='font-weight:600;padding-top:10px'>Trigger GPIO Pin:</label>"
        "<div>"
        "<input type='number' id='trigger_gpio_pin' name='trigger_gpio_pin' value='%d' min='0' max='39' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>GPIO pin for configuration mode trigger (0-39, pull LOW to enter config mode)</small>"
        "</div>"
        "</div>"
        "<div style='margin-top:25px;padding:20px;background:#f8f9fa;border-radius:8px;text-align:center'>"
        "<button type='submit' style='background:#28a745;color:white;padding:12px 30px;border:none;border-radius:6px;font-weight:bold;font-size:16px;cursor:pointer'>Save System Settings</button>"
        "</div>"
        "<div id='system-control-result' style='margin-top:15px;padding:10px;border-radius:6px;display:none'></div>"
        "</form>"
        "</div>",
        g_system_config.trigger_gpio_pin > 0 ? g_system_config.trigger_gpio_pin : 34);
    httpd_resp_sendstr_chunk(req, chunk);
    
    // Modem Control Section
    snprintf(chunk, sizeof(chunk),
        "<div class='sensor-card' style='padding:25px'>"
        "<h3 style='text-align:center;margin-bottom:20px;color:#17a2b8'>Modem Control</h3>"
        "<form id='modem-form' onsubmit='return saveModemConfig()'>"
        "<div style='display:flex;align-items:center;justify-content:center;padding:15px;background:#f0f8ff;border-radius:8px;margin-bottom:20px'>"
        "<label for='modem_reset_enabled' style='display:flex;align-items:center;justify-content:center;font-weight:600;font-size:15px;cursor:pointer;margin:0'>"
        "<input type='checkbox' id='modem_reset_enabled' name='modem_reset_enabled' value='1' %s style='margin-right:10px;width:18px;height:18px;cursor:pointer'>"
        "Enable Modem Reset on MQTT Disconnect</label>"
        "</div>"
        "<p style='color:#666;text-align:center;font-size:13px;margin-bottom:20px'>When enabled, the specified GPIO will power cycle the modem (2 seconds) on MQTT disconnection</p>"
        "<div style='display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-bottom:20px'>"
        "<label style='font-weight:600;padding-top:10px'>GPIO Pin for Modem Reset:</label>"
        "<div>"
        "<input type='number' id='modem_reset_gpio_pin' name='modem_reset_gpio_pin' value='%d' min='2' max='39' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>GPIO pin to control modem power (2-39, avoid 0,1,6-11 which are reserved)</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>Modem Boot Delay:</label>"
        "<div>"
        "<input type='number' id='modem_boot_delay' name='modem_boot_delay' value='%d' min='5' max='60' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>Time to wait for modem to boot up after reset (5-60 seconds)</small>"
        "</div>"
        "</div>"
        "<div style='margin-top:25px;padding:20px;background:#f8f9fa;border-radius:8px;text-align:center'>"
        "<button type='submit' style='background:#28a745;color:white;padding:12px 30px;border:none;border-radius:6px;font-weight:bold;font-size:16px;cursor:pointer'>Save Modem Settings</button>"
        "</div>"
        "<div id='modem-result' style='margin-top:15px;padding:10px;border-radius:6px;display:none'></div>"
        "</form>"
        "</div>", 
        g_system_config.modem_reset_enabled ? "checked" : "",
        g_system_config.modem_reset_gpio_pin > 0 ? g_system_config.modem_reset_gpio_pin : 2,
        g_system_config.modem_boot_delay > 0 ? g_system_config.modem_boot_delay : 15);
    httpd_resp_sendstr_chunk(req, chunk);
    
    snprintf(chunk, sizeof(chunk),
        "<div style='margin-top:25px;padding:20px;background:#fef4f4;border-radius:8px;text-align:center;border:1px solid #f8d7da'>"
        "<button type='button' onclick='rebootSystem()' style='background:#dc3545;color:white;padding:12px 30px;border:none;border-radius:6px;font-weight:bold;font-size:16px;cursor:pointer;box-shadow:0 2px 8px rgba(220,53,69,0.3)'>Reboot to Normal Mode</button>"
        "<p style='color:#721c24;font-size:13px;margin:10px 0 0 0'>Exit configuration mode and restart to normal operation mode.</p>"
        "</div>"
        "</div>");  // Close main WiFi section (id='wifi') containing Network Mode, WiFi, SIM, SD, RTC, Config Trigger, and Modem Control
    httpd_resp_sendstr_chunk(req, chunk);
    
    // Separate Azure IoT Hub Configuration Section - Part 1
    snprintf(chunk, sizeof(chunk),
        "<div id='azure' class='section'>"
        "<h2 class='section-title'><i>☁️</i>Azure IoT Hub Configuration (Password Protected)</h2>"
        "<div style='background:#fff3cd;border:1px solid #ffeaa7;padding:10px;margin:10px 0;border-radius:4px'>"
        "<p style='margin:0;color:#856404'><strong>Security Notice:</strong> This section contains sensitive Azure IoT Hub configuration. Access is password protected for security.</p>"
        "</div>"
        "<form method='POST' action='/save_azure_config'>"
        "<div class='sensor-card' style='padding:25px'>"
        "<h3 style='text-align:center;margin-bottom:20px;color:#007bff;font-size:20px'>Connection Settings</h3>"
        "<div style='display:grid;grid-template-columns:150px 1fr;gap:20px;align-items:start;margin-bottom:20px'>"
        "<label style='font-weight:600;padding-top:10px'>IoT Hub FQDN:</label>"
        "<div>"
        "<input type='text' name='azure_hub_fqdn' value='%s' readonly style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px;background:#f8f9fa'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>This is configured in the firmware (read-only)</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:10px'>Device ID:</label>"
        "<div>"
        "<input type='text' name='azure_device_id' value='%s' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px' placeholder='Enter your Azure device ID' required>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>Your Azure IoT Hub device identifier</small>"
        "</div>",
        IOT_CONFIG_IOTHUB_FQDN, g_system_config.azure_device_id);
    httpd_resp_sendstr_chunk(req, chunk);
    
    // Azure section - Part 2
    snprintf(chunk, sizeof(chunk),
        "<label style='font-weight:600;padding-top:10px'>Device Key *:</label>"
        "<div>"
        "<div style='position:relative'>"
        "<input type='password' id='azure_device_key' name='azure_device_key' value='%s' style='width:100%%;padding:10px;padding-right:60px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px' placeholder='Enter your Azure device primary key' required>"
        "<span onclick='toggleAzurePassword()' style='position:absolute;right:10px;top:50%%;transform:translateY(-50%%);cursor:pointer;color:#007bff;font-size:12px;font-weight:600;user-select:none;padding:4px 8px;background:#f0f8ff;border-radius:4px'>SHOW</span>"
        "</div>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>Required: Primary key from Azure IoT Hub device registration</small>"
        "</div>"
        "</div>"
        "</div>",
        g_system_config.azure_device_key);
    httpd_resp_sendstr_chunk(req, chunk);
    
    // Azure section - Part 3
    snprintf(chunk, sizeof(chunk),
        "<div class='sensor-card' style='padding:25px'>"
        "<h3 style='text-align:center;margin-bottom:20px;color:#007bff;font-size:20px'>Telemetry Settings</h3>"
        "<div style='display:grid;grid-template-columns:150px 1fr;gap:20px;align-items:start;margin-bottom:20px'>"
        "<label style='font-weight:600;padding-top:10px'>Send Interval:</label>"
        "<div>"
        "<input type='number' name='telemetry_interval' value='%d' min='30' max='3600' style='width:100%%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px'>"
        "<small style='color:#888;display:block;margin-top:5px;font-size:13px'>How often to send sensor data to Azure (30-3600 seconds)</small>"
        "</div>"
        "</div>"
        "<div style='margin-top:25px;padding:20px;background:#f8f9fa;border-radius:8px;text-align:center'>"
        "<button type='button' onclick='saveAzureConfig()' style='background:#28a745;color:white;padding:12px 30px;border:none;border-radius:6px;font-weight:bold;font-size:16px;cursor:pointer'>Save Azure Configuration</button>"
        "<p style='color:#666;font-size:12px;margin:10px 0 0 0'>This saves Azure IoT Hub settings only.</p>"
        "</div>"
        "</div>"
        "</form>"
        "</div>",
        g_system_config.telemetry_interval);
    httpd_resp_sendstr_chunk(req, chunk);

    // Telemetry Monitor section
    snprintf(chunk, sizeof(chunk),
        "<div id='telemetry' class='section'>"
        "<h2 class='section-title'><i>📊</i>Azure Telemetry Monitor</h2>"
        "<div style='background:#e3f2fd;border:1px solid #90caf9;padding:15px;margin:10px 0;border-radius:6px'>"
        "<p style='margin:0;color:#1565c0'><strong>ℹ️ Live Telemetry Stream:</strong> View real-time data being sent to Azure IoT Hub. Last 25 messages shown (newest first). Auto-refreshes every 5 seconds.</p>"
        "</div>"
        "<div class='sensor-card' style='padding:20px'>"
        "<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:15px'>"
        "<h3 style='margin:0;color:#007bff'>Recent Telemetry Messages</h3>"
        "<div style='display:flex;gap:10px'>"
        "<button onclick='downloadTelemetryCSV()' style='padding:8px 15px;background:#17a2b8;color:white;border:none;border-radius:4px;cursor:pointer;font-weight:600'>📥 Download CSV</button>"
        "<button onclick='downloadTelemetryJSON()' style='padding:8px 15px;background:#6c757d;color:white;border:none;border-radius:4px;cursor:pointer;font-weight:600'>📥 Download JSON</button>"
        "<button onclick='refreshTelemetryNow()' style='padding:8px 15px;background:#28a745;color:white;border:none;border-radius:4px;cursor:pointer;font-weight:600'>🔄 Refresh Now</button>"
        "</div>"
        "</div>"
        "<div id='telemetry-status' style='text-align:center;padding:10px;background:#f8f9fa;border-radius:4px;margin-bottom:15px;color:#666;font-size:14px'>"
        "Loading telemetry data..."
        "</div>"
        "<div style='overflow-x:auto'>"
        "<table style='width:100%%;border-collapse:collapse;font-size:13px'>"
        "<thead>"
        "<tr style='background:#007bff;color:white'>"
        "<th style='padding:12px;text-align:left;border:1px solid #dee2e6'>#</th>"
        "<th style='padding:12px;text-align:left;border:1px solid #dee2e6'>Date & Time (24h)</th>"
        "<th style='padding:12px;text-align:left;border:1px solid #dee2e6'>Status</th>"
        "<th style='padding:12px;text-align:left;border:1px solid #dee2e6;min-width:400px'>Payload (JSON)</th>"
        "</tr>"
        "</thead>"
        "<tbody id='telemetry-table-body'>"
        "<tr><td colspan='4' style='text-align:center;padding:20px;color:#888'>No data yet...</td></tr>"
        "</tbody>"
        "</table>"
        "</div>"
        "</div>"
        "</div>");
    httpd_resp_sendstr_chunk(req, chunk);

    // Sensors section with sub-menus
    int regular_sensor_count = 0;
    int water_quality_sensor_count = 0;
    
    // Count sensors by type
    for (int i = 0; i < g_system_config.sensor_count; i++) {
        if (g_system_config.sensors[i].enabled) {
            if (strcmp(g_system_config.sensors[i].sensor_type, "QUALITY") == 0) {
                water_quality_sensor_count++;
            } else {
                regular_sensor_count++;
            }
        }
    }
    
    snprintf(chunk, sizeof(chunk), 
        "<div id='sensors' class='section'>"
        "<h2 class='section-title'><i>🔌</i>Modbus Sensors (Total: %d)</h2>"
        
        "<!-- Sub-menu Navigation -->"
        "<div style='background:#f8f9fa;padding:15px;margin:10px 0;border-radius:5px;border:1px solid #dee2e6'>"
        "<div style='display:flex;gap:10px;flex-wrap:wrap'>"
        "<button type='button' onclick='showSensorSubMenu(\"regular\")' id='btn-regular-sensors' style='background:#007bff;color:white;padding:12px 20px;border:none;border-radius:6px;cursor:pointer;font-weight:bold;min-width:150px'>Regular Sensors (%d)</button>"
        "<button type='button' onclick='showSensorSubMenu(\"water_quality\")' id='btn-water-quality-sensors' style='background:#17a2b8;color:white;padding:12px 20px;border:none;border-radius:6px;cursor:pointer;font-weight:bold;min-width:150px'>Water Quality Sensors (%d)</button>"
"<button type='button' onclick='showSensorSubMenu(\"explorer\")' id='btn-modbus-explorer' style='background:#6c757d;color:white;padding:12px 20px;border:none;border-radius:6px;cursor:pointer;font-weight:bold;min-width:150px'>🔍 Modbus Explorer</button>"
        "</div>"
        "</div>", 
        g_system_config.sensor_count, regular_sensor_count, water_quality_sensor_count);
    httpd_resp_sendstr_chunk(req, chunk);
    
    // Regular Sensors Section
    httpd_resp_sendstr_chunk(req,
        "<div id='regular-sensors-list' class='sensor-submenu' style='display:block'>"
        "<h3 style='color:#007bff;margin:15px 0 10px 0'>Regular Sensors</h3>");
    
    ESP_LOGI(TAG, "Displaying regular sensors in web interface");
    bool has_regular_sensors = false;
    for (int i = 0; i < g_system_config.sensor_count; i++) {
        if (g_system_config.sensors[i].enabled && strcmp(g_system_config.sensors[i].sensor_type, "QUALITY") != 0) {
            has_regular_sensors = true;
            ESP_LOGI(TAG, "Display Regular Sensor %d: name='%s', sensor_type='%s' (len=%d)", i, g_system_config.sensors[i].name, g_system_config.sensors[i].sensor_type, strlen(g_system_config.sensors[i].sensor_type));
            
            // Show different parameters based on sensor type
            if (strcmp(g_system_config.sensors[i].sensor_type, "Level") == 0) {
                snprintf(chunk, sizeof(chunk),
                    "<div class='sensor-card' id='sensor-card-%d'>"
                    "<h3>%s (Sensor %d) - <span style='color:#007bff;font-weight:bold;'>%s</span></h3>"
                    "<p><strong>Unit ID:</strong> %s</p>"
                    "<p><strong>Slave ID:</strong> %d</p>"
                    "<p><strong>Register:</strong> %d</p>"
                    "<p><strong>Quantity:</strong> %d</p>"
                    "<p><strong>Register Type:</strong> %s</p>"
                    "<p><strong>Data Type:</strong> %s</p>"
                    "<p><strong>Sensor Height:</strong> %.2f</p>"
                    "<p><strong>Max Water Level:</strong> %.2f</p>"
                    "<p style='color:#666;font-size:12px;margin:5px 0;grid-column:1/-1'>RS485 Modbus - Level calc: (Height - Raw) / MaxLevel * 100</p>"
                    "<button type='button' onclick='editSensor(%d)' style='background:#17a2b8;color:white;margin:3px;padding:10px 18px;border:none;border-radius:5px;font-weight:bold;cursor:pointer'>Edit</button> "
                    "<button type='button' onclick='testSensor(%d)' style='background:#007bff;color:white;margin:3px;padding:10px 18px;border:none;border-radius:5px;font-weight:bold;cursor:pointer'>Test RS485</button> "
                    "<button type='button' onclick='deleteSensor(%d)' style='background:#dc3545;color:white;margin:3px;padding:10px 18px;border:none;border-radius:5px;font-weight:bold;cursor:pointer'>Delete</button>"
                    "<div id='test-result-%d' class='test-result' style='display:none'></div>"
                    "</div>",
                    i, g_system_config.sensors[i].name, i + 1, g_system_config.sensors[i].sensor_type,
                    g_system_config.sensors[i].unit_id, g_system_config.sensors[i].slave_id,
                    g_system_config.sensors[i].register_address, g_system_config.sensors[i].quantity,
                    g_system_config.sensors[i].register_type[0] ? g_system_config.sensors[i].register_type : "HOLDING",
                    g_system_config.sensors[i].data_type, 
                    g_system_config.sensors[i].sensor_height, g_system_config.sensors[i].max_water_level,
                    i, i, i, i);
            } else if (strcmp(g_system_config.sensors[i].sensor_type, "RAINGAUGE") == 0) {
                snprintf(chunk, sizeof(chunk),
                    "<div class='sensor-card' id='sensor-card-%d'>"
                    "<h3>%s (Sensor %d) - <span style='color:#28a745;font-weight:bold;'>Rain Gauge</span></h3>"
                    "<p><strong>Unit ID:</strong> %s</p>"
                    "<p><strong>Slave ID:</strong> %d</p>"
                    "<p><strong>Register:</strong> %d</p>"
                    "<p><strong>Quantity:</strong> %d</p>"
                    "<p><strong>Register Type:</strong> %s</p>"
                    "<p><strong>Data Type:</strong> %s</p>"
                    "<p><strong>Scale:</strong> %.3f</p>"
                    "<p style='color:#666;font-size:12px;margin:5px 0;grid-column:1/-1'>RS485 Modbus - Rain gauge measurement in mm or inches</p>"
                    "<button type='button' onclick='editSensor(%d)' style='background:#17a2b8;color:white;margin:3px;padding:10px 18px;border:none;border-radius:5px;font-weight:bold;cursor:pointer'>Edit</button> "
                    "<button type='button' onclick='testSensor(%d)' style='background:#007bff;color:white;margin:3px;padding:10px 18px;border:none;border-radius:5px;font-weight:bold;cursor:pointer'>Test RS485</button> "
                    "<button type='button' onclick='deleteSensor(%d)' style='background:#dc3545;color:white;margin:3px;padding:10px 18px;border:none;border-radius:5px;font-weight:bold;cursor:pointer'>Delete</button>"
                    "<div id='test-result-%d' class='test-result' style='display:none'></div>"
                    "</div>",
                    i, g_system_config.sensors[i].name, i + 1,
                    g_system_config.sensors[i].unit_id, g_system_config.sensors[i].slave_id,
                    g_system_config.sensors[i].register_address, g_system_config.sensors[i].quantity,
                    g_system_config.sensors[i].register_type[0] ? g_system_config.sensors[i].register_type : "HOLDING",
                    g_system_config.sensors[i].data_type, g_system_config.sensors[i].scale_factor,
                    i, i, i, i);
            } else if (strcmp(g_system_config.sensors[i].sensor_type, "BOREWELL") == 0) {
                snprintf(chunk, sizeof(chunk),
                    "<div class='sensor-card' id='sensor-card-%d'>"
                    "<h3>%s (Sensor %d) - <span style='color:#6f42c1;font-weight:bold;'>Borewell</span></h3>"
                    "<p><strong>Unit ID:</strong> %s</p>"
                    "<p><strong>Slave ID:</strong> %d</p>"
                    "<p><strong>Register:</strong> %d</p>"
                    "<p><strong>Quantity:</strong> %d</p>"
                    "<p><strong>Register Type:</strong> %s</p>"
                    "<p><strong>Data Type:</strong> %s</p>"
                    "<p><strong>Scale:</strong> %.3f</p>"
                    "<p style='color:#666;font-size:12px;margin:5px 0;grid-column:1/-1'>RS485 Modbus - Borewell water level and flow monitoring</p>"
                    "<button type='button' onclick='editSensor(%d)' style='background:#17a2b8;color:white;margin:3px;padding:10px 18px;border:none;border-radius:5px;font-weight:bold;cursor:pointer'>Edit</button> "
                    "<button type='button' onclick='testSensor(%d)' style='background:#007bff;color:white;margin:3px;padding:10px 18px;border:none;border-radius:5px;font-weight:bold;cursor:pointer'>Test RS485</button> "
                    "<button type='button' onclick='deleteSensor(%d)' style='background:#dc3545;color:white;margin:3px;padding:10px 18px;border:none;border-radius:5px;font-weight:bold;cursor:pointer'>Delete</button>"
                    "<div id='test-result-%d' class='test-result' style='display:none'></div>"
                    "</div>",
                    i, g_system_config.sensors[i].name, i + 1,
                    g_system_config.sensors[i].unit_id, g_system_config.sensors[i].slave_id,
                    g_system_config.sensors[i].register_address, g_system_config.sensors[i].quantity,
                    g_system_config.sensors[i].register_type[0] ? g_system_config.sensors[i].register_type : "HOLDING",
                    g_system_config.sensors[i].data_type, g_system_config.sensors[i].scale_factor,
                    i, i, i, i);
            } else if (strcmp(g_system_config.sensors[i].sensor_type, "ENERGY") == 0) {
                snprintf(chunk, sizeof(chunk),
                    "<div class='sensor-card' id='sensor-card-%d'>"
                    "<h3>%s (Sensor %d) - <span style='color:#fd7e14;font-weight:bold;'>Energy Meter</span></h3>"
                    "<p><strong>Unit ID:</strong> %s</p>"
                    "<p><strong>Slave ID:</strong> %d</p>"
                    "<p><strong>Register:</strong> %d</p>"
                    "<p><strong>Quantity:</strong> %d</p>"
                    "<p><strong>Register Type:</strong> %s</p>"
                    "<p><strong>Data Type:</strong> %s</p>"
                    "<p><strong>Scale:</strong> %.3f</p>"
                    "<p style='color:#666;font-size:12px;margin:5px 0;grid-column:1/-1'>RS485 Modbus - Energy meter monitoring with power consumption data</p>"
                    "<button type='button' onclick='editSensor(%d)' style='background:#17a2b8;color:white;margin:3px;padding:10px 18px;border:none;border-radius:5px;font-weight:bold;cursor:pointer'>Edit</button> "
                    "<button type='button' onclick='testSensor(%d)' style='background:#007bff;color:white;margin:3px;padding:10px 18px;border:none;border-radius:5px;font-weight:bold;cursor:pointer'>Test RS485</button> "
                    "<button type='button' onclick='deleteSensor(%d)' style='background:#dc3545;color:white;margin:3px;padding:10px 18px;border:none;border-radius:5px;font-weight:bold;cursor:pointer'>Delete</button>"
                    "<div id='test-result-%d' class='test-result' style='display:none'></div>"
                    "</div>",
                    i, g_system_config.sensors[i].name, i + 1,
                    g_system_config.sensors[i].unit_id, g_system_config.sensors[i].slave_id,
                    g_system_config.sensors[i].register_address, g_system_config.sensors[i].quantity,
                    g_system_config.sensors[i].register_type[0] ? g_system_config.sensors[i].register_type : "HOLDING",
                    g_system_config.sensors[i].data_type, g_system_config.sensors[i].scale_factor,
                    i, i, i, i);
            } else {
                snprintf(chunk, sizeof(chunk),
                    "<div class='sensor-card' id='sensor-card-%d'>"
                    "<h3>%s (Sensor %d) - <span style='color:#007bff;font-weight:bold;'>%s</span></h3>"
                    "<p><strong>Unit ID:</strong> %s</p>"
                    "<p><strong>Slave ID:</strong> %d</p>"
                    "<p><strong>Register:</strong> %d</p>"
                    "<p><strong>Quantity:</strong> %d</p>"
                    "<p><strong>Register Type:</strong> %s</p>"
                    "<p><strong>Data Type:</strong> %s</p>"
                    "<p><strong>Scale:</strong> %.3f</p>"
                    "<p style='color:#666;font-size:12px;margin:5px 0;grid-column:1/-1'>RS485 Modbus - Real-time data with ScadaCore format interpretations</p>"
                    "<button type='button' onclick='editSensor(%d)' style='background:#17a2b8;color:white;margin:3px;padding:10px 18px;border:none;border-radius:5px;font-weight:bold;cursor:pointer'>Edit</button> "
                    "<button type='button' onclick='testSensor(%d)' style='background:#007bff;color:white;margin:3px;padding:10px 18px;border:none;border-radius:5px;font-weight:bold;cursor:pointer'>Test RS485</button> "
                    "<button type='button' onclick='deleteSensor(%d)' style='background:#dc3545;color:white;margin:3px;padding:10px 18px;border:none;border-radius:5px;font-weight:bold;cursor:pointer'>Delete</button>"
                    "<div id='test-result-%d' class='test-result' style='display:none'></div>"
                    "</div>",
                    i, g_system_config.sensors[i].name, i + 1, g_system_config.sensors[i].sensor_type[0] ? g_system_config.sensors[i].sensor_type : "Flow-Meter",
                    g_system_config.sensors[i].unit_id, g_system_config.sensors[i].slave_id,
                    g_system_config.sensors[i].register_address, g_system_config.sensors[i].quantity,
                    g_system_config.sensors[i].register_type[0] ? g_system_config.sensors[i].register_type : "HOLDING",
                    g_system_config.sensors[i].data_type, g_system_config.sensors[i].scale_factor, i, i, i, i);
            }
            httpd_resp_sendstr_chunk(req, chunk);
        }
    }
    
    if (!has_regular_sensors) {
        httpd_resp_sendstr_chunk(req, 
            "<div style='background:#e3f2fd;border:1px solid #90caf9;padding:10px;margin:10px 0;border-radius:4px'>"
            "<p><strong>INFO: No regular sensors configured yet.</strong></p>"
            "<p>Use the Add New Regular Sensor button below to add regular sensors like Level, Flow-Meter, Energy, etc.</p>"
            "</div>");
    }
    
    // Add New Regular Sensor button
    httpd_resp_sendstr_chunk(req, 
        "<div style='background:#f8f9fa;padding:15px;margin:15px 0;border-radius:8px;border:1px solid #dee2e6'>"
        "<button type='button' onclick='addRegularSensor()' style='background:linear-gradient(135deg,#28a745,#20c997);color:white;padding:14px 35px;margin:10px;border:none;border-radius:8px;font-size:16px;font-weight:600;cursor:pointer;box-shadow:0 4px 12px rgba(40,167,69,0.3);transition:all 0.3s ease' onmouseover='this.style.transform=\"translateY(-2px)\";this.style.boxShadow=\"0 6px 16px rgba(40,167,69,0.4)\"' onmouseout='this.style.transform=\"translateY(0)\";this.style.boxShadow=\"0 4px 12px rgba(40,167,69,0.3)\"'>➕ Add New Regular Sensor</button>"
        "<p style='color:#666;font-size:12px;margin:10px 0 5px 0'>Add Level, Flow-Meter, Energy, or other regular Modbus sensors</p>"
        "</div>");
    
    httpd_resp_sendstr_chunk(req, "</div>");
    
    // Water Quality Sensors Section
    httpd_resp_sendstr_chunk(req,
        "<div id='water-quality-sensors-list' class='sensor-submenu' style='display:none'>"
        "<h3 style='color:#17a2b8;margin:15px 0 10px 0'>Water Quality Sensors</h3>");
    
    ESP_LOGI(TAG, "Displaying water quality sensors in web interface");
    bool has_quality_sensors = false;
    for (int i = 0; i < g_system_config.sensor_count; i++) {
        if (g_system_config.sensors[i].enabled && strcmp(g_system_config.sensors[i].sensor_type, "QUALITY") == 0) {
            has_quality_sensors = true;
            ESP_LOGI(TAG, "Water Quality Sensor %d: %s", i, g_system_config.sensors[i].name);
            
            snprintf(chunk, sizeof(chunk),
                "<div class='sensor-card' id='sensor-card-%d'>" 
                "<h3>%s (Sensor %d) - <span style='color:#17a2b8;font-weight:bold;'>Water Quality</span></h3>"
                "<p><strong>Unit ID:</strong> %s | <strong>Sub-Sensors:</strong> %d configured</p>"
                "<p style='color:#666;font-size:12px;margin:5px 0'>[TEST] Water Quality Modbus Sensor - Real-time water parameter monitoring</p>",
                i, g_system_config.sensors[i].name, i + 1,
                g_system_config.sensors[i].unit_id, g_system_config.sensors[i].sub_sensor_count);
            httpd_resp_sendstr_chunk(req, chunk);
            
            // Show configured sub-sensor parameters
            if (g_system_config.sensors[i].sub_sensor_count > 0) {
                httpd_resp_sendstr_chunk(req, "<div style='background:#f0f8ff;padding:10px;margin:5px 0;border-radius:4px;border:1px solid #17a2b8'>");
                httpd_resp_sendstr_chunk(req, "<strong style='color:#17a2b8'>[PARAM] Configured Parameters:</strong> ");
                bool first_param = true;
                for (int j = 0; j < g_system_config.sensors[i].sub_sensor_count; j++) {
                    if (g_system_config.sensors[i].sub_sensors[j].enabled && 
                        strlen(g_system_config.sensors[i].sub_sensors[j].parameter_name) > 0) {
                        if (!first_param) {
                            httpd_resp_sendstr_chunk(req, ", ");
                        }
                        char escaped_param_name[64];
                        html_escape(escaped_param_name, g_system_config.sensors[i].sub_sensors[j].parameter_name, sizeof(escaped_param_name));
                        snprintf(chunk, sizeof(chunk), "<span style='color:#28a745;font-weight:bold'>%s</span> (ID:%d, Reg:%d)", 
                                escaped_param_name,
                                g_system_config.sensors[i].sub_sensors[j].slave_id,
                                g_system_config.sensors[i].sub_sensors[j].register_address);
                        httpd_resp_sendstr_chunk(req, chunk);
                        first_param = false;
                    }
                }
                httpd_resp_sendstr_chunk(req, "</div>");
            }
            
            snprintf(chunk, sizeof(chunk),
                "<button type='button' onclick='editSensor(%d)' style='background:#17a2b8;color:white;margin:2px;padding:6px 12px'>Edit</button> "
                "<button type='button' onclick='testSensor(%d)' style='background:#007bff;color:white;margin:2px;padding:6px 12px'>Test RS485</button> "
                "<button type='button' onclick='deleteSensor(%d)' style='background:#dc3545;color:white;margin:2px;padding:6px 12px'>Delete</button>"
                "<div id='test-result-%d' class='test-result' style='display:none'></div>"
                "</div>", i, i, i, i);
            httpd_resp_sendstr_chunk(req, chunk);
        }
    }
    
    if (!has_quality_sensors) {
        httpd_resp_sendstr_chunk(req, 
            "<div style='background:#fff3cd;border:1px solid #ffeaa7;padding:10px;margin:10px 0;border-radius:4px'>"
            "<p><strong>INFO: No water quality sensors configured yet.</strong></p>"
            "<p>Use the Add New Water Quality Sensor button below to add water quality sensors like pH, TDS, Temperature, etc.</p>"
            "</div>");
    }
    
    // Add New Water Quality Sensor button
    httpd_resp_sendstr_chunk(req, 
        "<div style='background:#f8f9fa;padding:15px;margin:15px 0;border-radius:8px;border:1px solid #dee2e6'>"
        "<button type='button' onclick='addWaterQualitySensor()' style='background:linear-gradient(135deg,#17a2b8,#20c997);color:white;padding:14px 35px;margin:10px;border:none;border-radius:8px;font-size:16px;font-weight:600;cursor:pointer;box-shadow:0 4px 12px rgba(23,162,184,0.3);transition:all 0.3s ease' onmouseover='this.style.transform=\"translateY(-2px)\";this.style.boxShadow=\"0 6px 16px rgba(23,162,184,0.4)\"' onmouseout='this.style.transform=\"translateY(0)\";this.style.boxShadow=\"0 4px 12px rgba(23,162,184,0.3)\"'>💧 Add Water Quality Sensor</button>"
        "<p style='color:#666;font-size:12px;margin:10px 0 5px 0'>Create individual water quality sensors with unique Unit IDs and custom Modbus configurations</p>"
        "</div>");
    
    httpd_resp_sendstr_chunk(req, "</div>");
    
    // Modbus Explorer Section
    httpd_resp_sendstr_chunk(req,
        "<div id='modbus-explorer-list' class='sensor-submenu' style='display:none'>"
        "<h3 style='color:#6c757d;margin:15px 0 10px 0'>Modbus Explorer</h3>"
        ""
        "<div class='sensor-card'>"
        "<h3>Device Scanner</h3>"
        "<p>Scan the RS485 bus to discover Modbus devices by checking responses from slave IDs.</p>"
        "<div class='form-grid'>"
        "<label>Start Slave ID:</label>"
        "<input type='number' id='scan_start' min='1' max='247' value='1'>"
        "<label>End Slave ID:</label>"
        "<input type='number' id='scan_end' min='1' max='247' value='10'>"
        "<label>Register to Test:</label>"
        "<input type='number' id='scan_register' min='0' max='65535' value='0'>"
        "<label>Register Type:</label>"
        "<select id='scan_reg_type'>"
        "<option value='holding'>Holding Register (0x03)</option>"
        "<option value='input'>Input Register (0x04)</option>"
        "</select>"
        "</div>"
        "<button onclick='scanModbusDevices()' class='btn' style='background:var(--color-accent);color:white;width:auto;min-width:200px'>Scan for Devices</button>"
        "<div id='scan_progress' style='margin-top:var(--space-md);padding:var(--space-md);background:var(--color-bg-secondary);border-radius:var(--radius-md);display:none'></div>"
        "<div id='scan_results' style='margin-top:var(--space-md)'></div>"
        "</div>"
        ""
        "<div class='sensor-card'>"
        "<h3>Live Register Reader</h3>"
        "<p>Read registers in real-time with automatic format interpretation (ScadaCore compatible).</p>"
        "<div class='form-grid'>"
        "<label>Slave ID:</label>"
        "<input type='number' id='live_slave' min='1' max='247' value='1'>"
        "<label>Start Register:</label>"
        "<input type='number' id='live_register' min='0' max='65535' value='0'>"
        "<label>Quantity:</label>"
        "<input type='number' id='live_quantity' min='1' max='10' value='2'>"
        "<label>Register Type:</label>"
        "<select id='live_reg_type'>"
        "<option value='holding'>Holding Register (0x03)</option>"
        "<option value='input'>Input Register (0x04)</option>"
        "</select>"
        "</div>"
        "<div style='display:flex;gap:var(--space-md);margin-top:var(--space-md)'>"
        "<button onclick='readLiveRegisters()' class='btn' style='background:var(--color-success);color:white;flex:1'>Read Once</button>"
        "<button onclick='toggleAutoRefresh()' id='auto_refresh_btn' class='btn' style='background:var(--color-primary);color:white;flex:1'>Enable Auto-Refresh</button>"
        "</div>"
        "<div id='live_result' style='margin-top:var(--space-md)'></div>"
        "</div>");

    // Live Sensor Poll Card - Modbus Poll style
    httpd_resp_sendstr_chunk(req,
        ""
        "<div class='sensor-card'>"
        "<h3>📊 Live Sensor Poll (Modbus Poll Style)</h3>"
        "<p>Test ANY Modbus device in real-time - just like Modbus Poll software. Enter parameters and start polling to see live register values.</p>"
        "<div class='form-grid'>"
        "<label>Slave ID:</label>"
        "<input type='number' id='poll_slave' min='1' max='247' value='1'>"
        "<label>Start Register:</label>"
        "<input type='number' id='poll_register' min='0' max='65535' value='0'>"
        "<label>Quantity:</label>"
        "<input type='number' id='poll_quantity' min='1' max='20' value='5'>"
        "<label>Register Type:</label>"
        "<select id='poll_reg_type'>"
        "<option value='holding'>Holding Register (0x03)</option>"
        "<option value='input'>Input Register (0x04)</option>"
        "</select>"
        "<label>Poll Interval:</label>"
        "<select id='poll_interval'>"
        "<option value='1000'>1 second</option>"
        "<option value='2000'>2 seconds</option>"
        "<option value='3000' selected>3 seconds</option>"
        "<option value='5000'>5 seconds</option>"
        "<option value='10000'>10 seconds</option>"
        "</select>"
        "</div>"
        "<div style='display:flex;gap:var(--space-md);margin-top:var(--space-md)'>"
        "<button onclick='startModbusPoll()' id='start_poll_btn' class='btn' style='background:#28a745;color:white;flex:1'>Start Polling</button>"
        "<button onclick='stopModbusPoll()' id='stop_poll_btn' class='btn' style='background:#dc3545;color:white;flex:1;display:none'>Stop Polling</button>"
        "</div>"
        "<div id='poll_result' style='margin-top:var(--space-md)'></div>"
        "</div>"
        ""
        "<div class='sensor-card'>"
        "<h3>Explorer Notes</h3>"
        "<ul style='margin:10px 0;padding-left:20px'>"
        "<li><strong>Device Scanner:</strong> Tests each slave ID for a response. Non-responsive IDs are skipped.</li>"
        "<li><strong>Register Reader:</strong> Reads registers and shows all possible format interpretations.</li>"
        "<li><strong>Auto-Refresh:</strong> Continuously reads registers every 2 seconds for monitoring.</li>"
        "<li><strong>Format Display:</strong> Shows data in 16 formats (UINT16/32, INT16/32, FLOAT32, all byte orders).</li>"
        "<li><strong>Industrial Use:</strong> Useful for commissioning, troubleshooting, and device discovery.</li>"
        "</ul>"
        "</div>"
        "</div>"
    );

    // Add debug info if no sensors at all
    if (g_system_config.sensor_count == 0) {
        httpd_resp_sendstr_chunk(req, 
            "<div style='background:#f8d7da;border:1px solid #f5c6cb;padding:15px;margin:15px 0;border-radius:5px'>"
            "<h4 style='color:#721c24;margin:0 0 10px 0'>No Sensors Configured</h4>"
            "<p style='color:#721c24;margin:5px 0'>Get started by adding sensors:</p>"
            "<ul style='color:#721c24;margin:10px 0;padding-left:20px'>"
            "<li><strong>Regular Sensors:</strong> Use the 'Add New Regular Sensor' button in the Regular Sensors section for Level, Flow-Meter, Energy sensors</li>"
            "<li><strong>Water Quality Sensors:</strong> Use the 'Add New Water Quality Sensor' button in the Water Quality section for pH, TDS, Temperature sensors</li>"
            "</ul>"
            "</div>");
    }

    // Close the sensors section
    httpd_resp_sendstr_chunk(req, "</div>");
    
    // JavaScript - split into smaller chunks  
    snprintf(chunk, sizeof(chunk),
        "<script>"
        "console.log('Script loading - Initial sensor count: %d');"
        "let sensorCount = %d;"
        "function addSensor() {"
        "  console.log('ADD SENSOR CLICKED - Current count:', sensorCount);"
        "  var div = document.getElementById('sensors');"
        "  if (!div) { alert('ERROR: Sensors container not found!'); console.error('sensors div missing'); return; }"
        "  console.log('Found sensors div, building HTML...');"
        "  var h = '<div class=\"sensor-card\" id=\"sensor-card-' + sensorCount + '\" style=\"border:2px dashed #007bff;padding:15px;margin:10px 0;background:#f8f9fa\">';"
        "  h += '<h4 style=\"color:#007bff;margin-top:0\">New Sensor ' + (sensorCount+1) + '</h4>';"
        "  h += '<style>.sensor-card label { display:inline-block; width:160px; font-weight:bold; margin-bottom:8px; vertical-align:top; } .sensor-card input, .sensor-card select { margin-bottom:12px; } .sensor-form-row { margin-bottom:12px; }</style>';"
        "  h += '<label><strong>Sensor Type *:</strong></label><select name=\"sensor_' + sensorCount + '_sensor_type\" onchange=\"updateSensorForm(' + sensorCount + ')\" style=\"width:200px;border:2px solid #dc3545\" required>';"
        "  h += '<option value=\"\">Select Sensor Type</option>';"
        "  h += '<option value=\"Flow-Meter\">Flow-Meter</option>';"
        "  h += '<option value=\"Panda_USM\">Panda USM</option>';"
        "  h += '<option value=\"Clampon\">Clampon</option>';"
        "  h += '<option value=\"Dailian\">Dailian Ultrasonic</option>';"
        "  h += '<option value=\"Piezometer\">Piezometer (Water Level)</option>';"
        "  h += '<option value=\"Level\">Level</option>';"
        "  h += '<option value=\"Radar Level\">Radar Level</option>';"
        "  h += '<option value=\"RAINGAUGE\">Rain Gauge</option>';"
        "  h += '<option value=\"BOREWELL\">Borewell</option>';"
        "  h += '<option value=\"ENERGY\">Energy Meter</option>';"
        "  h += '<option value=\"ZEST\">ZEST</option>';"
        "  h += '</select><br>';"
        "  h += '<div id=\"sensor-form-' + sensorCount + '\" style=\"display:none\">';"
        "  h += '</div>';",
        g_system_config.sensor_count, g_system_config.sensor_count);
    httpd_resp_sendstr_chunk(req, chunk);

    // Add buttons and closing for the sensor form container
    httpd_resp_sendstr_chunk(req,
        "  h += '<div style=\"margin-top:25px;padding-top:20px;border-top:1px solid #e0e0e0;text-align:center\">';"
        "  h += '<button type=\"button\" onclick=\"testNewSensorRS485(' + sensorCount + ')\" style=\"background:linear-gradient(135deg,#17a2b8,#138496);color:white;padding:12px 28px;margin:5px;border:none;border-radius:6px;font-weight:600;cursor:pointer;box-shadow:0 3px 8px rgba(23,162,184,0.3);transition:all 0.3s ease;font-size:15px\" onmouseover=\"this.style.transform=\\'translateY(-2px)\\';this.style.boxShadow=\\'0 5px 12px rgba(23,162,184,0.4)\\'\" onmouseout=\"this.style.transform=\\'translateY(0)\\';this.style.boxShadow=\\'0 3px 8px rgba(23,162,184,0.3)\\'\">🔍 Test RS485</button>';"
        "  h += '<button type=\"button\" onclick=\"saveSingleSensor(' + sensorCount + ')\" style=\"background:linear-gradient(135deg,#28a745,#218838);color:white;padding:12px 28px;margin:5px;border:none;border-radius:6px;font-weight:600;cursor:pointer;box-shadow:0 3px 8px rgba(40,167,69,0.3);transition:all 0.3s ease;font-size:15px\" onmouseover=\"this.style.transform=\\'translateY(-2px)\\';this.style.boxShadow=\\'0 5px 12px rgba(40,167,69,0.4)\\'\" onmouseout=\"this.style.transform=\\'translateY(0)\\';this.style.boxShadow=\\'0 3px 8px rgba(40,167,69,0.3)\\'\">💾 Save This Sensor</button>';"
        "  h += '<button type=\"button\" onclick=\"removeSensorForm(' + sensorCount + ')\" style=\"background:linear-gradient(135deg,#dc3545,#c82333);color:white;padding:12px 28px;margin:5px;border:none;border-radius:6px;font-weight:600;cursor:pointer;box-shadow:0 3px 8px rgba(220,53,69,0.3);transition:all 0.3s ease;font-size:15px\" onmouseover=\"this.style.transform=\\'translateY(-2px)\\';this.style.boxShadow=\\'0 5px 12px rgba(220,53,69,0.4)\\'\" onmouseout=\"this.style.transform=\\'translateY(0)\\';this.style.boxShadow=\\'0 3px 8px rgba(220,53,69,0.3)\\'\">❌ Cancel</button>';"
        "  h += '<div id=\"test-result-new-' + sensorCount + '\" style=\"margin-top:10px;display:none\"></div>';"
        "  h += '</div></div>';");

    httpd_resp_sendstr_chunk(req,
        "  console.log('Adding HTML to sensors div...');"
        "  div.innerHTML += h;"
        "  var currentSensorId = sensorCount;"
        "  sensorCount++;"
        "  setTimeout(() => {"
        "    const select = document.querySelector('select[name=\"sensor_' + currentSensorId + '_data_type\"]');"
        "    if (select) {"
        "      select.value = '';"
        "      showDataTypeInfo(select, currentSensorId);"
        "    }"
        "  }, 100);"
        "  console.log('SUCCESS: New sensor added. Updated count:', sensorCount);"
        "  alert('SUCCESS: New sensor form added!\\n\\nSTEPS:\\n1. Fill in Name and Unit ID (required fields marked with red borders)\\n2. Select data type to see detailed format interpretations\\n3. Review auto-set quantity and adjust other settings as needed\\n4. Click Save This Sensor\\n\\nEach sensor is saved individually to flash memory!');"
        "}"
        "function addRegularSensor() {"
        "  console.log('ADD REGULAR SENSOR CLICKED - Current count:', sensorCount);"
        "  var div = document.getElementById('regular-sensors-list');"
        "  if (!div) { alert('ERROR: Regular sensors container not found!'); console.error('regular-sensors-list div missing'); return; }"
        "  console.log('Found regular sensors div, building HTML...');"
        "  var h = '<div class=\"sensor-card\" id=\"sensor-card-' + sensorCount + '\" style=\"border:2px solid #e0e0e0;border-radius:8px;padding:25px;margin:20px 0;background:#ffffff;box-shadow:0 2px 4px rgba(0,0,0,0.1)\">';"
        "  h += '<h4 style=\"color:#007bff;margin-top:0;margin-bottom:20px;font-size:18px\">New Regular Sensor ' + (sensorCount+1) + '</h4>';"
        "  h += '<div style=\"display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-bottom:20px\">';"
        "  h += '<label style=\"font-weight:600;padding-top:10px\">Sensor Type *:</label>';"
        "  h += '<div>';"
        "  h += '<select name=\"sensor_' + sensorCount + '_sensor_type\" onchange=\"updateSensorForm(' + sensorCount + ')\" style=\"width:100%;padding:10px;border:2px solid #dc3545;border-radius:6px;font-size:15px\" required>';"
        "  h += '<option value=\"\">Select Regular Sensor Type</option>';"
        "  h += '<option value=\"Flow-Meter\">Flow-Meter</option>';"
        "  h += '<option value=\"Panda_USM\">Panda USM</option>';"
        "  h += '<option value=\"Clampon\">Clampon</option>';"
        "  h += '<option value=\"Dailian\">Dailian Ultrasonic</option>';"
        "  h += '<option value=\"Piezometer\">Piezometer (Water Level)</option>';"
        "  h += '<option value=\"Level\">Level</option>';"
        "  h += '<option value=\"Radar Level\">Radar Level</option>';"
        "  h += '<option value=\"RAINGAUGE\">Rain Gauge</option>';"
        "  h += '<option value=\"BOREWELL\">Borewell</option>';"
        "  h += '<option value=\"ENERGY\">Energy Meter</option>';"
        "  h += '<option value=\"ZEST\">ZEST</option>';"
        "  h += '</select>';"
        "  h += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Choose the type of sensor to configure</small>';"
        "  h += '</div></div>';"
        "  h += '<div id=\"sensor-form-' + sensorCount + '\" style=\"display:none\">';"
        "  h += '</div>';"
        "  h += '<div style=\"margin-top:25px;padding-top:20px;border-top:1px solid #e0e0e0;text-align:center\">';"
        "  h += '<button type=\"button\" onclick=\"testNewSensorRS485(' + sensorCount + ')\" style=\"background:linear-gradient(135deg,#17a2b8,#138496);color:white;padding:12px 28px;border:none;border-radius:6px;font-weight:600;cursor:pointer;margin-right:10px;box-shadow:0 3px 8px rgba(23,162,184,0.3);transition:all 0.3s ease;font-size:15px\" onmouseover=\"this.style.transform=\\'translateY(-2px)\\';this.style.boxShadow=\\'0 5px 12px rgba(23,162,184,0.4)\\'\" onmouseout=\"this.style.transform=\\'translateY(0)\\';this.style.boxShadow=\\'0 3px 8px rgba(23,162,184,0.3)\\'\">🔍 Test RS485</button>';"
        "  h += '<button type=\"button\" onclick=\"saveSingleSensor(' + sensorCount + ')\" style=\"background:linear-gradient(135deg,#28a745,#218838);color:white;padding:12px 28px;border:none;border-radius:6px;font-weight:600;cursor:pointer;margin-right:10px;box-shadow:0 3px 8px rgba(40,167,69,0.3);transition:all 0.3s ease;font-size:15px\" onmouseover=\"this.style.transform=\\'translateY(-2px)\\';this.style.boxShadow=\\'0 5px 12px rgba(40,167,69,0.4)\\'\" onmouseout=\"this.style.transform=\\'translateY(0)\\';this.style.boxShadow=\\'0 3px 8px rgba(40,167,69,0.3)\\'\">💾 Save This Sensor</button>';"
        "  h += '<button type=\"button\" onclick=\"removeSensorForm(' + sensorCount + ')\" style=\"background:linear-gradient(135deg,#dc3545,#c82333);color:white;padding:12px 28px;border:none;border-radius:6px;font-weight:600;cursor:pointer;box-shadow:0 3px 8px rgba(220,53,69,0.3);transition:all 0.3s ease;font-size:15px\" onmouseover=\"this.style.transform=\\'translateY(-2px)\\';this.style.boxShadow=\\'0 5px 12px rgba(220,53,69,0.4)\\'\" onmouseout=\"this.style.transform=\\'translateY(0)\\';this.style.boxShadow=\\'0 3px 8px rgba(220,53,69,0.3)\\'\">❌ Cancel</button>';"
        "  h += '<div id=\"test-result-new-' + sensorCount + '\" style=\"margin-top:15px;display:none\"></div>';"
        "  h += '</div></div>';"
        "  console.log('Adding HTML to regular sensors div...');"
        "  div.innerHTML += h;"
        "  var currentSensorId = sensorCount;"
        "  sensorCount++;"
        "  setTimeout(() => {"
        "    const select = document.querySelector('select[name=\"sensor_' + currentSensorId + '_data_type\"]');"
        "    if (select) {"
        "      select.value = '';"
        "      showDataTypeInfo(select, currentSensorId);"
        "    }"
        "  }, 100);"
        "  console.log('SUCCESS: New regular sensor added. Updated count:', sensorCount);"
        "  alert('SUCCESS: New regular sensor form added!\\n\\nSTEPS:\\n1. Fill in Name and Unit ID (required fields)\\n2. Select sensor type and data type\\n3. Configure parameters as needed\\n4. Click Save This Sensor\\n\\nThis will be saved as a regular sensor!');"
        "}"
        "function addWaterQualitySensor() {"
        "  console.log('ADD WATER QUALITY SENSOR CLICKED - Current count:', sensorCount);"
        "  var div = document.getElementById('water-quality-sensors-list');"
        "  if (!div) { alert('ERROR: Water quality sensors container not found!'); console.error('water-quality-sensors-list div missing'); return; }"
        "  console.log('Found water quality sensors div, building HTML...');"
        "  var h = '<div class=\"sensor-card\" id=\"sensor-card-' + sensorCount + '\" style=\"border:2px solid #17a2b8;border-radius:8px;padding:25px;margin:20px 0;background:#ffffff;box-shadow:0 2px 4px rgba(0,0,0,0.1)\">';"
        "  h += '<h4 style=\"color:#17a2b8;margin-top:0;margin-bottom:20px;font-size:18px\">New Water Quality Sensor ' + (sensorCount+1) + '</h4>';"
        "  h += '<input type=\"hidden\" name=\"sensor_' + sensorCount + '_sensor_type\" value=\"QUALITY\">';"
        "  h += '<p style=\"color:#17a2b8;font-weight:600;margin:15px 0;padding:10px;background:#e3f2fd;border-radius:6px;font-size:14px\">[TEST] Water Quality Sensor - Automatically set to QUALITY type</p>';"
        "  h += '<div id=\"sensor-form-' + sensorCount + '\" style=\"display:block\">';"
        "  h += '<div style=\"display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-bottom:20px\">';"
        "  h += '<label style=\"font-weight:600;padding-top:10px\">Sensor Name *:</label>';"
        "  h += '<div><input type=\"text\" name=\"sensor_' + sensorCount + '_name\" placeholder=\"e.g., Tank 1 Water Quality\" style=\"width:100%;padding:10px;border:2px solid #dc3545;border-radius:6px;font-size:15px\" required>';"
        "  h += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Display name for this sensor</small></div>';"
        "  h += '<label style=\"font-weight:600;padding-top:10px\">Unit ID *:</label>';"
        "  h += '<div><input type=\"text\" name=\"sensor_' + sensorCount + '_unit_id\" placeholder=\"e.g., TFG2235Q\" style=\"width:100%;padding:10px;border:2px solid #dc3545;border-radius:6px;font-size:15px\" required>';"
        "  h += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Unique identifier for this water quality sensor</small></div>';"
        "  h += '<label style=\"font-weight:600;padding-top:10px\">Description:</label>';"
        "  h += '<div><input type=\"text\" name=\"sensor_' + sensorCount + '_description\" placeholder=\"e.g., Main tank water quality monitoring\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "  h += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Optional description of sensor location or purpose</small></div>';"
        "  h += '<label style=\"font-weight:600;padding-top:10px\">Baud Rate:</label>';"
        "  h += '<div><select name=\"sensor_' + sensorCount + '_baud_rate\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "  h += '<option value=\"9600\">9600</option><option value=\"19200\">19200</option><option value=\"38400\">38400</option><option value=\"115200\">115200</option>';"
        "  h += '</select>';"
        "  h += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Serial communication speed</small></div>';"
        "  h += '<label style=\"font-weight:600;padding-top:10px\">Parity:</label>';"
        "  h += '<div><select name=\"sensor_' + sensorCount + '_parity\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "  h += '<option value=\"none\">None</option><option value=\"even\">Even</option><option value=\"odd\">Odd</option>';"
        "  h += '</select>';"
        "  h += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Error checking method</small></div></div>';"
        "  h += '<p style=\"color:#28a745;font-size:14px;margin:15px 0;padding:10px;background:#f0fff4;border:1px solid #28a745;border-radius:4px\">';"
        "  h += '[INFO] <strong>Main Configuration:</strong> Set the shared Unit ID and communication settings. Individual parameter readings (pH, TDS, etc.) are configured as sub-sensors below.';"
        "  h += '</p>';"
        "  h += '<div style=\"margin-top:20px;padding:15px;background:#f0f8ff;border:2px dashed #17a2b8;border-radius:8px\">';"
        "  h += '<h4 style=\"color:#17a2b8;margin-top:0\">[PARAM] Sub-Sensors - Water Quality Parameters</h4>';"
        "  h += '<p style=\"color:#666;font-size:12px;margin:5px 0\">Add water quality parameters that will be grouped under this Unit ID in JSON format:</p>';"
        "  h += '<p style=\"color:#28a745;font-size:11px;font-family:monospace;background:#f8f8f8;padding:8px;border-radius:4px;margin:5px 0\">';"
        "  h += '{\"params_data\":{\"pH\":7,\"TDS\":100,\"Temp\":32,\"HUMIDITY\":65,\"BOD\":15,\"COD\":45},\"type\":\"QUALITY\",\"unit_id\":\"TFG2235Q\"}';"
        "  h += '</p>';"
        "  h += '<div id=\"sub-sensors-' + sensorCount + '\" style=\"margin:10px 0\"></div>';"
        "  h += '<button type=\"button\" onclick=\"addSubSensorToQualitySensor(' + sensorCount + ')\" style=\"background:#28a745;color:white;padding:8px 16px;border:none;border-radius:4px;font-size:14px;cursor:pointer\">➕ Add Sub-Sensor</button>';"
        "  h += '</div>';"
        "  h += '<div style=\"margin-top:25px;padding-top:20px;border-top:1px solid #e0e0e0;text-align:center\">';"
        "  h += '<button type=\"button\" onclick=\"testNewSensorRS485(' + sensorCount + ')\" style=\"background:linear-gradient(135deg,#17a2b8,#138496);color:white;padding:12px 28px;border:none;border-radius:6px;font-weight:600;cursor:pointer;margin-right:10px;box-shadow:0 3px 8px rgba(23,162,184,0.3);transition:all 0.3s ease;font-size:15px\" onmouseover=\"this.style.transform=\\'translateY(-2px)\\';this.style.boxShadow=\\'0 5px 12px rgba(23,162,184,0.4)\\'\" onmouseout=\"this.style.transform=\\'translateY(0)\\';this.style.boxShadow=\\'0 3px 8px rgba(23,162,184,0.3)\\'\">🔍 Test RS485</button>';"
        "  h += '<button type=\"button\" onclick=\"saveSingleSensor(' + sensorCount + ')\" style=\"background:linear-gradient(135deg,#28a745,#218838);color:white;padding:12px 28px;border:none;border-radius:6px;font-weight:600;cursor:pointer;margin-right:10px;box-shadow:0 3px 8px rgba(40,167,69,0.3);transition:all 0.3s ease;font-size:15px\" onmouseover=\"this.style.transform=\\'translateY(-2px)\\';this.style.boxShadow=\\'0 5px 12px rgba(40,167,69,0.4)\\'\" onmouseout=\"this.style.transform=\\'translateY(0)\\';this.style.boxShadow=\\'0 3px 8px rgba(40,167,69,0.3)\\'\">💾 Save Water Quality Sensor</button>';"
        "  h += '<button type=\"button\" onclick=\"removeSensorForm(' + sensorCount + ')\" style=\"background:linear-gradient(135deg,#dc3545,#c82333);color:white;padding:12px 28px;border:none;border-radius:6px;font-weight:600;cursor:pointer;box-shadow:0 3px 8px rgba(220,53,69,0.3);transition:all 0.3s ease;font-size:15px\" onmouseover=\"this.style.transform=\\'translateY(-2px)\\';this.style.boxShadow=\\'0 5px 12px rgba(220,53,69,0.4)\\'\" onmouseout=\"this.style.transform=\\'translateY(0)\\';this.style.boxShadow=\\'0 3px 8px rgba(220,53,69,0.3)\\'\">❌ Cancel</button>';"
        "  h += '<div id=\"test-result-new-' + sensorCount + '\" style=\"margin-top:15px;display:none\"></div>';"
        "  h += '</div></div>';"
        "  console.log('Adding HTML to water quality sensors div...');"
        "  div.innerHTML += h;"
        "  var currentSensorId = sensorCount;"
        "  sensorCount++;"
        "  console.log('SUCCESS: New water quality sensor added. Updated count:', sensorCount);"
        "  alert('SUCCESS: New water quality sensor form added!\\n\\nSTEPS:\\n1. Fill in Name and Unit ID (required fields)\\n2. Configure Modbus parameters\\n3. Set scale factor for proper value conversion\\n4. Click Save Water Quality Sensor\\n\\nThis will be saved as a water quality sensor!');"
        "}"
        "var qualityParameterTypes = ["
        "  {key: 'pH', name: 'pH', units: 'pH', description: 'Acidity/Alkalinity measurement'},"
        "  {key: 'Temp', name: 'Temp', units: 'degC', description: 'Water temperature sensor'},"
        "  {key: 'HUMIDITY', name: 'HUMIDITY', units: '%RH', description: 'Relative humidity measurement'},"
        "  {key: 'TDS', name: 'TDS', units: 'ppm', description: 'Total dissolved solids concentration'},"
        "  {key: 'TSS', name: 'TSS', units: 'mg/L', description: 'Total suspended solids'},"
        "  {key: 'BOD', name: 'BOD', units: 'mg/L', description: 'Biochemical oxygen demand'},"
        "  {key: 'COD', name: 'COD', units: 'mg/L', description: 'Chemical oxygen demand'},"
        "  {key: 'conductivity', name: 'Conductivity', units: 'µS/cm', description: 'Electrical conductivity'},"
        "  {key: 'do', name: 'Dissolved Oxygen', units: 'mg/L', description: 'Oxygen content in water'},"
        "  {key: 'orp', name: 'ORP (Oxidation Reduction Potential)', units: 'mV', description: 'Oxidation potential'},"
        "  {key: 'ammonia', name: 'Ammonia', units: 'mg/L', description: 'Ammonia concentration'},"
        "  {key: 'nitrate', name: 'Nitrate', units: 'mg/L', description: 'Nitrate levels'},"
        "  {key: 'phosphate', name: 'Phosphate', units: 'mg/L', description: 'Phosphate concentration'},"
        "  {key: 'chlorine', name: 'Chlorine', units: 'mg/L', description: 'Chlorine disinfectant level'},"
        "  {key: 'salinity', name: 'Salinity', units: 'ppt', description: 'Salt concentration'}"
        "];"
        "function addSubSensorToQualitySensor(sensorIndex) {"
        "  var subSensorDiv = document.getElementById('sub-sensors-' + sensorIndex);"
        "  if (!subSensorDiv) {"
        "    alert('ERROR: Sub-sensors container not found');"
        "    return;"
        "  }"
        "  "
        "  var subSensorCount = subSensorDiv.querySelectorAll('.sub-sensor').length;"
        "  var subSensorId = 'sub-sensor-' + sensorIndex + '-' + subSensorCount;"
        "  "
        "  var h = '<div class=\"sub-sensor\" id=\"' + subSensorId + '\" style=\"border:1px solid #28a745;padding:10px;margin:10px 0;background:#f8fff8;border-radius:5px\">';"
        "  h += '<h5 style=\"color:#28a745;margin-top:0\">[PARAM] Sub-Sensor ' + (subSensorCount + 1) + '</h5>';"
        "  h += '<div style=\"display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin:10px 0\">';"
        "  h += '<div><label style=\"font-weight:bold\">Parameter Type:</label><br><select name=\"sensor_' + sensorIndex + '_sub_' + subSensorCount + '_parameter\" style=\"width:100%;padding:5px\">';"
        "  h += '<option value=\"\">Select Parameter</option>';"
        "  for (var i = 0; i < qualityParameterTypes.length; i++) {"
        "    var param = qualityParameterTypes[i];"
        "    h += '<option value=\"' + param.key + '\">' + param.name + ' (' + param.units + ')</option>';"
        "  }"
        "  h += '</select></div>';"
        "  h += '<div><label style=\"font-weight:bold\">Slave ID:</label><br><input type=\"number\" name=\"sensor_' + sensorIndex + '_sub_' + subSensorCount + '_slave_id\" value=\"1\" min=\"1\" max=\"247\" style=\"width:100%;padding:5px\"></div>';"
        "  h += '<div><label style=\"font-weight:bold\">Register:</label><br><input type=\"number\" name=\"sensor_' + sensorIndex + '_sub_' + subSensorCount + '_register\" value=\"1\" min=\"0\" max=\"65535\" style=\"width:100%;padding:5px\"></div>';"
        "  h += '</div>';"
        "  h += '<div style=\"display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:10px 0\">';"
        "  h += '<div><label style=\"font-weight:bold\">Quantity:</label><br><input type=\"number\" name=\"sensor_' + sensorIndex + '_sub_' + subSensorCount + '_quantity\" value=\"1\" min=\"1\" max=\"10\" style=\"width:100%;padding:5px\"></div>';"
        "  h += '<div><label style=\"font-weight:bold\">Register Type:</label><br><select name=\"sensor_' + sensorIndex + '_sub_' + subSensorCount + '_register_type\" style=\"width:100%;padding:5px\">';"
        "  h += '<option value=\"HOLDING_REGISTER\" selected>Holding Register</option>';"
        "  h += '<option value=\"INPUT_REGISTER\">Input Register</option>';"
        "  h += '</select></div>';"
        "  h += '</div>';"
        "  h += '<div style=\"display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:10px 0\">';"
        "  h += '<div><label style=\"font-weight:bold\">Data Type:</label><br><select name=\"sensor_' + sensorIndex + '_sub_' + subSensorCount + '_data_type\" style=\"width:100%;padding:5px\">';"
        "  h += '<option value=\"\">Select Data Type</option>';"
        "  h += '<optgroup label=\"8-bit Integer Types\">';"
        "  h += '<option value=\"INT8\">INT8 - 8-bit Signed</option>';"
        "  h += '<option value=\"UINT8\">UINT8 - 8-bit Unsigned</option>';"
        "  h += '</optgroup>';"
        "  h += '<optgroup label=\"16-bit Integer Types\">';"
        "  h += '<option value=\"INT16\">INT16 - 16-bit Signed</option>';"
        "  h += '<option value=\"UINT16\">UINT16 - 16-bit Unsigned</option>';"
        "  h += '</optgroup>';"
        "  h += '<optgroup label=\"32-bit Integer Types\">';"
        "  h += '<option value=\"INT32_ABCD\">INT32_ABCD - Big Endian</option>';"
        "  h += '<option value=\"INT32_DCBA\">INT32_DCBA - Little Endian</option>';"
        "  h += '<option value=\"INT32_BADC\">INT32_BADC - Mid-Big Endian</option>';"
        "  h += '<option value=\"INT32_CDAB\">INT32_CDAB - Mid-Little Endian</option>';"
        "  h += '<option value=\"UINT32_ABCD\">UINT32_ABCD - Big Endian</option>';"
        "  h += '<option value=\"UINT32_DCBA\">UINT32_DCBA - Little Endian</option>';"
        "  h += '<option value=\"UINT32_BADC\">UINT32_BADC - Mid-Big Endian</option>';"
        "  h += '<option value=\"UINT32_CDAB\">UINT32_CDAB - Mid-Little Endian</option>';"
        "  h += '</optgroup>';"
        "  h += '<optgroup label=\"32-bit Float Types\">';"
        "  h += '<option value=\"FLOAT32_ABCD\">FLOAT32_ABCD - Big Endian</option>';"
        "  h += '<option value=\"FLOAT32_DCBA\">FLOAT32_DCBA - Little Endian</option>';"
        "  h += '<option value=\"FLOAT32_BADC\">FLOAT32_BADC - Mid-Big Endian</option>';"
        "  h += '<option value=\"FLOAT32_CDAB\">FLOAT32_CDAB - Mid-Little Endian</option>';"
        "  h += '</optgroup>';"
        "  h += '<optgroup label=\"64-bit Integer Types\">';"
        "  h += '<option value=\"INT64_12345678\">INT64_12345678 - Big Endian</option>';"
        "  h += '<option value=\"INT64_87654321\">INT64_87654321 - Little Endian</option>';"
        "  h += '<option value=\"INT64_21436587\">INT64_21436587 - Mid-Big Endian</option>';"
        "  h += '<option value=\"INT64_78563412\">INT64_78563412 - Mid-Little Endian</option>';"
        "  h += '<option value=\"UINT64_12345678\">UINT64_12345678 - Big Endian</option>';"
        "  h += '<option value=\"UINT64_87654321\">UINT64_87654321 - Little Endian</option>';"
        "  h += '<option value=\"UINT64_21436587\">UINT64_21436587 - Mid-Big Endian</option>';"
        "  h += '<option value=\"UINT64_78563412\">UINT64_78563412 - Mid-Little Endian</option>';"
        "  h += '</optgroup>';"
        "  h += '<optgroup label=\"64-bit Float Types\">';"
        "  h += '<option value=\"FLOAT64_12345678\">FLOAT64_12345678 - Big Endian</option>';"
        "  h += '<option value=\"FLOAT64_87654321\">FLOAT64_87654321 - Little Endian</option>';"
        "  h += '<option value=\"FLOAT64_21436587\">FLOAT64_21436587 - Mid-Big Endian</option>';"
        "  h += '<option value=\"FLOAT64_78563412\">FLOAT64_78563412 - Mid-Little Endian</option>';"
        "  h += '</optgroup>';"
        "  h += '<optgroup label=\"Special Types\">';"
        "  h += '<option value=\"ASCII\">ASCII String</option>';"
        "  h += '<option value=\"HEX\">Hexadecimal</option>';"
        "  h += '<option value=\"BOOL\">Boolean</option>';"
        "  h += '<option value=\"PDU\">PDU (Protocol Data Unit)</option>';"
        "  h += '</optgroup>';"
        "  h += '</select></div>';"
        "  h += '<div><label style=\"font-weight:bold\">Scale Factor:</label><br><input type=\"number\" name=\"sensor_' + sensorIndex + '_sub_' + subSensorCount + '_scale\" value=\"1\" step=\"0.001\" style=\"width:100%;padding:5px\"></div>';"
        "  h += '</div>';"
        "  h += '<div style=\"text-align:right;margin-top:10px\">';"
        "  h += '<button type=\"button\" onclick=\"saveSubSensor(\\'' + subSensorId + '\\')\" style=\"background:#28a745;color:white;padding:5px 15px;border:none;border-radius:3px;cursor:pointer;margin-right:5px\">[SAVE] Save</button>';"
        "  h += '<button type=\"button\" onclick=\"removeSubSensor(\\'' + subSensorId + '\\')\" style=\"background:#dc3545;color:white;padding:5px 15px;border:none;border-radius:3px;cursor:pointer\">[ERROR] Remove</button>';"
        "  h += '</div>';"
        "  h += '</div>';"
        "  "
        "  subSensorDiv.innerHTML += h;"
        "  alert('Sub-sensor added! Configure its parameters and save the water quality sensor.');"
        "}"
        "function removeSubSensor(subSensorId) {"
        "  var subSensor = document.getElementById(subSensorId);"
        "  if (subSensor && confirm('Remove this sub-sensor?')) {"
        "    subSensor.remove();"
        "  }"
        "}");
    
    // Sensor type form update function
    httpd_resp_sendstr_chunk(req,
        "function updateSensorForm(sensorId) {"
        "const sensorType = document.querySelector('select[name=\"sensor_' + sensorId + '_sensor_type\"]').value;"
        "const formDiv = document.getElementById('sensor-form-' + sensorId);"
        "if (!sensorType) {"
        "formDiv.style.display = 'none';"
        "return;"
        "}"
        "let formHtml = '';"
        "formHtml += '<div style=\"display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-top:20px\">';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Name *:</label>';"
        "formHtml += '<div><input type=\"text\" name=\"sensor_' + sensorId + '_name\" placeholder=\"e.g., Flow Meter 1\" style=\"width:100%;padding:10px;border:2px solid #dc3545;border-radius:6px;font-size:15px\" required>';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Sensor display name</small></div>';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Unit ID *:</label>';"
        "formHtml += '<div><input type=\"text\" name=\"sensor_' + sensorId + '_unit_id\" placeholder=\"e.g., FG1234\" style=\"width:100%;padding:10px;border:2px solid #dc3545;border-radius:6px;font-size:15px\" required>';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Unique identifier for this sensor</small></div>';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Slave ID:</label>';"
        "formHtml += '<div><input type=\"number\" name=\"sensor_' + sensorId + '_slave_id\" value=\"1\" min=\"1\" max=\"247\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Modbus slave address (1-247)</small></div>';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Register Address:</label>';"
        "formHtml += '<div><input type=\"number\" name=\"sensor_' + sensorId + '_register_address\" value=\"0\" min=\"0\" max=\"65535\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Starting register to read (0-65535)</small></div>';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Quantity:</label>';"
        "formHtml += '<div><input type=\"number\" name=\"sensor_' + sensorId + '_quantity\" value=\"1\" min=\"1\" max=\"125\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Number of registers to read (1-125)</small></div>';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Register Type:</label>';"
        "formHtml += '<div><select name=\"sensor_' + sensorId + '_register_type\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "formHtml += '<option value=\"HOLDING\">Holding Registers (03) - Read/Write</option>';"
        "formHtml += '<option value=\"INPUT\">Input Registers (04) - Read Only</option>';"
        "formHtml += '</select>';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Modbus register type</small></div>';"
        "formHtml += '</div>';"
        "if (sensorType !== 'ZEST' && sensorType !== 'Clampon' && sensorType !== 'Dailian' && sensorType !== 'Piezometer') {"
        "formHtml += '<div style=\"display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-top:20px\">';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Data Type:</label>';"
        "formHtml += '<div><select name=\"sensor_' + sensorId + '_data_type\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\" onchange=\"showDataTypeInfo(this, ' + sensorId + ')\">';"
        "formHtml += '<option value=\"\" disabled selected>-- Select Data Type (Required) --</option>';"
        "formHtml += '<optgroup label=\"8-bit Formats (0.5 register)\">';"
        "formHtml += '<option value=\"INT8\">8-bit INT (-128 to 127)</option>';"
        "formHtml += '<option value=\"UINT8\">8-bit UINT (0 to 255)</option>';"
        "formHtml += '</optgroup>';"
        "formHtml += '<optgroup label=\"16-bit Formats (1 register)\">';"
        "formHtml += '<option value=\"INT16_HI\">16-bit INT, high byte first</option>';"
        "formHtml += '<option value=\"INT16_LO\">16-bit INT, low byte first</option>';"
        "formHtml += '<option value=\"UINT16_HI\">16-bit UINT, high byte first</option>';"
        "formHtml += '<option value=\"UINT16_LO\">16-bit UINT, low byte first</option>';"
        "formHtml += '</optgroup>';"
        "formHtml += '<optgroup label=\"32-bit Float Formats (2 registers)\">';"
        "formHtml += '<option value=\"FLOAT32_1234\">32-bit float, Byte order 1,2,3,4</option>';"
        "formHtml += '<option value=\"FLOAT32_4321\">32-bit float, Byte order 4,3,2,1</option>';"
        "formHtml += '<option value=\"FLOAT32_2143\">32-bit float, Byte order 2,1,4,3</option>';"
        "formHtml += '<option value=\"FLOAT32_3412\">32-bit float, Byte order 3,4,1,2</option>';"
        "formHtml += '</optgroup>';"
        "formHtml += '<optgroup label=\"32-bit Integer Formats (2 registers)\">';"
        "formHtml += '<option value=\"INT32_1234\">32-bit INT, Byte order 1,2,3,4</option>';"
        "formHtml += '<option value=\"INT32_4321\">32-bit INT, Byte order 4,3,2,1</option>';"
        "formHtml += '<option value=\"INT32_2143\">32-bit INT, Byte order 2,1,4,3</option>';"
        "formHtml += '<option value=\"INT32_3412\">32-bit INT, Byte order 3,4,1,2</option>';"
        "formHtml += '<option value=\"UINT32_1234\">32-bit UINT, Byte order 1,2,3,4</option>';"
        "formHtml += '<option value=\"UINT32_4321\">32-bit UINT, Byte order 4,3,2,1</option>';"
        "formHtml += '<option value=\"UINT32_2143\">32-bit UINT, Byte order 2,1,4,3</option>';"
        "formHtml += '<option value=\"UINT32_3412\">32-bit UINT, Byte order 3,4,1,2</option>';"
        "formHtml += '</optgroup>';"
        "formHtml += '<optgroup label=\"64-bit Integer Formats (4 registers)\">';"
        "formHtml += '<option value=\"INT64_12345678\">64-bit INT, Byte order 1,2,3,4,5,6,7,8</option>';"
        "formHtml += '<option value=\"INT64_87654321\">64-bit INT, Byte order 8,7,6,5,4,3,2,1</option>';"
        "formHtml += '<option value=\"INT64_21436587\">64-bit INT, Byte order 2,1,4,3,6,5,8,7</option>';"
        "formHtml += '<option value=\"INT64_78563412\">64-bit INT, Byte order 7,8,5,6,3,4,1,2</option>';"
        "formHtml += '<option value=\"UINT64_12345678\">64-bit UINT, Byte order 1,2,3,4,5,6,7,8</option>';"
        "formHtml += '<option value=\"UINT64_87654321\">64-bit UINT, Byte order 8,7,6,5,4,3,2,1</option>';"
        "formHtml += '<option value=\"UINT64_21436587\">64-bit UINT, Byte order 2,1,4,3,6,5,8,7</option>';"
        "formHtml += '<option value=\"UINT64_78563412\">64-bit UINT, Byte order 7,8,5,6,3,4,1,2</option>';"
        "formHtml += '</optgroup>';"
        "formHtml += '<optgroup label=\"64-bit Float Formats (4 registers)\">';"
        "formHtml += '<option value=\"FLOAT64_12345678\">64-bit float, Byte order 1,2,3,4,5,6,7,8</option>';"
        "formHtml += '<option value=\"FLOAT64_87654321\">64-bit float, Byte order 8,7,6,5,4,3,2,1</option>';"
        "formHtml += '<option value=\"FLOAT64_21436587\">64-bit float, Byte order 2,1,4,3,6,5,8,7</option>';"
        "formHtml += '<option value=\"FLOAT64_78563412\">64-bit float, Byte order 7,8,5,6,3,4,1,2</option>';"
        "formHtml += '</optgroup>';"
        "formHtml += '<optgroup label=\"Special Formats\">';"
        "formHtml += '<option value=\"ASCII\">ASCII String</option>';"
        "formHtml += '<option value=\"HEX\">Hexadecimal</option>';"
        "formHtml += '<option value=\"BOOL\">Boolean (True/False)</option>';"
        "formHtml += '<option value=\"PDU\">PDU (Protocol Data Unit)</option>';"
        "formHtml += '</optgroup></select>';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Select the format of data returned by sensor</small></div>';"
        "formHtml += '</div>';"
        "formHtml += '<div id=\"datatype-info-' + sensorId + '\" style=\"background:#e3f2fd;padding:15px;margin:20px 0;border-radius:6px;font-size:13px;display:none\"></div>';"
        "} else if (sensorType === 'ZEST') {"
        "formHtml += '<input type=\"hidden\" name=\"sensor_' + sensorId + '_data_type\" value=\"ZEST_FIXED\">';"
        "formHtml += '<p style=\"color:#28a745;font-size:12px;margin:10px 0\"><strong>Data Format:</strong> Fixed - INT32_BE + INT32_LE_SWAP</p>';"
        "} else if (sensorType === 'Clampon') {"
        "formHtml += '<input type=\"hidden\" name=\"sensor_' + sensorId + '_data_type\" value=\"UINT32_3412\">';"
        "formHtml += '<p style=\"color:#28a745;font-size:12px;margin:10px 0\"><strong>Data Format:</strong> Fixed - UINT32_3412 (CDAB byte order)</p>';"
        "formHtml += '<p style=\"color:#007bff;font-size:11px;margin:5px 0\"><em>Clampon defaults: Register 8, Quantity 2 - reads as 32-bit unsigned integer</em></p>';"
        "} else if (sensorType === 'Dailian') {"
        "formHtml += '<input type=\"hidden\" name=\"sensor_' + sensorId + '_data_type\" value=\"UINT32_3412\">';"
        "formHtml += '<p style=\"color:#28a745;font-size:12px;margin:10px 0\"><strong>Data Format:</strong> Fixed - UINT32_3412 (CDAB byte order)</p>';"
        "formHtml += '<p style=\"color:#007bff;font-size:11px;margin:5px 0\"><em>Dailian defaults: Register 8, Quantity 2 - value has 3 implicit decimals (863 = 0.863 m³/h)</em></p>';"
        "} else if (sensorType === 'Piezometer') {"
        "formHtml += '<input type=\"hidden\" name=\"sensor_' + sensorId + '_data_type\" value=\"UINT16_HI\">';"
        "formHtml += '<p style=\"color:#28a745;font-size:12px;margin:10px 0\"><strong>Data Format:</strong> Fixed - UINT16_HI (16-bit unsigned integer)</p>';"
        "formHtml += '<p style=\"color:#007bff;font-size:11px;margin:5px 0\"><em>Piezometer defaults: Register 10, Quantity 1, Scale 0.01 - reads water level in mwc (e.g., 14542 × 0.01 = 145.42 mwc)</em></p>';"
        "}"
        "formHtml += '<div style=\"display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-top:20px\">';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Baud Rate:</label>';"
        "formHtml += '<div><select name=\"sensor_' + sensorId + '_baud_rate\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "formHtml += '<option value=\"2400\">2400 bps</option><option value=\"4800\">4800 bps</option><option value=\"9600\" selected>9600 bps</option><option value=\"19200\">19200 bps</option><option value=\"38400\">38400 bps</option><option value=\"57600\">57600 bps</option><option value=\"115200\">115200 bps</option></select>';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Serial communication speed</small></div>';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Parity:</label>';"
        "formHtml += '<div><select name=\"sensor_' + sensorId + '_parity\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "formHtml += '<option value=\"none\" selected>None</option><option value=\"even\">Even</option><option value=\"odd\">Odd</option></select>';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Error checking method</small></div>';"
        "formHtml += '</div>';"
        "if (sensorType === 'Flow-Meter') {"
        "formHtml += '<div style=\"display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-top:20px\">';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Scale Factor:</label>';"
        "formHtml += '<div><input type=\"number\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\" step=\"any\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Multiplier applied to raw sensor value (e.g., 0.1 for /10, 1.0 for no scaling)</small></div></div>';"
        "} else if (sensorType === 'Panda_USM') {"
        "formHtml += '<input type=\"hidden\" name=\"sensor_' + sensorId + '_data_type\" value=\"FLOAT64_12345678\">';"
        "formHtml += '<div style=\"display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-top:20px\">';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Scale Factor:</label>';"
        "formHtml += '<div><input type=\"number\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\" step=\"any\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Multiplier for positive volume (usually 1.0)</small></div></div>';"
        "formHtml += '<p style=\"color:#28a745;font-size:12px;margin:10px 0\"><strong>Data Format:</strong> Fixed - FLOAT64_12345678 (Big-Endian Double)</p>';"
        "formHtml += '<p style=\"color:#007bff;font-size:11px;margin:5px 0\"><em>Panda USM defaults: Register 8 (Positive Volume), Quantity 4 (64-bit Double)</em></p>';"
        "} else if (sensorType === 'Level') {"
        "formHtml += '<div style=\"display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-top:20px\">';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Sensor Height:</label>';"
        "formHtml += '<div><input type=\"number\" name=\"sensor_' + sensorId + '_sensor_height\" value=\"0\" step=\"any\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Physical height of the sensor from bottom (meters or your unit)</small></div>';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Max Water Level:</label>';"
        "formHtml += '<div><input type=\"number\" name=\"sensor_' + sensorId + '_max_water_level\" value=\"0\" step=\"any\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Maximum water level for calculation (meters or your unit)</small></div></div>';"
        "formHtml += '<input type=\"hidden\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\">';"
        "} else if (sensorType === 'Radar Level') {"
        "formHtml += '<div style=\"display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-top:20px\">';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Max Water Level *:</label>';"
        "formHtml += '<div><input type=\"number\" name=\"sensor_' + sensorId + '_max_water_level\" value=\"0\" step=\"any\" style=\"width:100%;padding:10px;border:2px solid #dc3545;border-radius:6px;font-size:15px\" required>';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Maximum water level for percentage calculation (meters or your unit)</small></div></div>';"
        "formHtml += '<input type=\"hidden\" name=\"sensor_' + sensorId + '_sensor_height\" value=\"0\">';"
        "formHtml += '<input type=\"hidden\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\">';"
        "} else if (sensorType === 'ENERGY') {"
        "formHtml += '<div style=\"display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-top:20px\">';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Meter Type *:</label>';"
        "formHtml += '<div><input type=\"text\" name=\"sensor_' + sensorId + '_meter_type\" placeholder=\"e.g., Power Meter, Electric Meter\" style=\"width:100%;padding:10px;border:2px solid #dc3545;border-radius:6px;font-size:15px\" required>';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Type of energy meter (will be used as meter identifier in JSON)</small></div></div>';"
        "formHtml += '<input type=\"hidden\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\">';"
        "} else if (sensorType === 'RAINGAUGE') {"
        "formHtml += '<div style=\"display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-top:20px\">';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Scale Factor:</label>';"
        "formHtml += '<div><input type=\"number\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\" step=\"any\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Rain gauge scaling factor (e.g., 0.1 for mm/tips, 1.0 for direct reading)</small></div></div>';"
        "} else if (sensorType === 'BOREWELL') {"
        "formHtml += '<div style=\"display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-top:20px\">';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Scale Factor:</label>';"
        "formHtml += '<div><input type=\"number\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\" step=\"any\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Scaling multiplier for borewell measurements</small></div></div>';"
        "} else if (sensorType === 'ZEST') {"
        "formHtml += '<div style=\"display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-top:20px\">';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Scale Factor:</label>';"
        "formHtml += '<div><input type=\"number\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\" step=\"any\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Applied to both INT32 values</small></div></div>';"
        "formHtml += '<p style=\"color:#007bff;font-size:11px;margin:5px 0\"><em>ZEST defaults: Register 4121, Quantity 4 - First 2 as INT32_BE, next 2 as INT32_LE_SWAP, then sums them</em></p>';"
        "formHtml += '<p style=\"color:#28a745;font-size:10px;margin:5px 0\"><strong>Fixed format - no data type selection needed</strong></p>';"
        "} else if (sensorType === 'Clampon') {"
        "formHtml += '<div style=\"display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-top:20px\">';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Scale Factor:</label>';"
        "formHtml += '<div><input type=\"number\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\" step=\"any\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Multiplier for raw value</small></div></div>';"
        "formHtml += '<p style=\"color:#28a745;font-size:13px;margin:15px 0;padding:10px;background:#f0fff4;border:1px solid #28a745;border-radius:4px\"><strong>Fixed format - UINT32_3412 (CDAB) - no data type selection needed</strong></p>';"
        "} else if (sensorType === 'Dailian') {"
        "formHtml += '<div style=\"display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-top:20px\">';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Scale Factor:</label>';"
        "formHtml += '<div><input type=\"number\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\" step=\"any\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">Value has 3 implicit decimals: 863 = 0.863</small></div></div>';"
        "formHtml += '<p style=\"color:#28a745;font-size:13px;margin:15px 0;padding:10px;background:#f0fff4;border:1px solid #28a745;border-radius:4px\"><strong>Fixed format - UINT32_3412 (CDAB) - no data type selection needed</strong></p>';"
        "} else if (sensorType === 'Piezometer') {"
        "formHtml += '<div style=\"display:grid;grid-template-columns:180px 1fr;gap:20px;align-items:start;margin-top:20px\">';"
        "formHtml += '<label style=\"font-weight:600;padding-top:10px\">Scale Factor:</label>';"
        "formHtml += '<div><input type=\"number\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"0.01\" step=\"any\" style=\"width:100%;padding:10px;border:1px solid #e0e0e0;border-radius:6px;font-size:15px\">';"
        "formHtml += '<small style=\"color:#888;display:block;margin-top:5px;font-size:13px\">0.01 for 2 implicit decimals: 14542 = 145.42</small></div></div>';"
        "formHtml += '<p style=\"color:#28a745;font-size:13px;margin:15px 0;padding:10px;background:#f0fff4;border:1px solid #28a745;border-radius:4px\"><strong>Fixed format - UINT16_HI (16-bit) - no data type selection needed</strong></p>';"
        "}"
        "formHtml += '<br><p style=\"color:#dc3545;font-size:12px;margin:5px 0\"><strong>* Required fields</strong> - Name and Unit ID must be filled</p>';"
        "formDiv.innerHTML = formHtml;"
        "formDiv.style.display = 'block';"
        "if (sensorType === 'Clampon') {"
        "const quantityInput = document.querySelector('input[name=\"sensor_' + sensorId + '_quantity\"]');"
        "const regAddrInput = document.querySelector('input[name=\"sensor_' + sensorId + '_register_address\"]');"
        "if (quantityInput) quantityInput.value = '2';"
        "if (regAddrInput) regAddrInput.value = '8';"
        "} else if (sensorType === 'Dailian') {"
        "const quantityInput = document.querySelector('input[name=\"sensor_' + sensorId + '_quantity\"]');"
        "const regAddrInput = document.querySelector('input[name=\"sensor_' + sensorId + '_register_address\"]');"
        "const scaleInput = document.querySelector('input[name=\"sensor_' + sensorId + '_scale_factor\"]');"
        "if (quantityInput) quantityInput.value = '2';"
        "if (regAddrInput) regAddrInput.value = '8';"
        "if (scaleInput) scaleInput.value = '1.0';"
        "} else if (sensorType === 'Piezometer') {"
        "const quantityInput = document.querySelector('input[name=\"sensor_' + sensorId + '_quantity\"]');"
        "const regAddrInput = document.querySelector('input[name=\"sensor_' + sensorId + '_register_address\"]');"
        "const scaleInput = document.querySelector('input[name=\"sensor_' + sensorId + '_scale_factor\"]');"
        "if (quantityInput) quantityInput.value = '1';"
        "if (regAddrInput) regAddrInput.value = '10';"
        "if (scaleInput) scaleInput.value = '0.01';"
        "} else if (sensorType === 'Panda_USM') {"
        "const quantityInput = document.querySelector('input[name=\"sensor_' + sensorId + '_quantity\"]');"
        "const regAddrInput = document.querySelector('input[name=\"sensor_' + sensorId + '_register_address\"]');"
        "const scaleInput = document.querySelector('input[name=\"sensor_' + sensorId + '_scale_factor\"]');"
        "if (quantityInput) quantityInput.value = '4';"
        "if (regAddrInput) regAddrInput.value = '8';"
        "if (scaleInput) scaleInput.value = '1.0';"
        "} else if (sensorType === 'ZEST') {"
        "const quantityInput = document.querySelector('input[name=\"sensor_' + sensorId + '_quantity\"]');"
        "const regAddrInput = document.querySelector('input[name=\"sensor_' + sensorId + '_register_address\"]');"
        "if (quantityInput) quantityInput.value = '4';"
        "if (regAddrInput) regAddrInput.value = '4121';"
        "}"
        "}");
    
    
    // Data type information and helper functions
    httpd_resp_sendstr_chunk(req,
        "function showDataTypeInfo(selectElement, sensorId) {"
        "const infoDiv = document.getElementById('datatype-info-' + sensorId);"
        "const dataType = selectElement.value;"
        "let infoHtml = '';"
        "if(dataType.startsWith('UINT16')) {"
        "const endian = dataType.includes('BE') ? 'Big Endian' : 'Little Endian';"
        "infoHtml = '<strong>' + dataType + ' - ' + endian + ' 16-bit Unsigned</strong><br>- Range: 0 to 65,535<br>- Byte Order: ' + (dataType.includes('BE') ? 'AB (High-Low)' : 'BA (Low-High)') + '<br>- Uses: Counters, status flags, small measurements<br>- Registers: 1';"
        "} else if(dataType.startsWith('INT16')) {"
        "const endian = dataType.includes('BE') ? 'Big Endian' : 'Little Endian';"
        "infoHtml = '<strong>' + dataType + ' - ' + endian + ' 16-bit Signed</strong><br>- Range: -32,768 to 32,767<br>- Byte Order: ' + (dataType.includes('BE') ? 'AB (High-Low)' : 'BA (Low-High)') + '<br>- Uses: Temperature, pressure differences, signed measurements<br>- Registers: 1';"
        "} else if(dataType.startsWith('UINT32')) {"
        "const order = dataType.split('_')[1];"
        "let orderDesc = '';"
        "switch(order) {"
        "case 'ABCD': orderDesc = 'Big Endian - Reg0(AB) + Reg1(CD)'; break;"
        "case 'DCBA': orderDesc = 'Little Endian - Completely byte-swapped'; break;"
        "case 'BADC': orderDesc = 'Mid-Big Endian - Word swap only'; break;"
        "case 'CDAB': orderDesc = 'Mid-Little Endian - Mixed byte/word swap'; break;"
        "}"
        "infoHtml = '<strong>' + dataType + ' - ' + orderDesc + '</strong><br>- Range: 0 to 4,294,967,295<br>- Byte Order: ' + order + '<br>- Uses: Large counters, totalizers, flow accumulators<br>- Registers: 2';"
        "} else if(dataType.startsWith('INT32')) {"
        "const order = dataType.split('_')[1];"
        "let orderDesc = '';"
        "switch(order) {"
        "case 'ABCD': orderDesc = 'Big Endian - Reg0(AB) + Reg1(CD)'; break;"
        "case 'DCBA': orderDesc = 'Little Endian - Completely byte-swapped'; break;"
        "case 'BADC': orderDesc = 'Mid-Big Endian - Word swap only'; break;"
        "case 'CDAB': orderDesc = 'Mid-Little Endian - Mixed byte/word swap'; break;"
        "}"
        "infoHtml = '<strong>' + dataType + ' - ' + orderDesc + '</strong><br>- Range: -2,147,483,648 to 2,147,483,647<br>- Byte Order: ' + order + '<br>- Uses: Large signed measurements, differences<br>- Registers: 2';"
        "} else if(dataType.startsWith('FLOAT32')) {"
        "const order = dataType.split('_')[1];"
        "let orderDesc = '';"
        "switch(order) {"
        "case 'ABCD': orderDesc = 'Big Endian IEEE 754'; break;"
        "case 'DCBA': orderDesc = 'Little Endian IEEE 754'; break;"
        "case 'BADC': orderDesc = 'Mid-Big Endian IEEE 754'; break;"
        "case 'CDAB': orderDesc = 'Mid-Little Endian IEEE 754'; break;"
        "}"
        "infoHtml = '<strong>' + dataType + ' - ' + orderDesc + '</strong><br>- Range: +/-3.4E+/-38 (7 decimal digits)<br>- Byte Order: ' + order + '<br>- Uses: Precise measurements, flow rates, calculated values<br>- Registers: 2';"
        "}"
        "infoDiv.innerHTML = infoHtml;"
        "infoDiv.style.display = 'block';"
        
        "const quantityField = document.querySelector('input[name=\"sensor_' + sensorId + '_quantity\"]');"
        "if (quantityField) {"
        "if (dataType.startsWith('UINT16') || dataType.startsWith('INT16')) {"
        "quantityField.value = '1';"
        "} else if (dataType.startsWith('UINT32') || dataType.startsWith('INT32') || dataType.startsWith('FLOAT32')) {"
        "quantityField.value = '2';"
        "}"
        "}"
        "}"
        
        "function showEditDataTypeInfo(selectElement, sensorId) {"
        "const infoDiv = document.getElementById('edit-datatype-info-' + sensorId);"
        "const dataType = selectElement.value;"
        "let infoHtml = '';"
        "if(dataType.startsWith('UINT16')) {"
        "const endian = dataType.includes('BE') ? 'Big Endian' : 'Little Endian';"
        "infoHtml = '<strong>' + dataType + ' - ' + endian + ' 16-bit Unsigned</strong><br>- Range: 0 to 65,535<br>- Byte Order: ' + (dataType.includes('BE') ? 'AB (High-Low)' : 'BA (Low-High)') + '<br>- Uses: Counters, status flags, small measurements<br>- Registers: 1';"
        "} else if(dataType.startsWith('INT16')) {"
        "const endian = dataType.includes('BE') ? 'Big Endian' : 'Little Endian';"
        "infoHtml = '<strong>' + dataType + ' - ' + endian + ' 16-bit Signed</strong><br>- Range: -32,768 to 32,767<br>- Byte Order: ' + (dataType.includes('BE') ? 'AB (High-Low)' : 'BA (Low-High)') + '<br>- Uses: Temperature, pressure differences, signed measurements<br>- Registers: 1';"
        "} else if(dataType.startsWith('UINT32')) {"
        "const order = dataType.split('_')[1];"
        "let orderDesc = '';"
        "switch(order) {"
        "case 'ABCD': orderDesc = 'Big Endian - Reg0(AB) + Reg1(CD)'; break;"
        "case 'DCBA': orderDesc = 'Little Endian - Completely byte-swapped'; break;"
        "case 'BADC': orderDesc = 'Mid-Big Endian - Word swap only'; break;"
        "case 'CDAB': orderDesc = 'Mid-Little Endian - Mixed byte/word swap'; break;"
        "}"
        "infoHtml = '<strong>' + dataType + ' - ' + orderDesc + '</strong><br>- Range: 0 to 4,294,967,295<br>- Byte Order: ' + order + '<br>- Uses: Large counters, totalizers, flow accumulators<br>- Registers: 2';"
        "} else if(dataType.startsWith('INT32')) {"
        "const order = dataType.split('_')[1];"
        "let orderDesc = '';"
        "switch(order) {"
        "case 'ABCD': orderDesc = 'Big Endian - Reg0(AB) + Reg1(CD)'; break;"
        "case 'DCBA': orderDesc = 'Little Endian - Completely byte-swapped'; break;"
        "case 'BADC': orderDesc = 'Mid-Big Endian - Word swap only'; break;"
        "case 'CDAB': orderDesc = 'Mid-Little Endian - Mixed byte/word swap'; break;"
        "}"
        "infoHtml = '<strong>' + dataType + ' - ' + orderDesc + '</strong><br>- Range: -2,147,483,648 to 2,147,483,647<br>- Byte Order: ' + order + '<br>- Uses: Large signed measurements, differences<br>- Registers: 2';"
        "} else if(dataType.startsWith('FLOAT32')) {"
        "const order = dataType.split('_')[1];"
        "let orderDesc = '';"
        "switch(order) {"
        "case 'ABCD': orderDesc = 'Big Endian IEEE 754'; break;"
        "case 'DCBA': orderDesc = 'Little Endian IEEE 754'; break;"
        "case 'BADC': orderDesc = 'Mid-Big Endian IEEE 754'; break;"
        "case 'CDAB': orderDesc = 'Mid-Little Endian IEEE 754'; break;"
        "}"
        "infoHtml = '<strong>' + dataType + ' - ' + orderDesc + '</strong><br>- Range: +/-3.4E+/-38 (7 decimal digits)<br>- Byte Order: ' + order + '<br>- Uses: Precise measurements, flow rates, calculated values<br>- Registers: 2';"
        "}"
        "infoDiv.innerHTML = infoHtml;"
        "infoDiv.style.display = 'block';"
        
        "const quantityField = document.getElementById('edit_quantity_' + sensorId);"
        "if (quantityField) {"
        "if (dataType.startsWith('UINT16') || dataType.startsWith('INT16')) {"
        "quantityField.value = '1';"
        "} else if (dataType.startsWith('UINT32') || dataType.startsWith('INT32') || dataType.startsWith('FLOAT32')) {"
        "quantityField.value = '2';"
        "}"
        "}"
        "}"
        
        "function saveSingleSensor(sensorId) {"
        "console.log('Saving sensor:', sensorId);"
        "const nameField = document.querySelector('input[name=\"sensor_' + sensorId + '_name\"]');"
        "const unitIdField = document.querySelector('input[name=\"sensor_' + sensorId + '_unit_id\"]');"
        "console.log('Name field:', nameField, 'Value:', nameField ? nameField.value : 'NOT FOUND');"
        "console.log('Unit ID field:', unitIdField, 'Value:', unitIdField ? unitIdField.value : 'NOT FOUND');"
        "if (!nameField || !unitIdField) {"
        "alert('ERROR: Cannot find sensor form fields. Please try adding the sensor again.');"
        "return;"
        "}"
        "const name = nameField.value.trim();"
        "const unitId = unitIdField.value.trim();"
        "if (!name || !unitId) {"
        "alert('ERROR: Please fill in required fields:\\n\\n- Sensor Name: \"' + name + '\"\\n- Unit ID: \"' + unitId + '\"\\n\\nBoth fields must have values.');"
        "return;"
        "}"
        "console.log('Building form data for sensor:', sensorId);"
        "const formData = new URLSearchParams();"
        "const sensorFields = document.querySelectorAll('input[name^=\"sensor_' + sensorId + '_\"], select[name^=\"sensor_' + sensorId + '_\"]');"
        "console.log('Found', sensorFields.length, 'sensor fields');"
        "sensorFields.forEach(field => {"
        "console.log('Adding field:', field.name, '=', field.value);"
        "formData.append(field.name, field.value);"
        "});"
        "console.log('Form data:', formData.toString());"
        "fetch('/save_single_sensor', {method: 'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: formData})"
        ".then(response => response.json()).then(data => {"
        "console.log('Save response:', data);"
        "if(data.status === 'success') {"
        "alert('SUCCESS: Sensor saved successfully!\\n\\nSensor: ' + data.sensor_name + '\\nUnit ID: ' + data.unit_id + '\\n\\nYou can now test this sensor using the Test RS485 button.');"
        "sessionStorage.setItem('showSection', 'sensors');"
        "window.location.reload();"
        "} else {"
        "alert('ERROR: Failed to save sensor:\\n\\n' + data.message);"
        "}"
        "}).catch(e => {"
        "console.error('Save error:', e);"
        "alert('ERROR: Save failed: ' + e.message);"
        "});"
        "}"
        
        "function removeSensorForm(sensorId){"
        "if(confirm('Remove this sensor form?\\n\\nThis will only remove the form - no saved sensors will be deleted.')) {"
        "const sensorCard=document.getElementById('sensor-card-'+sensorId);"
        "if(sensorCard){sensorCard.remove();}"
        "}}"
        
        "function togglePassword(){"
        "const input=document.getElementById('wifi_password');"
        "const toggle=event.target;"
        "if(input.type==='password'){input.type='text';toggle.textContent='HIDE';}else{input.type='password';toggle.textContent='SHOW';}"
        "}"
        "function toggleAzurePassword(){"
        "const input=document.getElementById('azure_device_key');"
        "const toggle=event.target;"
        "if(input.type==='password'){input.type='text';toggle.textContent='HIDE';}else{input.type='password';toggle.textContent='SHOW';}"
        "}"
        "let telemetryRefreshInterval=null;"
        "function updateTelemetryDisplay(){"
        "fetch('/api/telemetry/history').then(r=>r.json()).then(data=>{"
        "const tbody=document.getElementById('telemetry-table-body');"
        "const status=document.getElementById('telemetry-status');"
        "if(data.length===0){"
        "tbody.innerHTML='<tr><td colspan=\"4\" style=\"text-align:center;padding:20px;color:#888\">No telemetry data yet. Waiting for first send...</td></tr>';"
        "status.textContent='No messages yet';"
        "status.style.background='#fff3cd';"
        "status.style.color='#856404';"
        "}else{"
        "let html='';"
        "data.forEach((item,idx)=>{"
        "const statusColor=item.success?'#28a745':'#dc3545';"
        "const statusText=item.success?'✅ Sent':'❌ Failed';"
        "const payloadStr=JSON.stringify(item.payload,null,2);"
        "html+=`<tr style=\"border-bottom:1px solid #dee2e6\">`;"
        "html+=`<td style=\"padding:8px;border:1px solid #dee2e6;font-weight:600\">${idx+1}</td>`;"
        "html+=`<td style=\"padding:8px;border:1px solid #dee2e6\">${item.timestamp}</td>`;"
        "html+=`<td style=\"padding:8px;border:1px solid #dee2e6;color:${statusColor};font-weight:600\">${statusText}</td>`;"
        "html+=`<td style=\"padding:8px;border:1px solid #dee2e6\"><pre style=\"margin:0;font-size:11px;white-space:pre-wrap;word-wrap:break-word\">${payloadStr}</pre></td>`;"
        "html+='</tr>';"
        "});"
        "tbody.innerHTML=html;"
        "const now=new Date().toLocaleTimeString();"
        "status.textContent=`Last updated: ${now} | Total messages: ${data.length}/25`;"
        "status.style.background='#d4edda';"
        "status.style.color='#155724';"
        "}"
        "}).catch(err=>{"
        "console.error('Telemetry fetch error:',err);"
        "document.getElementById('telemetry-status').textContent='⚠️ Error loading telemetry data';"
        "document.getElementById('telemetry-status').style.background='#f8d7da';"
        "document.getElementById('telemetry-status').style.color='#721c24';"
        "});"
        "}"
        "function refreshTelemetryNow(){"
        "updateTelemetryDisplay();"
        "}"
        "function startTelemetryAutoRefresh(){"
        "if(telemetryRefreshInterval)clearInterval(telemetryRefreshInterval);"
        "updateTelemetryDisplay();"
        "telemetryRefreshInterval=setInterval(updateTelemetryDisplay,5000);"
        "}"
        "function stopTelemetryAutoRefresh(){"
        "if(telemetryRefreshInterval){"
        "clearInterval(telemetryRefreshInterval);"
        "telemetryRefreshInterval=null;"
        "}"
        "}"
        "function downloadTelemetryCSV(){"
        "fetch('/api/telemetry/history').then(r=>r.json()).then(data=>{"
        "if(!data.messages||data.messages.length===0){alert('No telemetry data to download');return;}"
        "let csv='Index,Timestamp,Status,Payload\\n';"
        "data.messages.forEach((msg,i)=>{"
        "const payload=msg.payload.replace(/\"/g,'\"\"');"
        "csv+=`${i+1},${msg.timestamp},${msg.status},\"${payload}\"\\n`;"
        "});"
        "const blob=new Blob([csv],{type:'text/csv'});"
        "const url=URL.createObjectURL(blob);"
        "const a=document.createElement('a');"
        "a.href=url;"
        "a.download='telemetry_'+new Date().toISOString().replace(/[:.]/g,'-').slice(0,-5)+'.csv';"
        "a.click();"
        "URL.revokeObjectURL(url);"
        "}).catch(err=>alert('Error downloading CSV: '+err.message));"
        "}"
        "function downloadTelemetryJSON(){"
        "fetch('/api/telemetry/history').then(r=>r.json()).then(data=>{"
        "if(!data.messages||data.messages.length===0){alert('No telemetry data to download');return;}"
        "const jsonStr=JSON.stringify(data,null,2);"
        "const blob=new Blob([jsonStr],{type:'application/json'});"
        "const url=URL.createObjectURL(blob);"
        "const a=document.createElement('a');"
        "a.href=url;"
        "a.download='telemetry_'+new Date().toISOString().replace(/[:.]/g,'-').slice(0,-5)+'.json';"
        "a.click();"
        "URL.revokeObjectURL(url);"
        "}).catch(err=>alert('Error downloading JSON: '+err.message));"
        "}"
        "function toggleNetworkMode(){"
        "const wifiMode=document.getElementById('mode_wifi').checked;"
        "const simMode=document.getElementById('mode_sim').checked;"
        "document.getElementById('wifi_panel').style.display=wifiMode?'block':'none';"
        "document.getElementById('sim_panel').style.display=simMode?'block':'none';"
        "document.getElementById('wifi-network-status').style.display=wifiMode?'block':'none';"
        "document.getElementById('sim-network-status').style.display=simMode?'block':'none';"
        "}"
        "function toggleSDOptions(){"
        "const enabled=document.getElementById('sd_enabled').checked;"
        "document.getElementById('sd_options').style.display=enabled?'block':'none';"
        "document.getElementById('sd_hw_options').style.display=enabled?'block':'none';"
        "}"
        "function toggleRTCOptions(){"
        "const enabled=document.getElementById('rtc_enabled').checked;"
        "document.getElementById('rtc_options').style.display=enabled?'block':'none';"
        "document.getElementById('rtc_hw_options').style.display=enabled?'block':'none';"
        "}"
        "function toggleTelegramOptions(){"
        "const enabled=document.getElementById('telegram_enabled').checked;"
        "document.getElementById('telegram_options').style.display=enabled?'block':'none';"
        "}"
        "function saveTelegramConfig(){"
        "const formData=new FormData(document.getElementById('telegram_config_form'));"
        "const result=document.getElementById('telegram_save_result');"
        "result.innerHTML='Saving...';"
        "result.style.display='block';"
        "result.style.backgroundColor='#fff3cd';"
        "result.style.color='#856404';"
        "fetch('/save_telegram_config',{method:'POST',body:formData})"
        ".then(r=>r.json())"
        ".then(data=>{"
        "result.innerHTML=data.message;"
        "result.style.backgroundColor='#d4edda';"
        "result.style.color='#155724';"
        "setTimeout(()=>{result.style.display='none';},3000);"
        "}).catch(err=>{"
        "result.innerHTML='Error: '+err.message;"
        "result.style.backgroundColor='#f8d7da';"
        "result.style.color='#721c24';"
        "});"
        "return false;"
        "}"
        "function testTelegramBot(){"
        "const result=document.getElementById('telegram_test_result');"
        "const botToken=document.getElementById('telegram_bot_token').value;"
        "const chatId=document.getElementById('telegram_chat_id').value;"
        "const params=new URLSearchParams();"
        "params.append('bot_token',botToken);"
        "params.append('chat_id',chatId);"
        "result.innerHTML='🧪 Testing Telegram bot...';"
        "result.style.display='block';"
        "result.style.backgroundColor='#fff3cd';"
        "result.style.color='#856404';"
        "fetch('/api/telegram_test',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:params.toString()})"
        ".then(r=>r.json())"
        ".then(data=>{"
        "if(data.status==='success'){"
        "result.innerHTML='✅ '+data.message;"
        "result.style.backgroundColor='#d4edda';"
        "result.style.color='#155724';"
        "}else{"
        "result.innerHTML='❌ '+data.message;"
        "result.style.backgroundColor='#f8d7da';"
        "result.style.color='#721c24';"
        "}"
        "}).catch(err=>{"
        "result.innerHTML='❌ Error: '+err.message;"
        "result.style.backgroundColor='#f8d7da';"
        "result.style.color='#721c24';"
        "});"
        "}"
        "let simTestPollInterval=null;"
        "function testSIMConnection(){"
        "const result=document.getElementById('sim_test_result');"
        "result.innerHTML='<div style=\"text-align:center\">⏳ Starting SIM test...<br><small>Please wait</small></div>';"
        "result.style.display='block';"
        "result.style.backgroundColor='#fff3cd';"
        "result.style.color='#856404';"
        "fetch('/api/sim_test',{method:'POST'})"
        ".then(r=>r.json())"
        ".then(data=>{"
        "if(data.status==='started'){"
        "result.innerHTML='<div style=\"text-align:center\">⏳ Testing SIM connection...<br><small>Initializing modem and connecting to network (this may take up to 30 seconds)</small></div>';"
        "if(simTestPollInterval) clearInterval(simTestPollInterval);"
        "simTestPollInterval=setInterval(checkSIMTestStatus,2000);"
        "}else{"
        "result.innerHTML='<span style=\"color:#721c24\">❌ Failed to start test: '+data.message+'</span>';"
        "result.style.backgroundColor='#f8d7da';"
        "}"
        "})"
        ".catch(err=>{"
        "result.innerHTML='<span style=\"color:#721c24\">❌ Test failed: '+err+'</span>';"
        "result.style.backgroundColor='#f8d7da';"
        "});"
        "}"
        "function checkSIMTestStatus(){"
        "fetch('/api/sim_test_status')"
        ".then(r=>r.json())"
        ".then(data=>{"
        "const result=document.getElementById('sim_test_result');"
        "if(data.status==='in_progress'){"
        "return;"
        "}"
        "if(simTestPollInterval){"
        "clearInterval(simTestPollInterval);"
        "simTestPollInterval=null;"
        "}"
        "if(data.status==='completed'){"
        "if(data.success){"
        "let msg='<div style=\"color:#155724\"><strong>✅ SIM Module Connected Successfully!</strong><br>';"
        "msg+='<div style=\"margin-top:10px;font-size:14px;line-height:1.6\">';"
        "msg+='<strong>📡 IP Address:</strong> <span style=\"background:#e7f3ff;padding:2px 8px;border-radius:3px;font-family:monospace\">'+data.ip+'</span><br>';"
        "msg+='<strong>📶 Signal:</strong> '+data.signal+' dBm ('+data.signal_quality+')<br>';"
        "msg+='<strong>📱 Operator:</strong> '+data.operator+'<br>';"
        "msg+='<strong>🌐 APN:</strong> '+data.apn+'<br>';"
        "msg+='</div>';"
        "msg+='<div style=\"margin-top:10px;padding:10px;background:#d1ecf1;border-left:4px solid #17a2b8;border-radius:4px;color:#0c5460\">';"
        "msg+='✓ Modem is working correctly<br>';"
        "msg+='✓ Network registration successful<br>';"
        "msg+='✓ Internet connectivity established';"
        "msg+='</div></div>';"
        "result.innerHTML=msg;"
        "result.style.backgroundColor='#d4edda';"
        "result.style.color='#155724';"
        "}else{"
        "let msg='<div style=\"color:#721c24\"><strong>❌ SIM Connection Failed</strong><br>';"
        "msg+='<div style=\"margin-top:8px;font-size:14px\">'+data.error+'</div>';"
        "if(data.signal){"
        "msg+='<div style=\"margin-top:10px;padding:8px;background:#fff3cd;border-radius:4px;color:#856404\">';"
        "msg+='📶 Signal: '+data.signal+' dBm';"
        "if(data.operator){msg+=' | Operator: '+data.operator;}"
        "msg+='</div>';"
        "}"
        "msg+='</div>';"
        "result.innerHTML=msg;"
        "result.style.backgroundColor='#f8d7da';"
        "result.style.color='#721c24';"
        "}"
        "}"
        "})"
        ".catch(err=>console.error('Poll error:',err));"
        "}"
        "function checkSDStatus(){"
        "const result=document.getElementById('sd_status_result');"
        "result.innerHTML='Checking SD card...';"
        "result.style.display='block';"
        "result.style.backgroundColor='#fff3cd';"
        "fetch('/api/sd_status')"
        ".then(r=>r.json())"
        ".then(data=>{"
        "if(data.mounted){"
        "result.innerHTML='<span style=\"color:#155724\">SD Card: '+data.size_mb+' MB total, '+data.free_mb+' MB free, '+data.cached_messages+' messages cached</span>';"
        "result.style.backgroundColor='#d4edda';"
        "}else{"
        "result.innerHTML='<span style=\"color:#721c24\">SD Card not mounted</span>';"
        "result.style.backgroundColor='#f8d7da';"
        "}"
        "})"
        ".catch(err=>{"
        "result.innerHTML='<span style=\"color:#721c24\">Failed to check SD: '+err+'</span>';"
        "result.style.backgroundColor='#f8d7da';"
        "});"
        "}"
        "function replayCachedMessages(){"
        "if(!confirm('Replay all cached messages now?'))return;"
        "fetch('/api/sd_replay',{method:'POST'})"
        ".then(r=>r.json())"
        ".then(data=>{"
        "if(data.success){"
        "alert('Replayed '+data.count+' messages successfully');"
        "}else{"
        "alert('Replay failed: '+data.error);"
        "}"
        "})"
        ".catch(err=>{alert('Replay failed: '+err);});"
        "}"
        "function clearCachedMessages(){"
        "if(!confirm('Clear all cached messages? This cannot be undone!'))return;"
        "fetch('/api/sd_clear',{method:'POST'})"
        ".then(r=>r.json())"
        ".then(data=>{"
        "if(data.success){"
        "alert('Cleared '+data.count+' cached messages');"
        "}else{"
        "alert('Clear failed: '+data.error);"
        "}"
        "})"
        ".catch(err=>{alert('Clear failed: '+err);});"
        "}"
        "function getRTCTime(){"
        "const result=document.getElementById('rtc_time_result');"
        "result.innerHTML='Reading RTC...';"
        "result.style.display='block';"
        "result.style.backgroundColor='#fff3cd';"
        "fetch('/api/rtc_time')"
        ".then(r=>r.json())"
        ".then(data=>{"
        "if(data.success){"
        "result.innerHTML='<span style=\"color:#155724\">RTC Time: '+data.time+', Temperature: '+data.temp+'C</span>';"
        "result.style.backgroundColor='#d4edda';"
        "}else{"
        "result.innerHTML='<span style=\"color:#721c24\">RTC not available</span>';"
        "result.style.backgroundColor='#f8d7da';"
        "}"
        "})"
        ".catch(err=>{"
        "result.innerHTML='<span style=\"color:#721c24\">Failed to read RTC: '+err+'</span>';"
        "result.style.backgroundColor='#f8d7da';"
        "});"
        "}"
        "function syncRTCFromNTP(){"
        "if(!confirm('Sync RTC from NTP now? Requires internet connection.'))return;"
        "fetch('/api/rtc_sync',{method:'POST'})"
        ".then(r=>r.json())"
        ".then(data=>{"
        "if(data.success){"
        "alert('RTC synced successfully from NTP');"
        "}else{"
        "alert('RTC sync failed: '+data.error);"
        "}"
        "})"
        ".catch(err=>{alert('RTC sync failed: '+err);});"
        "}"
        "function syncSystemFromRTC(){"
        "if(!confirm('Sync system time from RTC now?'))return;"
        "fetch('/api/rtc_set',{method:'POST'})"
        ".then(r=>r.json())"
        ".then(data=>{"
        "if(data.success){"
        "alert('System time synced from RTC');"
        "}else{"
        "alert('Sync failed: '+data.error);"
        "}"
        "})"
        ".catch(err=>{alert('Sync failed: '+err);});"
        "}"
        "function saveNetworkMode(e){"
        "e.preventDefault();"
        "const formData=new URLSearchParams(new FormData(e.target));"
        "const resultDiv=document.getElementById('network_mode_result');"
        "resultDiv.innerHTML='<span style=\"color:#856404\">Saving network mode...</span>';"
        "resultDiv.style.display='block';"
        "resultDiv.style.backgroundColor='#fff3cd';"
        "resultDiv.style.borderColor='#ffeaa7';"
        "fetch('/save_network_mode',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData})"
        ".then(r=>r.json())"
        ".then(data=>{"
        "if(data.status==='success'){"
        "resultDiv.innerHTML='<span style=\"color:#28a745\">✅ '+data.message+'</span>';"
        "resultDiv.style.backgroundColor='#d4edda';"
        "resultDiv.style.borderColor='#c3e6cb';"
        "setTimeout(()=>{"
        "resultDiv.style.display='none';"
        "},3000);"
        "}else{"
        "resultDiv.innerHTML='<span style=\"color:#721c24\">❌ Failed to save: '+data.message+'</span>';"
        "resultDiv.style.backgroundColor='#f8d7da';"
        "resultDiv.style.borderColor='#f5c6cb';"
        "}"
        "})"
        ".catch(err=>{"
        "resultDiv.innerHTML='<span style=\"color:#721c24\">❌ Error: '+err+'</span>';"
        "resultDiv.style.backgroundColor='#f8d7da';"
        "resultDiv.style.borderColor='#f5c6cb';"
        "});"
        "return false;"
        "}"
        "function saveSDConfig(){"
        "const formData=new URLSearchParams();"
        "if(document.getElementById('sd_enabled').checked) formData.append('sd_enabled','1');"
        "if(document.getElementById('sd_cache_on_failure').checked) formData.append('sd_cache_on_failure','1');"
        "formData.append('sd_mosi',document.getElementById('sd_mosi').value);"
        "formData.append('sd_miso',document.getElementById('sd_miso').value);"
        "formData.append('sd_clk',document.getElementById('sd_clk').value);"
        "formData.append('sd_cs',document.getElementById('sd_cs').value);"
        "formData.append('sd_spi_host',document.getElementById('sd_spi_host').value);"
        "const resultDiv=document.getElementById('sd_save_result');"
        "resultDiv.innerHTML='<span style=\"color:#856404\">Saving SD Card configuration...</span>';"
        "resultDiv.style.display='block';"
        "resultDiv.style.backgroundColor='#fff3cd';"
        "resultDiv.style.borderColor='#ffeaa7';"
        "resultDiv.style.color='#856404';"
        "fetch('/save_sd_config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData})"
        ".then(r=>r.json())"
        ".then(data=>{"
        "if(data.status==='success'){"
        "resultDiv.innerHTML='<span style=\"color:#28a745\">✅ '+data.message+'</span>';"
        "resultDiv.style.backgroundColor='#d4edda';"
        "resultDiv.style.borderColor='#c3e6cb';"
        "resultDiv.style.color='#155724';"
        "}else{"
        "resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Error: '+data.message+'</span>';"
        "resultDiv.style.backgroundColor='#f8d7da';"
        "resultDiv.style.borderColor='#f5c6cb';"
        "resultDiv.style.color='#721c24';"
        "}"
        "})"
        ".catch(e=>{"
        "resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Network Error: '+e.message+'</span>';"
        "resultDiv.style.backgroundColor='#f8d7da';"
        "resultDiv.style.borderColor='#f5c6cb';"
        "resultDiv.style.color='#721c24';"
        "});"
        "return false;"
        "}"
        "function saveRTCConfig(){"
        "const formData=new URLSearchParams();"
        "if(document.getElementById('rtc_enabled').checked) formData.append('rtc_enabled','1');"
        "if(document.getElementById('rtc_sync_on_boot').checked) formData.append('rtc_sync_on_boot','1');"
        "if(document.getElementById('rtc_update_from_ntp').checked) formData.append('rtc_update_from_ntp','1');"
        "formData.append('rtc_sda',document.getElementById('rtc_sda').value);"
        "formData.append('rtc_scl',document.getElementById('rtc_scl').value);"
        "formData.append('rtc_i2c_num',document.getElementById('rtc_i2c_num').value);"
        "const resultDiv=document.getElementById('rtc_save_result');"
        "resultDiv.innerHTML='<span style=\"color:#856404\">Saving RTC configuration...</span>';"
        "resultDiv.style.display='block';"
        "resultDiv.style.backgroundColor='#fff3cd';"
        "resultDiv.style.borderColor='#ffeaa7';"
        "resultDiv.style.color='#856404';"
        "fetch('/save_rtc_config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData})"
        ".then(r=>r.json())"
        ".then(data=>{"
        "if(data.status==='success'){"
        "resultDiv.innerHTML='<span style=\"color:#28a745\">✅ '+data.message+'</span>';"
        "resultDiv.style.backgroundColor='#d4edda';"
        "resultDiv.style.borderColor='#c3e6cb';"
        "resultDiv.style.color='#155724';"
        "}else{"
        "resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Error: '+data.message+'</span>';"
        "resultDiv.style.backgroundColor='#f8d7da';"
        "resultDiv.style.borderColor='#f5c6cb';"
        "resultDiv.style.color='#721c24';"
        "}"
        "})"
        ".catch(e=>{"
        "resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Network Error: '+e.message+'</span>';"
        "resultDiv.style.backgroundColor='#f8d7da';"
        "resultDiv.style.borderColor='#f5c6cb';"
        "resultDiv.style.color='#721c24';"
        "});"
        "return false;"
        "}"
        "function saveSIMConfig(){"
        "const formData=new URLSearchParams();"
        "formData.append('sim_apn',document.getElementById('sim_apn').value);"
        "formData.append('sim_apn_user',document.getElementById('sim_apn_user').value);"
        "formData.append('sim_apn_pass',document.getElementById('sim_apn_pass').value);"
        "formData.append('sim_uart',document.getElementById('sim_uart').value);"
        "formData.append('sim_tx_pin',document.getElementById('sim_tx_pin').value);"
        "formData.append('sim_rx_pin',document.getElementById('sim_rx_pin').value);"
        "formData.append('sim_pwr_pin',document.getElementById('sim_pwr_pin').value);"
        "formData.append('sim_reset_pin',document.getElementById('sim_reset_pin').value);"
        "formData.append('sim_baud',document.getElementById('sim_baud').value);"
        "const resultDiv=document.getElementById('sim_save_result');"
        "resultDiv.innerHTML='<span style=\"color:#856404\">Saving SIM configuration...</span>';"
        "resultDiv.style.display='block';"
        "resultDiv.style.backgroundColor='#fff3cd';"
        "resultDiv.style.borderColor='#ffeaa7';"
        "resultDiv.style.color='#856404';"
        "fetch('/save_sim_config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData})"
        ".then(r=>r.json())"
        ".then(data=>{"
        "if(data.status==='success'){"
        "resultDiv.innerHTML='<span style=\"color:#28a745\">✅ '+data.message+'</span>';"
        "resultDiv.style.backgroundColor='#d4edda';"
        "resultDiv.style.borderColor='#c3e6cb';"
        "resultDiv.style.color='#155724';"
        "}else{"
        "resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Error: '+data.message+'</span>';"
        "resultDiv.style.backgroundColor='#f8d7da';"
        "resultDiv.style.borderColor='#f5c6cb';"
        "resultDiv.style.color='#721c24';"
        "}"
        "})"
        ".catch(e=>{"
        "resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Network Error: '+e.message+'</span>';"
        "resultDiv.style.backgroundColor='#f8d7da';"
        "resultDiv.style.borderColor='#f5c6cb';"
        "resultDiv.style.color='#721c24';"
        "});"
        "return false;"
        "}"
        "function saveAzureConfig(){"
        "const password=prompt('Save Azure Configuration\\n\\nThis will update sensitive Azure IoT Hub settings.\\nPlease enter the admin password to confirm:');"
        "if(password===null) return;"
        "if(password!=='admin123'){"
        "alert('Access Denied\\n\\nIncorrect password. Cannot save Azure configuration.\\n\\nContact your system administrator if you need access.');"
        "return;}"
        "const deviceId=document.querySelector('input[name=\"azure_device_id\"]').value;"
        "const deviceKey=document.getElementById('azure_device_key').value;"
        "const telemetryInterval=document.querySelector('input[name=\"telemetry_interval\"]').value;"
        "if(!deviceId){alert('ERROR: Please enter the Azure device ID');return;}"
        "if(!deviceKey){alert('ERROR: Please enter the Azure device key');return;}"
        "const formData='azure_device_id='+encodeURIComponent(deviceId)+'&azure_device_key='+encodeURIComponent(deviceKey)+'&telemetry_interval='+telemetryInterval;"
        "fetch('/save_azure_config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData})"
        ".then(r=>r.json()).then(data=>{"
        "if(data.status==='success'){alert('SUCCESS: Azure configuration saved successfully!\\n\\nDevice ID, device key, and telemetry interval updated.');}else{alert('ERROR: '+data.message);}"
        "}).catch(e=>{alert('ERROR: Save failed: '+e.message);});"
        "}"
        "function saveModemConfig(){"
        "console.log('saveModemConfig function called');"
        "const formData=new URLSearchParams();"
        "const modemEnabled=document.getElementById('modem_reset_enabled').checked;"
        "const gpioPin=document.getElementById('modem_reset_gpio_pin').value;"
        "const bootDelay=document.getElementById('modem_boot_delay').value;"
        "if(gpioPin<2||gpioPin>39||(gpioPin>=6&&gpioPin<=11)){"
        "alert('ERROR: Invalid GPIO pin. Please use pins 2-39, excluding 6-11 which are reserved.');return false;"
        "}"
        "if(bootDelay<5||bootDelay>60){"
        "alert('ERROR: Boot delay must be between 5 and 60 seconds.');return false;"
        "}"
        "if(modemEnabled) formData.append('modem_reset_enabled','1');"
        "formData.append('modem_reset_gpio_pin',gpioPin);"
        "formData.append('modem_boot_delay',bootDelay);"
        "const resultDiv=document.getElementById('modem-result');"
        "resultDiv.innerHTML='<span style=\"color:#856404\">Saving modem configuration...</span>';"
        "resultDiv.style.display='block';"
        "resultDiv.style.backgroundColor='#fff3cd';"
        "resultDiv.style.borderColor='#ffeaa7';"
        "resultDiv.style.color='#856404';"
        "fetch('/save_modem_config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData})"
        ".then(r=>r.json()).then(data=>{"
        "if(data.status==='success'){"
        "resultDiv.innerHTML='<span style=\"color:#28a745\">✅ '+data.message+'</span>';"
        "resultDiv.style.backgroundColor='#d4edda';"
        "resultDiv.style.borderColor='#c3e6cb';"
        "resultDiv.style.color='#155724';"
        "}else{"
        "resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Error: '+data.message+'</span>';"
        "resultDiv.style.backgroundColor='#f8d7da';"
        "resultDiv.style.borderColor='#f5c6cb';"
        "resultDiv.style.color='#721c24';"
        "}"
        "}).catch(e=>{"
        "resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Network Error: '+e.message+'</span>';"
        "resultDiv.style.backgroundColor='#f8d7da';"
        "resultDiv.style.borderColor='#f5c6cb';"
        "resultDiv.style.color='#721c24';"
        "});return false;"
        "}");
    
    httpd_resp_sendstr_chunk(req,
        "function saveSystemConfig(){"
        "const triggerPin=document.getElementById('trigger_gpio_pin').value;"
        "if(triggerPin<0||triggerPin>39){"
        "alert('ERROR: Invalid GPIO pin. Use pins 0-39.');return false;"
        "}"
        "const formData=new URLSearchParams();"
        "formData.append('trigger_gpio_pin',triggerPin);"
        "const resultDiv=document.getElementById('system-control-result');"
        "resultDiv.innerHTML='<span style=\"color:#856404\">Saving system settings...</span>';"
        "resultDiv.style.display='block';"
        "resultDiv.style.backgroundColor='#fff3cd';"
        "resultDiv.style.borderColor='#ffeaa7';"
        "resultDiv.style.color='#856404';"
        "fetch('/save_system_config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData})"
        ".then(r=>r.json()).then(data=>{"
        "if(data.status==='success'){"
        "resultDiv.innerHTML='<span style=\"color:#28a745\">✅ '+data.message+'</span>';"
        "resultDiv.style.backgroundColor='#d4edda';"
        "resultDiv.style.borderColor='#c3e6cb';"
        "resultDiv.style.color='#155724';"
        "}else{"
        "resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Error: '+data.message+'</span>';"
        "resultDiv.style.backgroundColor='#f8d7da';"
        "resultDiv.style.borderColor='#f5c6cb';"
        "resultDiv.style.color='#721c24';"
        "}"
        "}).catch(e=>{"
        "resultDiv.innerHTML='<span style=\"color:#dc3545\">❌ Communication Error: '+e.message+'</span>';"
        "resultDiv.style.backgroundColor='#f8d7da';"
        "resultDiv.style.borderColor='#f5c6cb';"
        "resultDiv.style.color='#721c24';"
        "});return false;"
        "}");
    
    httpd_resp_sendstr_chunk(req,
        "let scanInProgress = false;"
        "function scanWiFi(){"
        "const statusDiv=document.getElementById('scan-status');"
        "const networksDiv=document.getElementById('networks');"
        "const scanBtn=document.querySelector('.scan-button');"
        "if (!statusDiv || !networksDiv) {"
        "console.error('Required elements not found for WiFi scan');"
        "return;"
        "}"
        "if (scanInProgress) {"
        "statusDiv.innerHTML='<span style=\"color:#ffc107\">Scan already in progress. Please wait...</span>';"
        "return;"
        "}"
        "scanInProgress = true;"
        "if (scanBtn) {"
        "scanBtn.style.opacity='0.6';"
        "scanBtn.style.pointerEvents='none';"
        "}"
        "statusDiv.innerHTML='<span style=\"color:#17a2b8\">Scanning for networks...</span>';"
        "networksDiv.style.display='none';"
        "console.log('Starting WiFi scan...');"
        "fetch('/scan_wifi').then(r=>{"
        "console.log('Received response:', r.status, r.statusText);"
        "if (!r.ok) {"
        "throw new Error('HTTP ' + r.status + ': ' + r.statusText);"
        "}"
        "return r.text();"
        "}).then(text=>{"
        "console.log('Response text:', text);"
        "let data;"
        "try {"
        "data = JSON.parse(text);"
        "} catch (e) {"
        "console.error('JSON parse error:', e, 'Text:', text);"
        "throw new Error('Invalid server response');"
        "}"
        "console.log('Parsed data:', data);"
        "if(data.error){"
        "statusDiv.innerHTML='<span style=\"color:#dc3545\">Error: '+data.error+'</span>';"
        "return;"
        "}"
        "const count = data.count || 0;"
        "statusDiv.innerHTML='<span style=\"color:#28a745\">'+count+' networks found</span>';"
        "networksDiv.innerHTML='';"
        "if(!data.networks || data.networks.length===0){"
        "networksDiv.innerHTML='<div style=\"padding:20px;text-align:center;color:#666;background:white;border-radius:6px;margin:8px\"><i>No networks found</i><br><small>Try scanning again</small></div>';"
        "networksDiv.style.display='block';"
        "return;"
        "}"
        "data.networks.sort((a,b)=>b.rssi-a.rssi);"
        "const maxHeight=count>15?'400px':count>10?'300px':count>5?'200px':'180px';"
        "networksDiv.style.maxHeight=maxHeight;"
        "networksDiv.style.overflowY='auto';"
        "networksDiv.style.padding='10px';"
        "data.networks.forEach((n,i)=>{"
        "const item=document.createElement('div');"
        "const signalColor=n.rssi>-50?'#28a745':n.rssi>-70?'#ffc107':'#dc3545';"
        "item.className='network-item';"
        "item.style.cssText='padding:12px;cursor:pointer;border-radius:6px;margin:4px 0;background:white;border:1px solid #e0e0e0;transition:all 0.2s ease';"
        "item.innerHTML='<div style=\"display:flex;justify-content:space-between;align-items:center\"><div><div style=\"display:flex;align-items:center;gap:8px\"><strong style=\"color:#2c3e50\">'+n.ssid+'</strong>'+n.security_icon+'</div><small style=\"color:#666;margin-top:2px;display:block\">Channel '+n.channel+' * '+n.signal_strength+'</small></div><div style=\"text-align:right\"><div style=\"color:'+signalColor+';font-weight:bold;font-size:13px\">'+n.rssi+'dBm</div><small style=\"color:#888\">'+n.signal_icon+'</small></div></div>';"
        "item.onmouseover=()=>{item.style.background='#e3f2fd';item.style.borderColor='#2196f3'};"
        "item.onmouseout=()=>{item.style.background='white';item.style.borderColor='#e0e0e0'};"
        "item.onclick=()=>{"
        "document.getElementById('wifi_ssid').value=n.ssid;"
        "networksDiv.style.display='none';"
        "statusDiv.innerHTML='<span style=\"color:#17a2b8\">Selected: <strong>'+n.ssid+'</strong> ('+n.signal_strength+')</span>';"
        "document.getElementById('wifi_ssid').focus();"
        "};"
        "networksDiv.appendChild(item);"
        "});"
        "networksDiv.style.display='block';"
        "}).catch(e=>{"
        "console.error('Scan error:', e);"
        "statusDiv.innerHTML='<span style=\"color:#dc3545\">Scan failed: '+e.message+'</span>';"
        "}).finally(()=>{"
        "scanInProgress = false;"
        "if (scanBtn) {"
        "scanBtn.style.opacity='1';"
        "scanBtn.style.pointerEvents='auto';"
        "}"
        "console.log('Scan completed');"
        "});"
        "}");
    
    // Reboot system function
    httpd_resp_sendstr_chunk(req,
        "function rebootSystem(){"
        "if(confirm('Are you sure you want to reboot the system? This will restart the device and it may take a few minutes to come back online.')){"
        "fetch('/reboot',{method:'POST',headers:{'Content-Type':'application/json'}})"
        ".then(r=>r.json()).then(data=>{"
        "if(data.status==='success'){"
        "alert('System is rebooting. Please wait a few minutes and refresh the page.');"
        "}else{"
        "alert('Reboot failed: '+data.message);"
        "}"
        "}).catch(e=>{"
        "alert('Reboot request sent. Please wait a few minutes and refresh the page.');"
        "});"
        "}"
        "}");
    
    // Simplified test sensor function - real-time RS485 communication
    httpd_resp_sendstr_chunk(req,
        "function testSensor(sensorId){"
        "const resultDiv=document.getElementById('test-result-'+sensorId);"
        "resultDiv.innerHTML='<div style=\"background:#e3f2fd;padding:8px;border-radius:4px;margin:5px 0\">Testing RS485 Modbus communication...</div>';"
        "resultDiv.style.display='block';"
        "fetch('/test_sensor',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'sensor_id='+sensorId})"
        ".then(r=>r.text()).then(htmlData=>{"
        "resultDiv.innerHTML=htmlData;"
        "}).catch(e=>{"
        "resultDiv.innerHTML='<div style=\"background:#ffebee;padding:8px;border-radius:4px;color:red;margin:5px 0\">Communication Error: '+e.message+'</div>';"
        "});}"
        );
    
    // Generate sensor data array for JavaScript use
    httpd_resp_sendstr_chunk(req, "const sensorData=[");
    for (int i = 0; i < g_system_config.sensor_count; i++) {
        char escaped_name[64], escaped_unit[32], escaped_type[64];
        html_escape(escaped_name, g_system_config.sensors[i].name, sizeof(escaped_name));
        html_escape(escaped_unit, g_system_config.sensors[i].unit_id, sizeof(escaped_unit));
        html_escape(escaped_type, g_system_config.sensors[i].data_type, sizeof(escaped_type));
        
        ESP_LOGI(TAG, "sensorData[%d]: name='%s', sensor_type='%s' (len=%d), data_type='%s' (len=%d), escaped_type='%s', scale_factor=%.2f", 
                 i, g_system_config.sensors[i].name, g_system_config.sensors[i].sensor_type, strlen(g_system_config.sensors[i].sensor_type), g_system_config.sensors[i].data_type, strlen(g_system_config.sensors[i].data_type), escaped_type, g_system_config.sensors[i].scale_factor);
        
        char escaped_meter_type[64];
        html_escape(escaped_meter_type, g_system_config.sensors[i].meter_type, sizeof(escaped_meter_type));
        
        // Start building sensor data object
        snprintf(chunk, sizeof(chunk),
            "{name:'%s',unit_id:'%s',slave_id:%d,register_address:%d,quantity:%d,data_type:'%s',baud_rate:%d,parity:'%s',scale_factor:%.2f,register_type:'%s',sensor_type:'%s',sensor_height:%.2f,max_water_level:%.2f,meter_type:'%s'",
            escaped_name, escaped_unit, g_system_config.sensors[i].slave_id, 
            g_system_config.sensors[i].register_address, g_system_config.sensors[i].quantity, 
            escaped_type, g_system_config.sensors[i].baud_rate, g_system_config.sensors[i].parity[0] ? g_system_config.sensors[i].parity : "none", g_system_config.sensors[i].scale_factor,
            g_system_config.sensors[i].register_type[0] ? g_system_config.sensors[i].register_type : "HOLDING",
            g_system_config.sensors[i].sensor_type[0] ? g_system_config.sensors[i].sensor_type : "Flow-Meter",
            g_system_config.sensors[i].sensor_height, g_system_config.sensors[i].max_water_level,
            escaped_meter_type);
        httpd_resp_sendstr_chunk(req, chunk);
        
        // Add sub-sensor data for QUALITY sensors
        if (strcmp(g_system_config.sensors[i].sensor_type, "QUALITY") == 0) {
            ESP_LOGI(TAG, "[FIND] Adding sub-sensor data for QUALITY sensor[%d]: sub_sensor_count=%d", 
                     i, g_system_config.sensors[i].sub_sensor_count);
            httpd_resp_sendstr_chunk(req, ",sub_sensors:[");
            bool first_sub_sensor = true;
            for (int j = 0; j < g_system_config.sensors[i].sub_sensor_count; j++) {
                if (g_system_config.sensors[i].sub_sensors[j].enabled) {
                    ESP_LOGI(TAG, "[FIND] Sub-sensor[%d][%d]: enabled=%d, param='%s', slave_id=%d, reg=%d", 
                             i, j, g_system_config.sensors[i].sub_sensors[j].enabled,
                             g_system_config.sensors[i].sub_sensors[j].parameter_name,
                             g_system_config.sensors[i].sub_sensors[j].slave_id,
                             g_system_config.sensors[i].sub_sensors[j].register_address);
                    char escaped_param_name[32], escaped_sub_data_type[32], escaped_sub_register_type[16];
                    html_escape(escaped_param_name, g_system_config.sensors[i].sub_sensors[j].parameter_name, sizeof(escaped_param_name));
                    html_escape(escaped_sub_data_type, g_system_config.sensors[i].sub_sensors[j].data_type, sizeof(escaped_sub_data_type));
                    html_escape(escaped_sub_register_type, g_system_config.sensors[i].sub_sensors[j].register_type, sizeof(escaped_sub_register_type));
                    
                    snprintf(chunk, sizeof(chunk),
                        "%s{parameter_name:'%s',slave_id:%d,register_address:%d,quantity:%d,data_type:'%s',scale_factor:%.3f,register_type:'%s'}",
                        first_sub_sensor ? "" : ",",
                        escaped_param_name, g_system_config.sensors[i].sub_sensors[j].slave_id,
                        g_system_config.sensors[i].sub_sensors[j].register_address, g_system_config.sensors[i].sub_sensors[j].quantity,
                        escaped_sub_data_type, g_system_config.sensors[i].sub_sensors[j].scale_factor,
                        escaped_sub_register_type[0] ? escaped_sub_register_type : "HOLDING_REGISTER");
                    httpd_resp_sendstr_chunk(req, chunk);
                    first_sub_sensor = false;
                }
            }
            httpd_resp_sendstr_chunk(req, "]");
        }
        
        // Close sensor object and add comma if not last sensor
        snprintf(chunk, sizeof(chunk), "}%s", (i < g_system_config.sensor_count - 1) ? "," : "");
        httpd_resp_sendstr_chunk(req, chunk);
    }
    httpd_resp_sendstr_chunk(req, "];");
    
    // Add script to show format info for existing sensors on page load
    httpd_resp_sendstr_chunk(req,
        "window.addEventListener('load', function() {"
        "setTimeout(() => {"
        "for(let i = 0; i < sensorData.length; i++) {"
        "const sensorCard = document.getElementById('sensor-card-' + i);"
        "if(sensorCard) {"
        "const dataType = sensorData[i].data_type;"
        "console.log('Sensor', i, 'data type:', dataType);"
        "}"
        "}"
        "}, 500);"
        "});");

    // Working edit sensor function with inline editing
    httpd_resp_sendstr_chunk(req,
        "function editSensor(sensorId){"
        "if(confirm('Edit Sensor '+(sensorId+1)+'? This will show an edit form.')){"
        "const sensor=sensorData[sensorId];"
        "if(!sensor){alert('Sensor data not found');return;}"
        "var editForm='<div class=\"sensor-card\" style=\"border:2px solid #007bff;background:#f0f8ff\">';"
        "editForm+='<h3 style=\"color:#007bff\">Editing Sensor '+(sensorId+1)+' - <span style=\"color:#28a745;font-weight:bold;\">'+(sensor.sensor_type||'Flow-Meter')+'</span></h3>';"
        "editForm+='<div class=\"form-grid\">';"
        "editForm+='<label>Name:</label><input id=\"edit_name_'+sensorId+'\" value=\"'+sensor.name+'\">';"
        "editForm+='<label>Unit ID:</label><input id=\"edit_unit_'+sensorId+'\" value=\"'+sensor.unit_id+'\">';"
        "if (sensor.sensor_type !== 'QUALITY') {"
        "editForm+='<label>Slave ID:</label><input type=\"number\" id=\"edit_slave_'+sensorId+'\" value=\"'+sensor.slave_id+'\" min=\"1\" max=\"247\">';"
        "editForm+='<label>Register:</label><input type=\"number\" id=\"edit_register_'+sensorId+'\" value=\"'+sensor.register_address+'\">';"
        "editForm+='<label>Quantity:</label><input type=\"number\" id=\"edit_quantity_'+sensorId+'\" value=\"'+sensor.quantity+'\" min=\"1\" max=\"125\">';"
        "editForm+='<label>Register Type:</label><select id=\"edit_register_type_'+sensorId+'\">';"
        "editForm+='<option value=\"HOLDING\" '+(sensor.register_type==='HOLDING'?'selected':'')+'>Holding Registers (03) - Read/Write</option>';"
        "editForm+='<option value=\"INPUT\" '+(sensor.register_type==='INPUT'?'selected':'')+'>Input Registers (04) - Read Only</option>';"
        "editForm+='<option value=\"COILS\" '+(sensor.register_type==='COILS'?'selected':'')+'>Coils (01) - Single Bit Read/Write</option>';"
        "editForm+='<option value=\"DISCRETE\" '+(sensor.register_type==='DISCRETE'?'selected':'')+'>Discrete Inputs (02) - Single Bit Read Only</option>';"
        "editForm+='</select>';"
        "}"
        "if (sensor.sensor_type === 'ZEST') {"
        "editForm+='<label>Data Type:</label><div style=\"padding:8px;background:#e8f5e8;border-radius:4px;color:#4caf50;font-weight:bold\">Fixed - INT32_BE + INT32_LE_SWAP</div><input type=\"hidden\" id=\"edit_datatype_'+sensorId+'\" value=\"ZEST_FIXED\">';"
        "} else {"
        "var mappedDataType = sensor.data_type || '';"
        "console.log('Edit form - Original data_type:', sensor.data_type, 'length:', sensor.data_type ? sensor.data_type.length : 0);"
        "if (sensor.data_type === '' || sensor.data_type === null || sensor.data_type === undefined) {"
        "  mappedDataType = 'UINT16_HI';"
        "} else if (sensor.data_type === 'FLOAT32_AB') mappedDataType = 'FLOAT32_1234';"
        "else if (sensor.data_type === 'FLOAT32_43') mappedDataType = 'FLOAT32_1234';"
        "else if (sensor.data_type === 'FLOAT32_ABCD') mappedDataType = 'FLOAT32_1234';"
        "else if (sensor.data_type === 'FLOAT32_1234') mappedDataType = 'FLOAT32_1234';"
        "else if (sensor.data_type === 'FLOAT32_DCBA') mappedDataType = 'FLOAT32_4321';"
        "else if (sensor.data_type === 'FLOAT32_4321') mappedDataType = 'FLOAT32_4321';"
        "else if (sensor.data_type === 'FLOAT32_432') mappedDataType = 'FLOAT32_4321';"
        "else if (sensor.data_type === 'FLOAT32_BADC') mappedDataType = 'FLOAT32_2143';"
        "else if (sensor.data_type === 'FLOAT32_2143') mappedDataType = 'FLOAT32_2143';"
        "else if (sensor.data_type === 'FLOAT32_214') mappedDataType = 'FLOAT32_2143';"
        "else if (sensor.data_type === 'FLOAT32_CDAB') mappedDataType = 'FLOAT32_3412';"
        "else if (sensor.data_type === 'FLOAT32_3412') mappedDataType = 'FLOAT32_3412';"
        "else if (sensor.data_type === 'FLOAT32_341') mappedDataType = 'FLOAT32_3412';"
        "else if (sensor.data_type === 'UINT32_AB') mappedDataType = 'UINT32_1234';"
        "else if (sensor.data_type === 'UINT32_ABCD') mappedDataType = 'UINT32_1234';"
        "else if (sensor.data_type === 'UINT32_1234') mappedDataType = 'UINT32_1234';"
        "else if (sensor.data_type === 'UINT32_DCBA') mappedDataType = 'UINT32_4321';"
        "else if (sensor.data_type === 'UINT32_432') mappedDataType = 'UINT32_4321';"
        "else if (sensor.data_type === 'UINT32_4321') mappedDataType = 'UINT32_4321';"
        "else if (sensor.data_type === 'UINT32_BADC') mappedDataType = 'UINT32_2143';"
        "else if (sensor.data_type === 'UINT32_2143') mappedDataType = 'UINT32_2143';"
        "else if (sensor.data_type === 'UINT32_CDAB') mappedDataType = 'UINT32_3412';"
        "else if (sensor.data_type === 'UINT32_3412') mappedDataType = 'UINT32_3412';"
        "else if (sensor.data_type === 'INT32_AB') mappedDataType = 'INT32_1234';"
        "else if (sensor.data_type === 'INT32_ABCD') mappedDataType = 'INT32_1234';"
        "else if (sensor.data_type === 'INT32_1234') mappedDataType = 'INT32_1234';"
        "else if (sensor.data_type === 'INT32_DCBA') mappedDataType = 'INT32_4321';"
        "else if (sensor.data_type === 'INT32_432') mappedDataType = 'INT32_4321';"
        "else if (sensor.data_type === 'INT32_4321') mappedDataType = 'INT32_4321';"
        "else if (sensor.data_type === 'UINT16') mappedDataType = 'UINT16_HI';"
        "else if (sensor.data_type === 'UINT16_BE') mappedDataType = 'UINT16_HI';"
        "else if (sensor.data_type === 'UINT16_HI') mappedDataType = 'UINT16_HI';"
        "else if (sensor.data_type === 'UINT16_LE') mappedDataType = 'UINT16_LO';"
        "else if (sensor.data_type === 'UINT16_LO') mappedDataType = 'UINT16_LO';"
        "else if (sensor.data_type === 'INT16') mappedDataType = 'INT16_HI';"
        "else if (sensor.data_type === 'INT16_BE') mappedDataType = 'INT16_HI';"
        "else if (sensor.data_type === 'INT16_HI') mappedDataType = 'INT16_HI';"
        "else if (sensor.data_type === 'INT16_LE') mappedDataType = 'INT16_LO';"
        "else if (sensor.data_type === 'INT16_LO') mappedDataType = 'INT16_LO';"
        "else if (sensor.data_type === 'INT32_BADC') mappedDataType = 'INT32_2143';"
        "else if (sensor.data_type === 'INT32_2143') mappedDataType = 'INT32_2143';"
        "else if (sensor.data_type === 'INT32_214') mappedDataType = 'INT32_2143';"
        "else if (sensor.data_type === 'INT32_CDAB') mappedDataType = 'INT32_3412';"
        "else if (sensor.data_type === 'INT32_3412') mappedDataType = 'INT32_3412';"
        "else if (sensor.data_type === 'INT32_341') mappedDataType = 'INT32_3412';"
        "else if (sensor.data_type === 'UINT32_DCB') mappedDataType = 'UINT32_4321';"
        "else if (sensor.data_type === 'INT32_DCB') mappedDataType = 'INT32_4321';"
        "else if (sensor.data_type === 'FLOAT32_DCB') mappedDataType = 'FLOAT32_4321';"
        "else if (sensor.data_type === 'UINT32') mappedDataType = 'UINT32_1234';"
        "else if (sensor.data_type === 'INT32') mappedDataType = 'INT32_1234';"
        "else if (sensor.data_type === 'UINT32_CDA') mappedDataType = 'UINT32_3412';"
        "else if (sensor.data_type === 'UINT32_BAD') mappedDataType = 'UINT32_2143';"
        "else if (sensor.data_type === 'UINT32_DCA') mappedDataType = 'UINT32_4321';"
        "else if (sensor.data_type === 'FLOAT32_DC') mappedDataType = 'FLOAT32_4321';"
        "else if (sensor.data_type === 'FLOAT32_CDA') mappedDataType = 'FLOAT32_3412';"
        "else if (sensor.data_type === 'FLOAT32_BAD') mappedDataType = 'FLOAT32_2143';"
        "else if (sensor.data_type === 'FLOAT32_DCA') mappedDataType = 'FLOAT32_4321';"
        "else if (sensor.data_type === 'UINT16_SWAP') mappedDataType = 'UINT16_LO';"
        "else if (sensor.data_type === 'INT16_SWAP') mappedDataType = 'INT16_LO';"
        "else if (sensor.data_type === 'UINT32_SWAP') mappedDataType = 'UINT32_4321';"
        "else if (sensor.data_type === 'UINT32_1234') mappedDataType = 'UINT32_1234';"
        "else if (sensor.data_type === 'UINT32_4321') mappedDataType = 'UINT32_4321';"
        "else if (sensor.data_type === 'UINT32_2143') mappedDataType = 'UINT32_2143';"
        "else if (sensor.data_type === 'UINT32_3412') mappedDataType = 'UINT32_3412';"
        "else if (sensor.data_type === 'INT32_SWAP') mappedDataType = 'INT32_4321';"
        "else if (sensor.data_type === 'INT32_1234') mappedDataType = 'INT32_1234';"
        "else if (sensor.data_type === 'FLOAT32_SWAP') mappedDataType = 'FLOAT32_4321';"
        "else if (sensor.data_type === 'FLOAT32') mappedDataType = 'FLOAT32_1234';"
        "else if (sensor.data_type === 'FLOAT32_1234') mappedDataType = 'FLOAT32_1234';"
        "else if (sensor.data_type === 'FLOAT32_4321') mappedDataType = 'FLOAT32_4321';"
        "else if (sensor.data_type === 'FLOAT32_2143') mappedDataType = 'FLOAT32_2143';"
        "else if (sensor.data_type === 'FLOAT32_3412') mappedDataType = 'FLOAT32_3412';"
        "else if (sensor.data_type === 'INT8') mappedDataType = 'INT8';"
        "else if (sensor.data_type === 'UINT8') mappedDataType = 'UINT8';"
        "else if (sensor.data_type === 'BOOL') mappedDataType = 'BOOL';"
        "else if (sensor.data_type === 'ASCII') mappedDataType = 'ASCII';"
        "else if (sensor.data_type === 'HEX') mappedDataType = 'HEX';"
        "else if (sensor.data_type === 'PDU') mappedDataType = 'PDU';"
        "else if (sensor.data_type === 'INT64_12345678') mappedDataType = 'INT64_12345678';"
        "else if (sensor.data_type === 'INT64_87654321') mappedDataType = 'INT64_87654321';"
        "else if (sensor.data_type === 'INT64_21436587') mappedDataType = 'INT64_21436587';"
        "else if (sensor.data_type === 'INT64_78563412') mappedDataType = 'INT64_78563412';"
        "else if (sensor.data_type === 'UINT64_12345678') mappedDataType = 'UINT64_12345678';"
        "else if (sensor.data_type === 'UINT64_87654321') mappedDataType = 'UINT64_87654321';"
        "else if (sensor.data_type === 'UINT64_21436587') mappedDataType = 'UINT64_21436587';"
        "else if (sensor.data_type === 'UINT64_78563412') mappedDataType = 'UINT64_78563412';"
        "else if (sensor.data_type === 'FLOAT64_12345678') mappedDataType = 'FLOAT64_12345678';"
        "else if (sensor.data_type === 'FLOAT64_87654321') mappedDataType = 'FLOAT64_87654321';"
        "else if (sensor.data_type === 'FLOAT64_21436587') mappedDataType = 'FLOAT64_21436587';"
        "else if (sensor.data_type === 'FLOAT64_78563412') mappedDataType = 'FLOAT64_78563412';"
        "else if (sensor.data_type === 'FLOAT64') mappedDataType = 'FLOAT64_12345678';"
        "else if (sensor.data_type === 'INT64') mappedDataType = 'INT64_12345678';"
        "else if (sensor.data_type === 'UINT64') mappedDataType = 'UINT64_12345678';"
        "else if (sensor.data_type === 'INT64_BE') mappedDataType = 'INT64_12345678';"
        "else if (sensor.data_type === 'INT64_LE') mappedDataType = 'INT64_87654321';"
        "else if (sensor.data_type === 'UINT64_BE') mappedDataType = 'UINT64_12345678';"
        "else if (sensor.data_type === 'UINT64_LE') mappedDataType = 'UINT64_87654321';"
        "else if (sensor.data_type === 'FLOAT64_BE') mappedDataType = 'FLOAT64_12345678';"
        "else if (sensor.data_type === 'FLOAT64_LE') mappedDataType = 'FLOAT64_87654321';"
        "else {"
        "  console.warn('Edit form - UNMAPPED data_type:', sensor.data_type, '- using as-is');"
        "  mappedDataType = sensor.data_type || 'UINT16_HI';"
        "}"
        "console.log('Edit form - Mapped data_type:', mappedDataType);"
        "editForm+='<label>Data Type:</label><select id=\"edit_datatype_'+sensorId+'\" style=\"width:280px\">';"
        "editForm+='<option value=\"UINT16_HI\" '+(mappedDataType===''||mappedDataType==='UINT16_HI'?'selected':'')+'>16-bit UINT, high byte first (Default)</option>';"
        "editForm+='<optgroup label=\"8-bit Formats (0.5 register)\">';"
        "editForm+='<option value=\"INT8\" '+(mappedDataType==='INT8'?'selected':'')+'>8-bit INT (-128 to 127)</option>';"
        "editForm+='<option value=\"UINT8\" '+(mappedDataType==='UINT8'?'selected':'')+'>8-bit UINT (0 to 255)</option>';"
        "editForm+='</optgroup>';"
        "editForm+='<optgroup label=\"16-bit Formats (1 register)\">';"
        "editForm+='<option value=\"INT16_HI\" '+(mappedDataType==='INT16_HI'?'selected':'')+'>16-bit INT, high byte first</option>';"
        "editForm+='<option value=\"INT16_LO\" '+(mappedDataType==='INT16_LO'?'selected':'')+'>16-bit INT, low byte first</option>';"
        "editForm+='<option value=\"UINT16_LO\" '+(mappedDataType==='UINT16_LO'?'selected':'')+'>16-bit UINT, low byte first</option>';"
        "editForm+='</optgroup>';"
        "editForm+='<optgroup label=\"32-bit Float Formats (2 registers)\">';"
        "var float32_1234_selected = (mappedDataType==='FLOAT32_1234');"
        "editForm+='<option value=\"FLOAT32_1234\" '+(float32_1234_selected?'selected':'')+'>32-bit float, Byte order 1,2,3,4</option>';"
        "editForm+='<option value=\"FLOAT32_4321\" '+(mappedDataType==='FLOAT32_4321'?'selected':'')+'>32-bit float, Byte order 4,3,2,1</option>';"
        "editForm+='<option value=\"FLOAT32_2143\" '+(mappedDataType==='FLOAT32_2143'?'selected':'')+'>32-bit float, Byte order 2,1,4,3</option>';"
        "editForm+='<option value=\"FLOAT32_3412\" '+(mappedDataType==='FLOAT32_3412'?'selected':'')+'>32-bit float, Byte order 3,4,1,2</option>';"
        "editForm+='</optgroup>';"
        "editForm+='<optgroup label=\"32-bit Integer Formats (2 registers)\">';"
        "editForm+='<option value=\"INT32_1234\" '+(mappedDataType==='INT32_1234'?'selected':'')+'>32-bit INT, Byte order 1,2,3,4</option>';"
        "editForm+='<option value=\"INT32_4321\" '+(mappedDataType==='INT32_4321'?'selected':'')+'>32-bit INT, Byte order 4,3,2,1</option>';"
        "editForm+='<option value=\"INT32_2143\" '+(mappedDataType==='INT32_2143'?'selected':'')+'>32-bit INT, Byte order 2,1,4,3</option>';"
        "editForm+='<option value=\"INT32_3412\" '+(mappedDataType==='INT32_3412'?'selected':'')+'>32-bit INT, Byte order 3,4,1,2</option>';"
        "editForm+='<option value=\"UINT32_1234\" '+(mappedDataType==='UINT32_1234'?'selected':'')+'>32-bit UINT, Byte order 1,2,3,4</option>';"
        "editForm+='<option value=\"UINT32_4321\" '+(mappedDataType==='UINT32_4321'?'selected':'')+'>32-bit UINT, Byte order 4,3,2,1</option>';"
        "editForm+='<option value=\"UINT32_2143\" '+(mappedDataType==='UINT32_2143'?'selected':'')+'>32-bit UINT, Byte order 2,1,4,3</option>';"
        "editForm+='<option value=\"UINT32_3412\" '+(mappedDataType==='UINT32_3412'?'selected':'')+'>32-bit UINT, Byte order 3,4,1,2</option>';"
        "editForm+='</optgroup>';"
        "editForm+='<optgroup label=\"64-bit Integer Formats (4 registers)\">';"
        "editForm+='<option value=\"INT64_12345678\" '+(mappedDataType==='INT64_12345678'?'selected':'')+'>64-bit INT, Byte order 1,2,3,4,5,6,7,8</option>';"
        "editForm+='<option value=\"INT64_87654321\" '+(mappedDataType==='INT64_87654321'?'selected':'')+'>64-bit INT, Byte order 8,7,6,5,4,3,2,1</option>';"
        "editForm+='<option value=\"INT64_21436587\" '+(mappedDataType==='INT64_21436587'?'selected':'')+'>64-bit INT, Byte order 2,1,4,3,6,5,8,7</option>';"
        "editForm+='<option value=\"INT64_78563412\" '+(mappedDataType==='INT64_78563412'?'selected':'')+'>64-bit INT, Byte order 7,8,5,6,3,4,1,2</option>';"
        "editForm+='<option value=\"UINT64_12345678\" '+(mappedDataType==='UINT64_12345678'?'selected':'')+'>64-bit UINT, Byte order 1,2,3,4,5,6,7,8</option>';"
        "editForm+='<option value=\"UINT64_87654321\" '+(mappedDataType==='UINT64_87654321'?'selected':'')+'>64-bit UINT, Byte order 8,7,6,5,4,3,2,1</option>';"
        "editForm+='<option value=\"UINT64_21436587\" '+(mappedDataType==='UINT64_21436587'?'selected':'')+'>64-bit UINT, Byte order 2,1,4,3,6,5,8,7</option>';"
        "editForm+='<option value=\"UINT64_78563412\" '+(mappedDataType==='UINT64_78563412'?'selected':'')+'>64-bit UINT, Byte order 7,8,5,6,3,4,1,2</option>';"
        "editForm+='</optgroup>';"
        "editForm+='<optgroup label=\"64-bit Float Formats (4 registers)\">';"
        "editForm+='<option value=\"FLOAT64_12345678\" '+(mappedDataType==='FLOAT64_12345678'?'selected':'')+'>64-bit float, Byte order 1,2,3,4,5,6,7,8</option>';"
        "editForm+='<option value=\"FLOAT64_87654321\" '+(mappedDataType==='FLOAT64_87654321'?'selected':'')+'>64-bit float, Byte order 8,7,6,5,4,3,2,1</option>';"
        "editForm+='<option value=\"FLOAT64_21436587\" '+(mappedDataType==='FLOAT64_21436587'?'selected':'')+'>64-bit float, Byte order 2,1,4,3,6,5,8,7</option>';"
        "editForm+='<option value=\"FLOAT64_78563412\" '+(mappedDataType==='FLOAT64_78563412'?'selected':'')+'>64-bit float, Byte order 7,8,5,6,3,4,1,2</option>';"
        "editForm+='</optgroup>';"
        "editForm+='<optgroup label=\"Special Formats\">';"
        "editForm+='<option value=\"ASCII\" '+(mappedDataType==='ASCII'?'selected':'')+'>ASCII String</option>';"
        "editForm+='<option value=\"HEX\" '+(mappedDataType==='HEX'?'selected':'')+'>Hexadecimal</option>';"
        "editForm+='<option value=\"BOOL\" '+(mappedDataType==='BOOL'?'selected':'')+'>Boolean (True/False)</option>';"
        "editForm+='<option value=\"PDU\" '+(mappedDataType==='PDU'?'selected':'')+'>PDU (Protocol Data Unit)</option>';"
        "editForm+='</optgroup></select>';"
        "}"
        "editForm+='<label>Baud Rate:</label><select id=\"edit_baud_'+sensorId+'\">';"
        "editForm+='<option value=\"2400\" '+(sensor.baud_rate==2400?'selected':'')+'>2400 bps</option>';"
        "editForm+='<option value=\"4800\" '+(sensor.baud_rate==4800?'selected':'')+'>4800 bps</option>';"
        "editForm+='<option value=\"9600\" '+(sensor.baud_rate==9600?'selected':'')+'>9600 bps</option>';"
        "editForm+='<option value=\"14400\" '+(sensor.baud_rate==14400?'selected':'')+'>14400 bps</option>';"
        "editForm+='<option value=\"19200\" '+(sensor.baud_rate==19200?'selected':'')+'>19200 bps</option>';"
        "editForm+='<option value=\"28800\" '+(sensor.baud_rate==28800?'selected':'')+'>28800 bps</option>';"
        "editForm+='<option value=\"38400\" '+(sensor.baud_rate==38400?'selected':'')+'>38400 bps</option>';"
        "editForm+='<option value=\"57600\" '+(sensor.baud_rate==57600?'selected':'')+'>57600 bps</option>';"
        "editForm+='<option value=\"115200\" '+(sensor.baud_rate==115200?'selected':'')+'>115200 bps</option>';"
        "editForm+='<option value=\"230400\" '+(sensor.baud_rate==230400?'selected':'')+'>230400 bps</option>';"
        "editForm+='</select>';"
        "editForm+='<label>Parity:</label><select id=\"edit_parity_'+sensorId+'\">';"
        "editForm+='<option value=\"none\" '+(sensor.parity==='none'||!sensor.parity?'selected':'')+'>None</option>';"
        "editForm+='<option value=\"even\" '+(sensor.parity==='even'?'selected':'')+'>Even</option>';"
        "editForm+='<option value=\"odd\" '+(sensor.parity==='odd'?'selected':'')+'>Odd</option>';"
        "editForm+='</select>';"
        "if (sensor.sensor_type === 'Level') {"
        "editForm+='<label>Sensor Height:</label><div><input type=\"number\" id=\"edit_sensor_height_'+sensorId+'\" value=\"'+sensor.sensor_height+'\" step=\"any\"><small style=\"color:#666;margin-left:8px\">(meters or your unit)</small></div>';"
        "editForm+='<label>Max Water Level:</label><div><input type=\"number\" id=\"edit_max_water_level_'+sensorId+'\" value=\"'+sensor.max_water_level+'\" step=\"any\"><small style=\"color:#666;margin-left:8px\">(meters or your unit)</small></div>';"
        "} else if (sensor.sensor_type === 'Radar Level') {"
        "editForm+='<label>Max Water Level *:</label><div><input type=\"number\" id=\"edit_max_water_level_'+sensorId+'\" value=\"'+sensor.max_water_level+'\" step=\"any\" style=\"border:2px solid #dc3545\" required><small style=\"color:#666;margin-left:8px\">(meters or your unit)</small></div>';"
        "} else if (sensor.sensor_type === 'ENERGY') {"
        "editForm+='<label>Meter Type:</label><div><input type=\"text\" id=\"edit_meter_type_'+sensorId+'\" value=\"'+(sensor.meter_type||'')+'\" placeholder=\"e.g., Power Meter\"><small style=\"color:#666;margin-left:8px\">(Used as meter identifier)</small></div>';"
        "} else if (sensor.sensor_type === 'QUALITY') {"
        "editForm+='<p style=\"color:#17a2b8;font-weight:bold;margin:10px 0\">[TEST] Water Quality Sensor - Sub-Sensors Configuration</p>';"
        "editForm+='<div style=\"margin-top:20px;padding:15px;background:#f0f8ff;border:2px dashed #17a2b8;border-radius:8px\">';"
        "editForm+='<h4 style=\"color:#17a2b8;margin-top:0\">[PARAM] Sub-Sensors - Water Quality Parameters</h4>';"
        "editForm+='<div id=\"edit-sub-sensors-'+sensorId+'\" style=\"margin:10px 0\"></div>';"
        "editForm+='<button type=\"button\" onclick=\"addEditSubSensor('+sensorId+')\" style=\"background:#28a745;color:white;padding:8px 16px;border:none;border-radius:4px;font-size:14px;cursor:pointer\">➕ Add Sub-Sensor</button>';"
        "editForm+='</div>';"
        "} else if (sensor.sensor_type === 'RAINGAUGE') {"
        "console.log('Edit form: Creating RAINGAUGE sensor edit form for sensor', sensorId, 'with scale_factor:', sensor.scale_factor);"
        "editForm+='<label>Scale Factor:</label><div><input type=\"number\" id=\"edit_scale_factor_'+sensorId+'\" value=\"'+(sensor.scale_factor||1.0)+'\" step=\"any\" style=\"width:100px\"><small style=\"color:#666;margin-left:8px\">(Rain gauge scaling - e.g., 0.1 for mm/tips)</small></div>';"
        "} else if (sensor.sensor_type === 'BOREWELL') {"
        "console.log('Edit form: Creating BOREWELL sensor edit form for sensor', sensorId, 'with scale_factor:', sensor.scale_factor);"
        "editForm+='<label>Scale Factor:</label><div><input type=\"number\" id=\"edit_scale_factor_'+sensorId+'\" value=\"'+(sensor.scale_factor||1.0)+'\" step=\"any\" style=\"width:100px\"><small style=\"color:#666;margin-left:8px\">(Borewell sensor scaling)</small></div>';"
        "} else {"
        "console.log('Edit form: Creating generic sensor edit form for sensor', sensorId, 'type:', sensor.sensor_type, 'scale_factor:', sensor.scale_factor);"
        "editForm+='<label>Scale Factor:</label><input type=\"number\" id=\"edit_scale_factor_'+sensorId+'\" value=\"'+(sensor.scale_factor||1.0)+'\" step=\"any\">';"
        "}"
        "editForm+='</div>';"
        "editForm+='<div id=\"test-result-edit-'+sensorId+'\" style=\"margin:10px 0;display:none\"></div>';"
        "editForm+='<div style=\"display:flex;gap:10px;margin-top:15px;justify-content:flex-start;align-items:center\">';"
        "editForm+='<button type=\"button\" onclick=\"saveSensorEdit('+sensorId+')\" style=\"background:#28a745;color:white;padding:10px 20px;border:none;border-radius:4px;font-weight:bold;min-width:120px;cursor:pointer\">Save Changes</button>';"
        "editForm+='<button type=\"button\" onclick=\"testEditSensorRS485('+sensorId+')\" style=\"background:#007bff;color:white;padding:10px 20px;border:none;border-radius:4px;font-weight:bold;min-width:120px;cursor:pointer\">Test RS485</button>';"
        "editForm+='<button type=\"button\" onclick=\"cancelSensorEdit('+sensorId+')\" style=\"background:#6c757d;color:white;padding:10px 20px;border:none;border-radius:4px;font-weight:bold;min-width:120px;cursor:pointer\">Cancel</button>';"
        "editForm+='</div>';"
        "editForm+='</div>';"
        "document.getElementById('sensor-card-'+sensorId).outerHTML=editForm;"
        "setTimeout(() => {"
        "const editSelect = document.getElementById('edit_datatype_' + sensorId);"
        "if (editSelect) {"
        "if (editSelect.value !== sensor.data_type) {"
        "let mappedType = sensor.data_type;"
        "if (sensor.data_type === '' || sensor.data_type === null || sensor.data_type === undefined) {"
        "mappedType = 'UINT16_HI';"
        "} else if (sensor.data_type === 'FLOAT32_AB') mappedType = 'FLOAT32_1234';"
        "else if (sensor.data_type === 'FLOAT32_43') mappedType = 'FLOAT32_1234';"
        "else if (sensor.data_type === 'FLOAT32_ABCD') mappedType = 'FLOAT32_1234';"
        "else if (sensor.data_type === 'FLOAT32_1234') mappedType = 'FLOAT32_1234';"
        "else if (sensor.data_type === 'FLOAT32_DCBA') mappedType = 'FLOAT32_4321';"
        "else if (sensor.data_type === 'FLOAT32_4321') mappedType = 'FLOAT32_4321';"
        "else if (sensor.data_type === 'FLOAT32_432') mappedType = 'FLOAT32_4321';"
        "else if (sensor.data_type === 'FLOAT32_BADC') mappedType = 'FLOAT32_2143';"
        "else if (sensor.data_type === 'FLOAT32_2143') mappedType = 'FLOAT32_2143';"
        "else if (sensor.data_type === 'FLOAT32_214') mappedType = 'FLOAT32_2143';"
        "else if (sensor.data_type === 'FLOAT32_CDAB') mappedType = 'FLOAT32_3412';"
        "else if (sensor.data_type === 'FLOAT32_3412') mappedType = 'FLOAT32_3412';"
        "else if (sensor.data_type === 'FLOAT32_341') mappedType = 'FLOAT32_3412';"
        "else if (sensor.data_type === 'UINT32_AB') mappedType = 'UINT32_1234';"
        "else if (sensor.data_type === 'UINT32_ABCD') mappedType = 'UINT32_1234';"
        "else if (sensor.data_type === 'UINT32_1234') mappedType = 'UINT32_1234';"
        "else if (sensor.data_type === 'UINT32_DCBA') mappedType = 'UINT32_4321';"
        "else if (sensor.data_type === 'UINT32_4321') mappedType = 'UINT32_4321';"
        "else if (sensor.data_type === 'UINT32_432') mappedType = 'UINT32_4321';"
        "else if (sensor.data_type === 'UINT32_BADC') mappedType = 'UINT32_2143';"
        "else if (sensor.data_type === 'UINT32_2143') mappedType = 'UINT32_2143';"
        "else if (sensor.data_type === 'UINT32_214') mappedType = 'UINT32_2143';"
        "else if (sensor.data_type === 'UINT32_CDAB') mappedType = 'UINT32_3412';"
        "else if (sensor.data_type === 'UINT32_3412') mappedType = 'UINT32_3412';"
        "else if (sensor.data_type === 'UINT32_341') mappedType = 'UINT32_3412';"
        "else if (sensor.data_type === 'INT32_AB') mappedType = 'INT32_1234';"
        "else if (sensor.data_type === 'INT32_ABCD') mappedType = 'INT32_1234';"
        "else if (sensor.data_type === 'INT32_1234') mappedType = 'INT32_1234';"
        "else if (sensor.data_type === 'INT32_DCBA') mappedType = 'INT32_4321';"
        "else if (sensor.data_type === 'INT32_4321') mappedType = 'INT32_4321';"
        "else if (sensor.data_type === 'INT32_432') mappedType = 'INT32_4321';"
        "else if (sensor.data_type === 'INT32_BADC') mappedType = 'INT32_2143';"
        "else if (sensor.data_type === 'INT32_2143') mappedType = 'INT32_2143';"
        "else if (sensor.data_type === 'INT32_214') mappedType = 'INT32_2143';"
        "else if (sensor.data_type === 'INT32_CDAB') mappedType = 'INT32_3412';"
        "else if (sensor.data_type === 'INT32_3412') mappedType = 'INT32_3412';"
        "else if (sensor.data_type === 'INT32_341') mappedType = 'INT32_3412';"
        "else if (sensor.data_type === 'UINT32_DCB') mappedType = 'UINT32_4321';"
        "else if (sensor.data_type === 'INT32_DCB') mappedType = 'INT32_4321';"
        "else if (sensor.data_type === 'FLOAT32_DCB') mappedType = 'FLOAT32_4321';"
        "else if (sensor.data_type === 'UINT32') mappedType = 'UINT32_1234';"
        "else if (sensor.data_type === 'INT32') mappedType = 'INT32_1234';"
        "else if (sensor.data_type === 'UINT32_CDA') mappedType = 'UINT32_3412';"
        "else if (sensor.data_type === 'UINT32_BAD') mappedType = 'UINT32_2143';"
        "else if (sensor.data_type === 'UINT32_DCA') mappedType = 'UINT32_4321';"
        "else if (sensor.data_type === 'INT32_CDA') mappedType = 'INT32_3412';"
        "else if (sensor.data_type === 'INT32_BAD') mappedType = 'INT32_2143';"
        "else if (sensor.data_type === 'INT32_DCA') mappedType = 'INT32_4321';"
        "else if (sensor.data_type === 'FLOAT32_DC') mappedType = 'FLOAT32_4321';"
        "else if (sensor.data_type === 'FLOAT32_CDA') mappedType = 'FLOAT32_3412';"
        "else if (sensor.data_type === 'FLOAT32_BAD') mappedType = 'FLOAT32_2143';"
        "else if (sensor.data_type === 'FLOAT32_DCA') mappedType = 'FLOAT32_4321';"
        "else if (sensor.data_type === 'UINT16_B') mappedType = 'UINT16_HI';"
        "else if (sensor.data_type === 'UINT16_BE') mappedType = 'UINT16_HI';"
        "else if (sensor.data_type === 'UINT16_HI') mappedType = 'UINT16_HI';"
        "else if (sensor.data_type === 'UINT16_L') mappedType = 'UINT16_LO';"
        "else if (sensor.data_type === 'UINT16_LE') mappedType = 'UINT16_LO';"
        "else if (sensor.data_type === 'UINT16_LO') mappedType = 'UINT16_LO';"
        "else if (sensor.data_type === 'UINT16') mappedType = 'UINT16_HI';"
        "else if (sensor.data_type === 'INT16_B') mappedType = 'INT16_HI';"
        "else if (sensor.data_type === 'INT16_BE') mappedType = 'INT16_HI';"
        "else if (sensor.data_type === 'INT16_HI') mappedType = 'INT16_HI';"
        "else if (sensor.data_type === 'INT16_L') mappedType = 'INT16_LO';"
        "else if (sensor.data_type === 'INT16_LE') mappedType = 'INT16_LO';"
        "else if (sensor.data_type === 'INT16_LO') mappedType = 'INT16_LO';"
        "else if (sensor.data_type === 'INT16') mappedType = 'INT16_HI';"
        "else if (sensor.data_type === 'UINT8') mappedType = 'UINT8';"
        "else if (sensor.data_type === 'INT8') mappedType = 'INT8';"
        "else if (sensor.data_type === 'ASCII') mappedType = 'ASCII';"
        "else if (sensor.data_type === 'HEX') mappedType = 'HEX';"
        "else if (sensor.data_type === 'BOOL') mappedType = 'BOOL';"
        "else if (sensor.data_type === 'PDU') mappedType = 'PDU';"
        "else if (sensor.data_type === 'INT64_12345678') mappedType = 'INT64_12345678';"
        "else if (sensor.data_type === 'INT64_87654321') mappedType = 'INT64_87654321';"
        "else if (sensor.data_type === 'INT64_21436587') mappedType = 'INT64_21436587';"
        "else if (sensor.data_type === 'INT64_78563412') mappedType = 'INT64_78563412';"
        "else if (sensor.data_type === 'UINT64_12345678') mappedType = 'UINT64_12345678';"
        "else if (sensor.data_type === 'UINT64_87654321') mappedType = 'UINT64_87654321';"
        "else if (sensor.data_type === 'UINT64_21436587') mappedType = 'UINT64_21436587';"
        "else if (sensor.data_type === 'UINT64_78563412') mappedType = 'UINT64_78563412';"
        "else if (sensor.data_type === 'FLOAT64_12345678') mappedType = 'FLOAT64_12345678';"
        "else if (sensor.data_type === 'FLOAT64_87654321') mappedType = 'FLOAT64_87654321';"
        "else if (sensor.data_type === 'FLOAT64_21436587') mappedType = 'FLOAT64_21436587';"
        "else if (sensor.data_type === 'FLOAT64_78563412') mappedType = 'FLOAT64_78563412';"
        "else if (sensor.data_type === 'INT64') mappedType = 'INT64_12345678';"
        "else if (sensor.data_type === 'UINT64') mappedType = 'UINT64_12345678';"
        "else if (sensor.data_type === 'FLOAT64') mappedType = 'FLOAT64_12345678';"
        "else if (sensor.data_type === 'INT64_BE') mappedType = 'INT64_12345678';"
        "else if (sensor.data_type === 'INT64_LE') mappedType = 'INT64_87654321';"
        "else if (sensor.data_type === 'UINT64_BE') mappedType = 'UINT64_12345678';"
        "else if (sensor.data_type === 'UINT64_LE') mappedType = 'UINT64_87654321';"
        "else if (sensor.data_type === 'FLOAT64_BE') mappedType = 'FLOAT64_12345678';"
        "else if (sensor.data_type === 'FLOAT64_LE') mappedType = 'FLOAT64_87654321';"
        "editSelect.value = mappedType;"
        "if (!editSelect.value) {"
        "for (let option of editSelect.options) {"
        "if (option.value === mappedType) {"
        "option.selected = true;"
        "editSelect.value = mappedType;"
        "break;"
        "}"
        "}"
        "if (!editSelect.value) {"
        "}"
        "}"
        "} else {"
        "}"
        "} else {"
        "}"
        "if (sensor.sensor_type === 'QUALITY') {"
        "loadEditSubSensors(sensorId);"
        "} else {"
        "}"
        "}, 500);"
        "}}");
    
    // Functions for editing water quality sub-sensors
    httpd_resp_sendstr_chunk(req,
        "function loadEditSubSensors(sensorId) {"
        "const sensor = sensorData[sensorId];"
        "if (!sensor) {"
        "console.error('[ERROR] ERROR: No sensor data found for sensor', sensorId);"
        "return;"
        "}"
        "if (!sensor.sub_sensors) {"
        "console.error('[ERROR] ERROR: No sub_sensors property found for sensor', sensorId);"
        "return;"
        "}"
        "const subSensorsDiv = document.getElementById('edit-sub-sensors-' + sensorId);"
        "if (!subSensorsDiv) {"
        "console.error('Sub-sensors div not found for sensor', sensorId);"
        "return;"
        "}"
        "subSensorsDiv.innerHTML = '';"
        "for (let i = 0; i < sensor.sub_sensors.length; i++) {"
        "const subSensor = sensor.sub_sensors[i];"
        "if (subSensor.parameter_name) {"
        "const subSensorId = 'edit-sub-sensor-' + sensorId + '-' + i;"
        "let h = '<div class=\"sub-sensor\" id=\"' + subSensorId + '\" style=\"border:1px solid #28a745;padding:10px;margin:10px 0;background:#f8fff8;border-radius:5px\">';"
        "h += '<h5 style=\"color:#28a745;margin-top:0\">[PARAM] Sub-Sensor ' + (i + 1) + '</h5>';"
        "h += '<div style=\"display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin:10px 0\">';"
        "h += '<div><label style=\"font-weight:bold\">Parameter Type:</label><br><select name=\"sensor_' + sensorId + '_sub_' + i + '_parameter\" style=\"width:100%;padding:5px\">';"
        "for (let j = 0; j < qualityParameterTypes.length; j++) {"
        "const param = qualityParameterTypes[j];"
        "const selected = (param.key === subSensor.parameter_name) ? ' selected' : '';"
        "h += '<option value=\"' + param.key + '\"' + selected + '>' + param.name + ' (' + param.units + ')</option>';"
        "}"
        "h += '</select></div>';"
        "h += '<div><label style=\"font-weight:bold\">Slave ID:</label><br><input type=\"number\" name=\"sensor_' + sensorId + '_sub_' + i + '_slave_id\" value=\"' + subSensor.slave_id + '\" min=\"1\" max=\"247\" style=\"width:100%;padding:5px\"></div>';"
        "h += '<div><label style=\"font-weight:bold\">Register:</label><br><input type=\"number\" name=\"sensor_' + sensorId + '_sub_' + i + '_register\" value=\"' + subSensor.register_address + '\" min=\"0\" max=\"65535\" style=\"width:100%;padding:5px\"></div>';"
        "h += '</div>';"
        "h += '<div style=\"display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin:10px 0\">';"
        "h += '<div><label style=\"font-weight:bold\">Quantity:</label><br><input type=\"number\" name=\"sensor_' + sensorId + '_sub_' + i + '_quantity\" value=\"' + subSensor.quantity + '\" min=\"1\" max=\"10\" style=\"width:100%;padding:5px\"></div>';"
        "h += '<div><label style=\"font-weight:bold\">Register Type:</label><br><select name=\"sensor_' + sensorId + '_sub_' + i + '_register_type\" style=\"width:100%;padding:5px\">';"
        "const inputSelected = (subSensor.register_type === 'INPUT_REGISTER') ? ' selected' : '';"
        "const holdingSelected = (subSensor.register_type === 'HOLDING_REGISTER') ? ' selected' : '';"
        "h += '<option value=\"INPUT_REGISTER\"' + inputSelected + '>Input Register</option>';"
        "h += '<option value=\"HOLDING_REGISTER\"' + holdingSelected + '>Holding Register</option>';"
        "h += '</select></div>';"
        "h += '<div><label style=\"font-weight:bold\">Scale Factor:</label><br><input type=\"number\" name=\"sensor_' + sensorId + '_sub_' + i + '_scale_factor\" value=\"' + subSensor.scale_factor + '\" step=\"any\" style=\"width:100%;padding:5px\"></div>';"
        "h += '</div>';"
        "h += '<div style=\"display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:10px 0\">';"
        "h += '<div><label style=\"font-weight:bold\">Data Type:</label><br><select name=\"sensor_' + sensorId + '_sub_' + i + '_data_type\" style=\"width:100%;padding:5px\">';"
        "h += '<optgroup label=\"8-bit Integer Types\">';"
        "const int8Selected = (subSensor.data_type === 'INT8') ? ' selected' : '';"
        "const uint8Selected = (subSensor.data_type === 'UINT8') ? ' selected' : '';"
        "h += '<option value=\"INT8\"' + int8Selected + '>INT8 - 8-bit Signed</option>';"
        "h += '<option value=\"UINT8\"' + uint8Selected + '>UINT8 - 8-bit Unsigned</option>';"
        "h += '</optgroup>';"
        "h += '<optgroup label=\"16-bit Integer Types\">';"
        "const int16Selected = (subSensor.data_type === 'INT16') ? ' selected' : '';"
        "const uint16Selected = (subSensor.data_type === 'UINT16') ? ' selected' : '';"
        "h += '<option value=\"INT16\"' + int16Selected + '>INT16 - 16-bit Signed</option>';"
        "h += '<option value=\"UINT16\"' + uint16Selected + '>UINT16 - 16-bit Unsigned</option>';"
        "h += '</optgroup>';"
        "h += '<optgroup label=\"32-bit Integer Types\">';"
        "const int32AbcdSelected = (subSensor.data_type === 'INT32_ABCD') ? ' selected' : '';"
        "const int32DcbaSelected = (subSensor.data_type === 'INT32_DCBA') ? ' selected' : '';"
        "const int32BadcSelected = (subSensor.data_type === 'INT32_BADC') ? ' selected' : '';"
        "const int32CdabSelected = (subSensor.data_type === 'INT32_CDAB') ? ' selected' : '';"
        "const uint32AbcdSelected = (subSensor.data_type === 'UINT32_ABCD') ? ' selected' : '';"
        "const uint32DcbaSelected = (subSensor.data_type === 'UINT32_DCBA') ? ' selected' : '';"
        "const uint32BadcSelected = (subSensor.data_type === 'UINT32_BADC') ? ' selected' : '';"
        "const uint32CdabSelected = (subSensor.data_type === 'UINT32_CDAB') ? ' selected' : '';"
        "h += '<option value=\"INT32_ABCD\"' + int32AbcdSelected + '>INT32_ABCD - Big Endian</option>';"
        "h += '<option value=\"INT32_DCBA\"' + int32DcbaSelected + '>INT32_DCBA - Little Endian</option>';"
        "h += '<option value=\"INT32_BADC\"' + int32BadcSelected + '>INT32_BADC - Mid-Big Endian</option>';"
        "h += '<option value=\"INT32_CDAB\"' + int32CdabSelected + '>INT32_CDAB - Mid-Little Endian</option>';"
        "h += '<option value=\"UINT32_ABCD\"' + uint32AbcdSelected + '>UINT32_ABCD - Big Endian</option>';"
        "h += '<option value=\"UINT32_DCBA\"' + uint32DcbaSelected + '>UINT32_DCBA - Little Endian</option>';"
        "h += '<option value=\"UINT32_BADC\"' + uint32BadcSelected + '>UINT32_BADC - Mid-Big Endian</option>';"
        "h += '<option value=\"UINT32_CDAB\"' + uint32CdabSelected + '>UINT32_CDAB - Mid-Little Endian</option>';"
        "h += '</optgroup>';"
        "h += '<optgroup label=\"32-bit Float Types\">';"
        "const float32AbcdSelected = (subSensor.data_type === 'FLOAT32_ABCD') ? ' selected' : '';"
        "const float32DcbaSelected = (subSensor.data_type === 'FLOAT32_DCBA') ? ' selected' : '';"
        "const float32BadcSelected = (subSensor.data_type === 'FLOAT32_BADC') ? ' selected' : '';"
        "const float32CdabSelected = (subSensor.data_type === 'FLOAT32_CDAB') ? ' selected' : '';"
        "h += '<option value=\"FLOAT32_ABCD\"' + float32AbcdSelected + '>FLOAT32_ABCD - Big Endian</option>';"
        "h += '<option value=\"FLOAT32_DCBA\"' + float32DcbaSelected + '>FLOAT32_DCBA - Little Endian</option>';"
        "h += '<option value=\"FLOAT32_BADC\"' + float32BadcSelected + '>FLOAT32_BADC - Mid-Big Endian</option>';"
        "h += '<option value=\"FLOAT32_CDAB\"' + float32CdabSelected + '>FLOAT32_CDAB - Mid-Little Endian</option>';"
        "h += '</optgroup>';"
        "h += '<optgroup label=\"64-bit Integer Types\">';"
        "const int64_1Selected = (subSensor.data_type === 'INT64_12345678') ? ' selected' : '';"
        "const int64_2Selected = (subSensor.data_type === 'INT64_87654321') ? ' selected' : '';"
        "const int64_3Selected = (subSensor.data_type === 'INT64_21436587') ? ' selected' : '';"
        "const int64_4Selected = (subSensor.data_type === 'INT64_78563412') ? ' selected' : '';"
        "const uint64_1Selected = (subSensor.data_type === 'UINT64_12345678') ? ' selected' : '';"
        "const uint64_2Selected = (subSensor.data_type === 'UINT64_87654321') ? ' selected' : '';"
        "const uint64_3Selected = (subSensor.data_type === 'UINT64_21436587') ? ' selected' : '';"
        "const uint64_4Selected = (subSensor.data_type === 'UINT64_78563412') ? ' selected' : '';"
        "h += '<option value=\"INT64_12345678\"' + int64_1Selected + '>INT64_12345678 - Big Endian</option>';"
        "h += '<option value=\"INT64_87654321\"' + int64_2Selected + '>INT64_87654321 - Little Endian</option>';"
        "h += '<option value=\"INT64_21436587\"' + int64_3Selected + '>INT64_21436587 - Mid-Big Endian</option>';"
        "h += '<option value=\"INT64_78563412\"' + int64_4Selected + '>INT64_78563412 - Mid-Little Endian</option>';"
        "h += '<option value=\"UINT64_12345678\"' + uint64_1Selected + '>UINT64_12345678 - Big Endian</option>';"
        "h += '<option value=\"UINT64_87654321\"' + uint64_2Selected + '>UINT64_87654321 - Little Endian</option>';"
        "h += '<option value=\"UINT64_21436587\"' + uint64_3Selected + '>UINT64_21436587 - Mid-Big Endian</option>';"
        "h += '<option value=\"UINT64_78563412\"' + uint64_4Selected + '>UINT64_78563412 - Mid-Little Endian</option>';"
        "h += '</optgroup>';"
        "h += '<optgroup label=\"64-bit Float Types\">';"
        "const float64_1Selected = (subSensor.data_type === 'FLOAT64_12345678') ? ' selected' : '';"
        "const float64_2Selected = (subSensor.data_type === 'FLOAT64_87654321') ? ' selected' : '';"
        "const float64_3Selected = (subSensor.data_type === 'FLOAT64_21436587') ? ' selected' : '';"
        "const float64_4Selected = (subSensor.data_type === 'FLOAT64_78563412') ? ' selected' : '';"
        "h += '<option value=\"FLOAT64_12345678\"' + float64_1Selected + '>FLOAT64_12345678 - Big Endian</option>';"
        "h += '<option value=\"FLOAT64_87654321\"' + float64_2Selected + '>FLOAT64_87654321 - Little Endian</option>';"
        "h += '<option value=\"FLOAT64_21436587\"' + float64_3Selected + '>FLOAT64_21436587 - Mid-Big Endian</option>';"
        "h += '<option value=\"FLOAT64_78563412\"' + float64_4Selected + '>FLOAT64_78563412 - Mid-Little Endian</option>';"
        "h += '</optgroup>';"
        "h += '<optgroup label=\"Special Types\">';"
        "const asciiSelected = (subSensor.data_type === 'ASCII') ? ' selected' : '';"
        "const hexSelected = (subSensor.data_type === 'HEX') ? ' selected' : '';"
        "const boolSelected = (subSensor.data_type === 'BOOL') ? ' selected' : '';"
        "const pduSelected = (subSensor.data_type === 'PDU') ? ' selected' : '';"
        "h += '<option value=\"ASCII\"' + asciiSelected + '>ASCII String</option>';"
        "h += '<option value=\"HEX\"' + hexSelected + '>Hexadecimal</option>';"
        "h += '<option value=\"BOOL\"' + boolSelected + '>Boolean</option>';"
        "h += '<option value=\"PDU\"' + pduSelected + '>PDU (Protocol Data Unit)</option>';"
        "h += '</optgroup>';"
        "h += '</select></div>';"
        "h += '<div style=\"display:flex;align-items:end\"><button type=\"button\" onclick=\"removeEditSubSensor(\\\"' + subSensorId + '\\\")\" style=\"background:#dc3545;color:white;padding:8px 12px;border:none;border-radius:4px;font-size:12px;cursor:pointer\">[DEL] Remove</button></div>';"
        "h += '</div>';"
        "h += '</div>';"
        "subSensorsDiv.innerHTML += h;"
        "}"
        "}"
        "}"
        "function addEditSubSensor(sensorId) {"
        "const subSensorsDiv = document.getElementById('edit-sub-sensors-' + sensorId);"
        "if (!subSensorsDiv) {"
        "alert('ERROR: Sub-sensors container not found');"
        "return;"
        "}"
        "const subSensorCount = subSensorsDiv.querySelectorAll('.sub-sensor').length;"
        "const subSensorId = 'edit-sub-sensor-' + sensorId + '-' + subSensorCount;"
        "let h = '<div class=\"sub-sensor\" id=\"' + subSensorId + '\" style=\"border:1px solid #28a745;padding:10px;margin:10px 0;background:#f8fff8;border-radius:5px\">';"
        "h += '<h5 style=\"color:#28a745;margin-top:0\">[PARAM] Sub-Sensor ' + (subSensorCount + 1) + '</h5>';"
        "h += '<div style=\"display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin:10px 0\">';"
        "h += '<div><label style=\"font-weight:bold\">Parameter Type:</label><br><select name=\"sensor_' + sensorId + '_sub_' + subSensorCount + '_parameter\" style=\"width:100%;padding:5px\">';"
        "h += '<option value=\"\">Select Parameter</option>';"
        "for (let i = 0; i < qualityParameterTypes.length; i++) {"
        "const param = qualityParameterTypes[i];"
        "h += '<option value=\"' + param.key + '\">' + param.name + ' (' + param.units + ')</option>';"
        "}"
        "h += '</select></div>';"
        "h += '<div><label style=\"font-weight:bold\">Slave ID:</label><br><input type=\"number\" name=\"sensor_' + sensorId + '_sub_' + subSensorCount + '_slave_id\" value=\"1\" min=\"1\" max=\"247\" style=\"width:100%;padding:5px\"></div>';"
        "h += '<div><label style=\"font-weight:bold\">Register:</label><br><input type=\"number\" name=\"sensor_' + sensorId + '_sub_' + subSensorCount + '_register\" value=\"1\" min=\"0\" max=\"65535\" style=\"width:100%;padding:5px\"></div>';"
        "h += '</div>';"
        "h += '<div style=\"display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin:10px 0\">';"
        "h += '<div><label style=\"font-weight:bold\">Quantity:</label><br><input type=\"number\" name=\"sensor_' + sensorId + '_sub_' + subSensorCount + '_quantity\" value=\"1\" min=\"1\" max=\"10\" style=\"width:100%;padding:5px\"></div>';"
        "h += '<div><label style=\"font-weight:bold\">Register Type:</label><br><select name=\"sensor_' + sensorId + '_sub_' + subSensorCount + '_register_type\" style=\"width:100%;padding:5px\">';"
        "h += '<option value=\"HOLDING_REGISTER\" selected>Holding Register</option>';"
        "h += '<option value=\"INPUT_REGISTER\">Input Register</option>';"
        "h += '</select></div>';"
        "h += '<div><label style=\"font-weight:bold\">Scale Factor:</label><br><input type=\"number\" name=\"sensor_' + sensorId + '_sub_' + subSensorCount + '_scale_factor\" value=\"1.0\" step=\"any\" style=\"width:100%;padding:5px\"></div>';"
        "h += '</div>';"
        "h += '<div style=\"display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:10px 0\">';"
        "h += '<div><label style=\"font-weight:bold\">Data Type:</label><br><select name=\"sensor_' + sensorId + '_sub_' + subSensorCount + '_data_type\" style=\"width:100%;padding:5px\">';"
        "h += '<option value=\"\">Select Data Type</option>';"
        "h += '<optgroup label=\"8-bit Integer Types\">';"
        "h += '<option value=\"INT8\">INT8 - 8-bit Signed</option>';"
        "h += '<option value=\"UINT8\">UINT8 - 8-bit Unsigned</option>';"
        "h += '</optgroup>';"
        "h += '<optgroup label=\"16-bit Integer Types\">';"
        "h += '<option value=\"INT16\">INT16 - 16-bit Signed</option>';"
        "h += '<option value=\"UINT16\">UINT16 - 16-bit Unsigned</option>';"
        "h += '</optgroup>';"
        "h += '<optgroup label=\"32-bit Integer Types\">';"
        "h += '<option value=\"INT32_ABCD\">INT32_ABCD - Big Endian</option>';"
        "h += '<option value=\"INT32_DCBA\">INT32_DCBA - Little Endian</option>';"
        "h += '<option value=\"INT32_BADC\">INT32_BADC - Mid-Big Endian</option>';"
        "h += '<option value=\"INT32_CDAB\">INT32_CDAB - Mid-Little Endian</option>';"
        "h += '<option value=\"UINT32_ABCD\">UINT32_ABCD - Big Endian</option>';"
        "h += '<option value=\"UINT32_DCBA\">UINT32_DCBA - Little Endian</option>';"
        "h += '<option value=\"UINT32_BADC\">UINT32_BADC - Mid-Big Endian</option>';"
        "h += '<option value=\"UINT32_CDAB\">UINT32_CDAB - Mid-Little Endian</option>';"
        "h += '</optgroup>';"
        "h += '<optgroup label=\"32-bit Float Types\">';"
        "h += '<option value=\"FLOAT32_ABCD\">FLOAT32_ABCD - Big Endian</option>';"
        "h += '<option value=\"FLOAT32_DCBA\">FLOAT32_DCBA - Little Endian</option>';"
        "h += '<option value=\"FLOAT32_BADC\">FLOAT32_BADC - Mid-Big Endian</option>';"
        "h += '<option value=\"FLOAT32_CDAB\">FLOAT32_CDAB - Mid-Little Endian</option>';"
        "h += '</optgroup>';"
        "h += '<optgroup label=\"64-bit Integer Types\">';"
        "h += '<option value=\"INT64_12345678\">INT64_12345678 - Big Endian</option>';"
        "h += '<option value=\"INT64_87654321\">INT64_87654321 - Little Endian</option>';"
        "h += '<option value=\"INT64_21436587\">INT64_21436587 - Mid-Big Endian</option>';"
        "h += '<option value=\"INT64_78563412\">INT64_78563412 - Mid-Little Endian</option>';"
        "h += '<option value=\"UINT64_12345678\">UINT64_12345678 - Big Endian</option>';"
        "h += '<option value=\"UINT64_87654321\">UINT64_87654321 - Little Endian</option>';"
        "h += '<option value=\"UINT64_21436587\">UINT64_21436587 - Mid-Big Endian</option>';"
        "h += '<option value=\"UINT64_78563412\">UINT64_78563412 - Mid-Little Endian</option>';"
        "h += '</optgroup>';"
        "h += '<optgroup label=\"64-bit Float Types\">';"
        "h += '<option value=\"FLOAT64_12345678\">FLOAT64_12345678 - Big Endian</option>';"
        "h += '<option value=\"FLOAT64_87654321\">FLOAT64_87654321 - Little Endian</option>';"
        "h += '<option value=\"FLOAT64_21436587\">FLOAT64_21436587 - Mid-Big Endian</option>';"
        "h += '<option value=\"FLOAT64_78563412\">FLOAT64_78563412 - Mid-Little Endian</option>';"
        "h += '</optgroup>';"
        "h += '<optgroup label=\"Special Types\">';"
        "h += '<option value=\"ASCII\">ASCII String</option>';"
        "h += '<option value=\"HEX\">Hexadecimal</option>';"
        "h += '<option value=\"BOOL\">Boolean</option>';"
        "h += '<option value=\"PDU\">PDU (Protocol Data Unit)</option>';"
        "h += '</optgroup>';"
        "h += '</select></div>';"
        "h += '<div style=\"display:flex;align-items:end;gap:5px\">';"
        "h += '<button type=\"button\" onclick=\"saveEditSubSensor(\\\"' + subSensorId + '\\\")\" style=\"background:#28a745;color:white;padding:8px 12px;border:none;border-radius:4px;font-size:12px;cursor:pointer\">[SAVE] Save</button>';"
        "h += '<button type=\"button\" onclick=\"removeEditSubSensor(\\\"' + subSensorId + '\\\")\" style=\"background:#dc3545;color:white;padding:8px 12px;border:none;border-radius:4px;font-size:12px;cursor:pointer\">[DEL] Remove</button>';"
        "h += '</div>';"
        "h += '</div>';"
        "h += '</div>';"
        "subSensorsDiv.innerHTML += h;"
        "}"
        ""
        "function removeEditSubSensor(subSensorId) {"
        "const subSensorDiv = document.getElementById(subSensorId);"
        "if (subSensorDiv) {"
        "subSensorDiv.remove();"
        "}"
        "}");
    
    // Save and cancel functions
    httpd_resp_sendstr_chunk(req,
        "function saveSensorEdit(sensorId){"
        "const name=document.getElementById('edit_name_'+sensorId).value;"
        "const unitId=document.getElementById('edit_unit_'+sensorId).value;"
        "const slaveId=document.getElementById('edit_slave_'+sensorId).value;"
        "const register=document.getElementById('edit_register_'+sensorId).value;"
        "const quantity=document.getElementById('edit_quantity_'+sensorId).value;"
        "const dataType=document.getElementById('edit_datatype_'+sensorId).value;"
        "const baudRate=document.getElementById('edit_baud_'+sensorId).value;"
        "const parity=document.getElementById('edit_parity_'+sensorId).value;"
        "const registerType=document.getElementById('edit_register_type_'+sensorId).value;"
        "const sensor=sensorData[sensorId];"
        "let formData='sensor_id='+sensorId+'&name='+encodeURIComponent(name)+'&unit_id='+encodeURIComponent(unitId)+"
        "'&slave_id='+slaveId+'&register_address='+register+'&quantity='+quantity+'&data_type='+encodeURIComponent(dataType)+'&register_type='+registerType+'&baud_rate='+baudRate+'&parity='+parity;"
        "if (sensor.sensor_type === 'Level') {"
        "const sensorHeight=document.getElementById('edit_sensor_height_'+sensorId).value||'0';"
        "const maxWaterLevel=document.getElementById('edit_max_water_level_'+sensorId).value||'0';"
        "formData+='&sensor_type=Level&sensor_height='+sensorHeight+'&max_water_level='+maxWaterLevel+'&scale_factor=1.0&meter_type=';"
        "console.log('Saving Level sensor:',sensorId,'Height:',sensorHeight,'MaxLevel:',maxWaterLevel);"
        "} else if (sensor.sensor_type === 'Radar Level') {"
        "const maxWaterLevel=document.getElementById('edit_max_water_level_'+sensorId).value||'0';"
        "formData+='&sensor_type=Radar Level&sensor_height=0&max_water_level='+maxWaterLevel+'&scale_factor=1.0&meter_type=';"
        "console.log('Saving Radar Level sensor:',sensorId,'MaxLevel:',maxWaterLevel);"
        "} else if (sensor.sensor_type === 'ENERGY') {"
        "const meterType=document.getElementById('edit_meter_type_'+sensorId).value||'';"
        "formData+='&sensor_type=ENERGY&meter_type='+encodeURIComponent(meterType)+'&scale_factor=1.0&sensor_height=0&max_water_level=0';"
        "console.log('Saving ENERGY sensor:',sensorId,'MeterType:',meterType);"
        "} else if (sensor.sensor_type === 'QUALITY') {"
        "formData+='&sensor_type=QUALITY&scale_factor=1.0&sensor_height=0&max_water_level=0&meter_type=';"
        "const subSensors = document.querySelectorAll('#edit-sub-sensors-' + sensorId + ' .sub-sensor');"
        "for (let i = 0; i < subSensors.length; i++) {"
        "const subSensor = subSensors[i];"
        "const parameterSelect = subSensor.querySelector('select[name*=\"_parameter\"]');"
        "const slaveIdInput = subSensor.querySelector('input[name*=\"_slave_id\"]');"
        "const registerInput = subSensor.querySelector('input[name*=\"_register\"]');"
        "const quantityInput = subSensor.querySelector('input[name*=\"_quantity\"]');"
        "const registerTypeSelect = subSensor.querySelector('select[name*=\"_register_type\"]');"
        "const scaleFactorInput = subSensor.querySelector('input[name*=\"_scale_factor\"]');"
        "const dataTypeSelect = subSensor.querySelector('select[name*=\"_data_type\"]');"
        "if (parameterSelect && parameterSelect.value && slaveIdInput && registerInput) {"
        "formData += '&sensor_' + sensorId + '_sub_' + i + '_parameter=' + encodeURIComponent(parameterSelect.value);"
        "formData += '&sensor_' + sensorId + '_sub_' + i + '_slave_id=' + (slaveIdInput.value || '1');"
        "formData += '&sensor_' + sensorId + '_sub_' + i + '_register=' + (registerInput.value || '30001');"
        "formData += '&sensor_' + sensorId + '_sub_' + i + '_quantity=' + (quantityInput ? quantityInput.value || '1' : '1');"
        "formData += '&sensor_' + sensorId + '_sub_' + i + '_register_type=' + (registerTypeSelect ? registerTypeSelect.value || 'HOLDING_REGISTER' : 'HOLDING_REGISTER');"
        "formData += '&sensor_' + sensorId + '_sub_' + i + '_scale_factor=' + (scaleFactorInput ? scaleFactorInput.value || '1.0' : '1.0');"
        "formData += '&sensor_' + sensorId + '_sub_' + i + '_data_type=' + (dataTypeSelect ? dataTypeSelect.value : '');"
        "}"
        "}"
        "console.log('Saving QUALITY sensor:',sensorId,'Sub-sensors count:',subSensors.length);"
        "} else if (sensor.sensor_type === 'RAINGAUGE') {"
        "const scaleFactor=document.getElementById('edit_scale_factor_'+sensorId).value||'1.0';"
        "formData+='&sensor_type=RAINGAUGE&scale_factor='+scaleFactor+'&sensor_height=0&max_water_level=0&meter_type=';"
        "console.log('Saving RAINGAUGE sensor:',sensorId,'ScaleFactor:',scaleFactor);"
        "} else if (sensor.sensor_type === 'BOREWELL') {"
        "const scaleFactor=document.getElementById('edit_scale_factor_'+sensorId).value||'1.0';"
        "formData+='&sensor_type=BOREWELL&scale_factor='+scaleFactor+'&sensor_height=0&max_water_level=0&meter_type=';"
        "console.log('Saving BOREWELL sensor:',sensorId,'ScaleFactor:',scaleFactor);"
        "} else {"
        "console.log('Edit save debug: Unknown sensor_type=', sensor.sensor_type, 'preserving original type for sensorId=', sensorId);"
        "const scaleFactor=document.getElementById('edit_scale_factor_'+sensorId).value||'1.0';"
        "const sensorTypeToSave = sensor.sensor_type || 'Flow-Meter';"
        "formData+='&sensor_type='+encodeURIComponent(sensorTypeToSave)+'&scale_factor='+scaleFactor+'&sensor_height=0&max_water_level=0&meter_type=';"
        "console.log('Saving sensor:',sensorId,'SensorType:',sensorTypeToSave,'ScaleFactor:',scaleFactor);"
        "}"
        "if(!name||!unitId){alert('ERROR: Please fill in Name and Unit ID');return;}"
        "fetch('/edit_sensor',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData})"
        ".then(r=>r.json()).then(data=>{"
        "if(data.status==='success'){alert('SUCCESS: Sensor updated successfully');sessionStorage.setItem('showSection', 'sensors');window.location.reload();}else{alert('ERROR: '+data.message);}"
        "}).catch(e=>{alert('ERROR: Update failed: '+e.message);});}"
        "function cancelSensorEdit(sensorId){sessionStorage.setItem('showSection', 'sensors');window.location.reload();}"
        "function deleteSensor(sensorId){"
        "if(confirm('Are you sure you want to delete this sensor?')){"
        "fetch('/delete_sensor',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'sensor_id='+sensorId})"
        ".then(r=>r.json()).then(data=>{"
        "if(data.status==='success'){alert('SUCCESS: Sensor deleted');sessionStorage.setItem('showSection', 'sensors');window.location.reload();}else{alert('ERROR: '+data.message);}"
        "}).catch(e=>{alert('ERROR: Delete failed: '+e.message);});"
        "}}"
        "function testNewSensorRS485(sensorId){"
        "console.log('Testing new sensor RS485 for sensor ID:', sensorId);"
        "const resultDiv=document.getElementById('test-result-new-'+sensorId);"
        "if(!resultDiv){alert('Error: Result div not found for ID: test-result-new-'+sensorId);return;}"
        "resultDiv.style.display='block';"
        "resultDiv.innerHTML='<div style=\"background:#fff3cd;padding:10px;border-radius:4px;color:#856404\">Testing RS485 communication...</div>';"
        "console.log('Looking for form elements with sensor ID:', sensorId);"
        "const slaveIdElem=document.querySelector('input[name=\"sensor_'+sensorId+'_slave_id\"]');"
        "const regAddrElem=document.querySelector('input[name=\"sensor_'+sensorId+'_register_address\"]');"
        "const quantityElem=document.querySelector('input[name=\"sensor_'+sensorId+'_quantity\"]');"
        "let dataTypeElem=document.querySelector('select[name=\"sensor_'+sensorId+'_data_type\"]');"
        "if(!dataTypeElem) dataTypeElem=document.querySelector('input[name=\"sensor_'+sensorId+'_data_type\"]');"
        "const baudRateElem=document.querySelector('select[name=\"sensor_'+sensorId+'_baud_rate\"]');"
        "const parityElem=document.querySelector('select[name=\"sensor_'+sensorId+'_parity\"]');"
        "const scaleFactorElem=document.querySelector('input[name=\"sensor_'+sensorId+'_scale_factor\"]');"
        "const registerTypeElem=document.querySelector('select[name=\"sensor_'+sensorId+'_register_type\"]');"
        "const sensorTypeElem=document.querySelector('select[name=\"sensor_'+sensorId+'_sensor_type\"]');"
        "let sensorType=sensorTypeElem ? sensorTypeElem.value : '';"
        "console.log('Form elements found:', {slaveId: !!slaveIdElem, regAddr: !!regAddrElem, quantity: !!quantityElem, dataType: !!dataTypeElem, baudRate: !!baudRateElem, parity: !!parityElem, registerType: !!registerTypeElem, sensorType: !!sensorTypeElem});"
        "if(!slaveIdElem||!regAddrElem||!quantityElem||(sensorType !== 'ZEST' && sensorType !== 'Clampon' && sensorType !== 'Dailian' && sensorType !== 'Piezometer' && !dataTypeElem)||!baudRateElem||!parityElem){"
        "let missingFields=[];"
        "if(!slaveIdElem) missingFields.push('slave_id');"
        "if(!regAddrElem) missingFields.push('register_address');"
        "if(!quantityElem) missingFields.push('quantity');"
        "if(sensorType !== 'ZEST' && sensorType !== 'Clampon' && sensorType !== 'Dailian' && sensorType !== 'Piezometer' && !dataTypeElem) missingFields.push('data_type');"
        "if(!baudRateElem) missingFields.push('baud_rate');"
        "if(!parityElem) missingFields.push('parity');"
        "resultDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\"><strong>ERROR: Missing Form Elements</strong><br>Cannot find: '+missingFields.join(', ')+'<br>Please make sure all form fields are filled and visible.</div>';"
        "return;}"
        "const slaveId=slaveIdElem.value||'1';"
        "const regAddr=regAddrElem.value||'0';"
        "const quantity=quantityElem.value||'1';"
        "let dataType=dataTypeElem ? dataTypeElem.value : '';"
        "if(sensorType==='Piezometer' && !dataType) dataType='UINT16_HI';"
        "if(sensorType==='Clampon' && !dataType) dataType='UINT32_3412';"
        "if(sensorType==='Dailian' && !dataType) dataType='UINT32_3412';"
        "if(sensorType==='ZEST' && !dataType) dataType='ZEST_FIXED';"
        "const baudRate=baudRateElem.value||'9600';"
        "const parity=parityElem.value||'none';"
        "const registerType=registerTypeElem ? registerTypeElem.value : 'HOLDING';"
        "const scaleFactor=(scaleFactorElem ? scaleFactorElem.value : null)||'1.0';"
        "if(!sensorType){"
        "console.log('Sensor type not found, using Flow-Meter as default');"
        "sensorType='Flow-Meter';"
        "}"
        "console.log('New sensor test params:',{slaveId,regAddr,quantity,dataType,baudRate,parity,scaleFactor,sensorType});"
        "if(sensorType !== 'ZEST' && sensorType !== 'Clampon' && sensorType !== 'Dailian' && sensorType !== 'Piezometer' && !dataType){"
        "resultDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\"><strong>ERROR: Data Type Required</strong><br>Please select a data type before testing.</div>';"
        "return;}"
        "if(!slaveId||!regAddr||!quantity||!baudRate||!parity||!scaleFactor){"
        "resultDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\"><strong>ERROR: Validation Error</strong><br>Please fill in all required fields before testing.</div>';"
        "return;}"
        "let data='slave_id='+slaveId+'&register_address='+regAddr+'&quantity='+quantity+'&data_type='+dataType+'&register_type='+registerType+'&baud_rate='+baudRate+'&parity='+parity+'&scale_factor='+scaleFactor;"
        "data+='&sensor_type='+encodeURIComponent(sensorType);"
        "const sensorHeightElem=document.querySelector('input[name=\"sensor_'+sensorId+'_sensor_height\"]');"
        "const maxWaterLevelElem=document.querySelector('input[name=\"sensor_'+sensorId+'_max_water_level\"]');"
        "if(sensorHeightElem) data+='&sensor_height='+sensorHeightElem.value;"
        "if(maxWaterLevelElem) data+='&max_water_level='+maxWaterLevelElem.value;"
        "console.log('Sending new sensor test data:', data);"
        "fetch('/test_rs485',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:data})"
        ".then(response => {"
        "console.log('New sensor test response status:', response.status);"
        "if (!response.ok) {"
        "throw new Error('HTTP '+response.status+': '+response.statusText);"
        "}"
        "return response.text();"
        "})"
        ".then(htmlData=>{"
        "console.log('New sensor test response HTML received');"
        "resultDiv.innerHTML=htmlData;"
        "}).catch(e=>{"
        "console.error('New sensor test error:', e);"
        "resultDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\"><strong>ERROR: Communication Error</strong><br>'+e.message+'<br>Check browser console (F12) for details.</div>';"
        "});"
        "}"
        "function testEditSensorRS485(sensorId){"
        "console.log('Testing edit sensor RS485 for sensor ID:', sensorId);"
        "const resultDiv=document.getElementById('test-result-edit-'+sensorId);"
        "if(!resultDiv){alert('Error: Result div not found');return;}"
        "resultDiv.style.display='block';"
        "resultDiv.innerHTML='<div style=\"background:#fff3cd;padding:10px;border-radius:4px;color:#856404\">Testing RS485 communication...</div>';"
        "const slaveIdElem=document.getElementById('edit_slave_'+sensorId);"
        "const regAddrElem=document.getElementById('edit_register_'+sensorId);"
        "const quantityElem=document.getElementById('edit_quantity_'+sensorId);"
        "const registerTypeElem=document.getElementById('edit_register_type_'+sensorId);"
        "const dataTypeElem=document.getElementById('edit_datatype_'+sensorId);"
        "const baudRateElem=document.getElementById('edit_baud_'+sensorId);"
        "const parityElem=document.getElementById('edit_parity_'+sensorId);"
        "const scaleFactorElem=document.getElementById('edit_scale_factor_'+sensorId);"
        "if(!slaveIdElem||!regAddrElem||!quantityElem||!registerTypeElem||!dataTypeElem||!baudRateElem||!parityElem){"
        "resultDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\"><strong>ERROR: Form Error</strong><br>Cannot find form elements. Please refresh the page.</div>';"
        "return;}"
        "const slaveId=slaveIdElem.value||'1';"
        "const regAddr=regAddrElem.value||'0';"
        "const quantity=quantityElem.value||'1';"
        "const registerType=registerTypeElem.value||'HOLDING';"
        "const dataType=dataTypeElem.value||'';"
        "const baudRate=baudRateElem.value||'9600';"
        "const parity=parityElem.value||'none';"
        "const sensor=sensorData[sensorId];"
        "const scaleFactor=(scaleFactorElem ? scaleFactorElem.value : null)||'1.0';"
        "const sensorType=sensor ? sensor.sensor_type : 'Flow-Meter';"
        "console.log('Edit test params:',{slaveId,regAddr,quantity,registerType,dataType,baudRate,parity,scaleFactor,sensorType});"
        "if(sensorType !== 'ZEST' && sensorType !== 'Clampon' && sensorType !== 'Dailian' && sensorType !== 'Piezometer' && !dataType){"
        "resultDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\"><strong>ERROR: Data Type Required</strong><br>Please select a data type before testing.</div>';"
        "return;}"
        "if(!slaveId||!regAddr||!quantity||!registerType||!baudRate||!parity||!scaleFactor){"
        "resultDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\"><strong>ERROR: Validation Error</strong><br>Please fill in all required fields before testing.</div>';"
        "return;}"
        "let data='slave_id='+slaveId+'&register_address='+regAddr+'&quantity='+quantity+'&register_type='+registerType+'&data_type='+dataType+'&baud_rate='+baudRate+'&parity='+parity+'&scale_factor='+scaleFactor;"
        "data+='&sensor_type='+encodeURIComponent(sensorType);"
        "const sensorHeightElem=document.getElementById('edit_sensor_height_'+sensorId);"
        "const maxWaterLevelElem=document.getElementById('edit_max_water_level_'+sensorId);"
        "if(sensorHeightElem) data+='&sensor_height='+sensorHeightElem.value;"
        "if(maxWaterLevelElem) data+='&max_water_level='+maxWaterLevelElem.value;"
        "fetch('/test_rs485',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:data})"
        ".then(r=>r.text()).then(htmlData=>{"
        "resultDiv.innerHTML=htmlData;"
        "}).catch(e=>{"
        "resultDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\"><strong>ERROR: Communication Error</strong><br>'+e.message+'</div>';"
        "});"
        "}"
        "function writeSingleRegister(){"
        "const slaveId=document.getElementById('write_single_slave').value;"
        "const address=document.getElementById('write_single_addr').value;"
        "const value=document.getElementById('write_single_value').value;"
        "const resultDiv=document.getElementById('write_single_result');"
        "if(slaveId===''||address===''||value===''){alert('Please fill all fields');return;}"
        "if(slaveId<0||slaveId>247){alert('Slave ID must be between 0-247 (0 for broadcast)');return;}"
        "if(address<0||address>65535){alert('Register address must be between 0-65535');return;}"
        "if(value<0||value>65535){alert('Value must be between 0-65535');return;}"
        "resultDiv.style.display='block';"
        "resultDiv.innerHTML='<div style=\"background:#fff3cd;padding:10px;border-radius:4px;color:#856404\">Writing single register...</div>';"
        "const data='slave_id='+slaveId+'&register_addr='+address+'&value='+value;"
        "fetch('/write_single_register',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:data})"
        ".then(response=>response.json()).then(result=>{"
        "if(result.status==='success'){"
        "resultDiv.innerHTML='<div style=\"background:#d4edda;padding:10px;border-radius:4px;color:#155724\">SUCCESS: Register '+address+' = '+value+' written to slave '+slaveId+'</div>';"
        "}else{"
        "resultDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\">ERROR: '+result.message+'</div>';"
        "}"
        "}).catch(error=>{"
        "resultDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\">NETWORK ERROR: '+error.message+'</div>';"
        "});}"
        "function writeMultipleRegisters(){"
        "const slaveId=document.getElementById('write_multi_slave').value;"
        "const startAddr=document.getElementById('write_multi_start').value;"
        "const valuesText=document.getElementById('write_multi_values').value;"
        "const resultDiv=document.getElementById('write_multi_result');"
        "if(slaveId===''||startAddr===''||!valuesText){alert('Please fill all fields');return;}"
        "if(slaveId<0||slaveId>247){alert('Slave ID must be between 0-247 (0 for broadcast)');return;}"
        "if(startAddr<0||startAddr>65535){alert('Start register must be between 0-65535');return;}"
        "const values=valuesText.split(',').map(v=>parseInt(v.trim())).filter(v=>!isNaN(v));"
        "if(values.length===0){alert('Please enter valid comma-separated values');return;}"
        "if(values.length>125){alert('Maximum 125 registers can be written at once');return;}"
        "for(let v of values){if(v<0||v>65535){alert('All values must be between 0-65535');return;}}"
        "resultDiv.style.display='block';"
        "resultDiv.innerHTML='<div style=\"background:#fff3cd;padding:10px;border-radius:4px;color:#856404\">Writing '+values.length+' registers...</div>';"
        "const data='slave_id='+slaveId+'&start_addr='+startAddr+'&values='+values.join(',');"
        "fetch('/write_multiple_registers',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:data})"
        ".then(response=>response.json()).then(result=>{"
        "if(result.status==='success'){"
        "resultDiv.innerHTML='<div style=\"background:#d4edda;padding:10px;border-radius:4px;color:#155724\">SUCCESS: '+values.length+' registers written starting at '+startAddr+' on slave '+slaveId+'</div>';"
        "}else{"
        "resultDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\">ERROR: '+result.message+'</div>';"
        "}"
        "}).catch(error=>{"
        "resultDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\">NETWORK ERROR: '+error.message+'</div>';"
        "});}"
        "function showSensorSubMenu(menuType) {"
        "const regularSensors = document.getElementById('regular-sensors-list');"
        "const waterQualitySensors = document.getElementById('water-quality-sensors-list');"
        "const modbusExplorer = document.getElementById('modbus-explorer-list');"
        "const btnRegular = document.getElementById('btn-regular-sensors');"
        "const btnWaterQuality = document.getElementById('btn-water-quality-sensors');"
        "const btnModbusExplorer = document.getElementById('btn-modbus-explorer');"
        "if (menuType === 'regular') {"
        "regularSensors.style.display = 'block';"
        "waterQualitySensors.style.display = 'none';"
        "modbusExplorer.style.display = 'none';"
        "btnRegular.style.background = '#007bff';"
        "btnWaterQuality.style.background = '#6c757d';"
        "btnModbusExplorer.style.background = '#6c757d';"
        "} else if (menuType === 'water_quality') {"
        "regularSensors.style.display = 'none';"
        "waterQualitySensors.style.display = 'block';"
        "modbusExplorer.style.display = 'none';"
        "btnRegular.style.background = '#6c757d';"
        "btnWaterQuality.style.background = '#17a2b8';"
        "btnModbusExplorer.style.background = '#6c757d';"
        "} else if (menuType === 'explorer') {"
        "regularSensors.style.display = 'none';"
        "waterQualitySensors.style.display = 'none';"
        "modbusExplorer.style.display = 'block';"
        "btnRegular.style.background = '#6c757d';"
        "btnWaterQuality.style.background = '#6c757d';"
        "btnModbusExplorer.style.background = '#6c757d';"
        "}"
        "}"
        "let autoRefreshInterval=null;"
        "function scanModbusDevices(){"
        "const startId=parseInt(document.getElementById('scan_start').value);"
        "const endId=parseInt(document.getElementById('scan_end').value);"
        "const testRegister=parseInt(document.getElementById('scan_register').value);"
        "const regType=document.getElementById('scan_reg_type').value;"
        "const progressDiv=document.getElementById('scan_progress');"
        "const resultsDiv=document.getElementById('scan_results');"
        "if(startId<1||startId>247||endId<1||endId>247||startId>endId){"
        "alert('Invalid slave ID range (1-247)');return;}"
        "progressDiv.style.display='block';"
        "progressDiv.innerHTML='<div style=\"background:#d1ecf1;padding:10px;border-radius:4px;color:#0c5460\">Scanning devices '+startId+'-'+endId+'...</div>';"
        "resultsDiv.innerHTML='';"
        "const data='start_id='+startId+'&end_id='+endId+'&test_register='+testRegister+'&reg_type='+regType;"
        "fetch('/modbus_scan',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:data})"
        ".then(response=>response.json()).then(result=>{"
        "progressDiv.style.display='none';"
        "if(result.status==='success'){"
        "if(result.devices.length===0){"
        "resultsDiv.innerHTML='<div class=\"sensor-card\"><h3>No Devices Found</h3><p>No responsive Modbus devices found in range '+startId+'-'+endId+'</p></div>';"
        "}else{"
        "let html='<div class=\"sensor-card\"><h3>Discovered Devices ('+result.devices.length+')</h3><table style=\"width:100%;border-collapse:collapse\"><thead><tr><th>Slave ID</th><th>Status</th></tr></thead><tbody>';"
        "result.devices.forEach(dev=>{"
        "html+='<tr><td>'+dev.slave_id+'</td><td><span style=\"color:#28a745;font-weight:bold\">✓ Responsive</span></td></tr>';"
        "});"
        "html+='</tbody></table></div>';"
        "resultsDiv.innerHTML=html;"
        "}"
        "}else{"
        "resultsDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\">ERROR: '+result.message+'</div>';"
        "}"
        "}).catch(error=>{"
        "progressDiv.style.display='none';"
        "resultsDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\">NETWORK ERROR: '+error.message+'</div>';"
        "});}"
        "function readLiveRegisters(){"
        "const slaveId=parseInt(document.getElementById('live_slave').value);"
        "const startRegister=parseInt(document.getElementById('live_register').value);"
        "const quantity=parseInt(document.getElementById('live_quantity').value);"
        "const regType=document.getElementById('live_reg_type').value;"
        "const resultDiv=document.getElementById('live_result');"
        "if(slaveId<1||slaveId>247){alert('Slave ID must be 1-247');return;}"
        "if(quantity<1||quantity>10){alert('Quantity must be 1-10');return;}"
        "const data='slave_id='+slaveId+'&start_register='+startRegister+'&quantity='+quantity+'&reg_type='+regType;"
        "fetch('/modbus_read_live',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:data})"
        ".then(response=>response.json()).then(result=>{"
        "if(result.status==='success'){"
        "const f=result.formats;"
        "let html='<div class=\"sensor-card\"><h3>Register Values</h3>';"
        "html+='<table class=\"scada-table\"><thead><tr class=\"scada-header-main\"><th colspan=\"2\">Raw Data</th></tr></thead><tbody>';"
        "html+='<tr><td><strong>Hex String</strong></td><td>'+f.hex_string+'</td></tr></tbody></table>';"
        "if(f.uint16_be!==undefined){"
        "html+='<table class=\"scada-table\"><thead><tr class=\"scada-header-uint\"><th colspan=\"2\">16-bit Formats</th></tr></thead><tbody>';"
        "html+='<tr><td><strong>UINT16 Big Endian</strong></td><td>'+f.uint16_be+'</td></tr>';"
        "html+='<tr><td><strong>UINT16 Little Endian</strong></td><td>'+f.uint16_le+'</td></tr>';"
        "html+='<tr><td><strong>INT16 Big Endian</strong></td><td>'+f.int16_be+'</td></tr>';"
        "html+='<tr><td><strong>INT16 Little Endian</strong></td><td>'+f.int16_le+'</td></tr></tbody></table>';"
        "}"
        "if(f.uint32_abcd!==undefined){"
        "html+='<table class=\"scada-table\"><thead><tr class=\"scada-header-uint\"><th colspan=\"2\">32-bit UINT Formats</th></tr></thead><tbody>';"
        "html+='<tr><td><strong>UINT32 ABCD</strong></td><td>'+f.uint32_abcd+'</td></tr>';"
        "html+='<tr><td><strong>UINT32 DCBA</strong></td><td>'+f.uint32_dcba+'</td></tr>';"
        "html+='<tr><td><strong>UINT32 BADC</strong></td><td>'+f.uint32_badc+'</td></tr>';"
        "html+='<tr><td><strong>UINT32 CDAB</strong></td><td>'+f.uint32_cdab+'</td></tr></tbody></table>';"
        "html+='<table class=\"scada-table\"><thead><tr class=\"scada-header-int\"><th colspan=\"2\">32-bit INT Formats</th></tr></thead><tbody>';"
        "html+='<tr><td><strong>INT32 ABCD</strong></td><td>'+f.int32_abcd+'</td></tr>';"
        "html+='<tr><td><strong>INT32 DCBA</strong></td><td>'+f.int32_dcba+'</td></tr>';"
        "html+='<tr><td><strong>INT32 BADC</strong></td><td>'+f.int32_badc+'</td></tr>';"
        "html+='<tr><td><strong>INT32 CDAB</strong></td><td>'+f.int32_cdab+'</td></tr></tbody></table>';"
        "html+='<table class=\"scada-table\"><thead><tr class=\"scada-header-float\"><th colspan=\"2\">32-bit FLOAT Formats</th></tr></thead><tbody>';"
        "html+='<tr><td><strong>FLOAT32 ABCD</strong></td><td>'+f.float32_abcd.toFixed(6)+'</td></tr>';"
        "html+='<tr><td><strong>FLOAT32 DCBA</strong></td><td>'+f.float32_dcba.toFixed(6)+'</td></tr>';"
        "html+='<tr><td><strong>FLOAT32 BADC</strong></td><td>'+f.float32_badc.toFixed(6)+'</td></tr>';"
        "html+='<tr><td><strong>FLOAT32 CDAB</strong></td><td>'+f.float32_cdab.toFixed(6)+'</td></tr></tbody></table>';"
        "}"
        "html+='</div>';"
        "resultDiv.innerHTML=html;"
        "}else{"
        "resultDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\">ERROR: '+result.message+'</div>';"
        "}"
        "}).catch(error=>{"
        "resultDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\">NETWORK ERROR: '+error.message+'</div>';"
        "});}"
        "function toggleAutoRefresh(){"
        "const btn=document.getElementById('auto_refresh_btn');"
        "if(autoRefreshInterval){"
        "clearInterval(autoRefreshInterval);"
        "autoRefreshInterval=null;"
        "btn.textContent='Enable Auto-Refresh';"
        "btn.style.background='var(--color-primary)';"
        "}else{"
        "readLiveRegisters();"
        "autoRefreshInterval=setInterval(readLiveRegisters,2000);"
        "btn.textContent='Disable Auto-Refresh';"
        "btn.style.background='var(--color-error)';"
        "}}"
        "let modbusPollInterval=null;"
        "function startModbusPoll(){"
        "const slaveId=parseInt(document.getElementById('poll_slave').value);"
        "const startReg=parseInt(document.getElementById('poll_register').value);"
        "const quantity=parseInt(document.getElementById('poll_quantity').value);"
        "const regType=document.getElementById('poll_reg_type').value;"
        "const interval=parseInt(document.getElementById('poll_interval').value);"
        "const resultDiv=document.getElementById('poll_result');"
        "const startBtn=document.getElementById('start_poll_btn');"
        "const stopBtn=document.getElementById('stop_poll_btn');"
        "if(slaveId<1||slaveId>247||startReg<0||startReg>65535||quantity<1||quantity>20){"
        "alert('Invalid parameters');return;}"
        "startBtn.style.display='none';"
        "stopBtn.style.display='block';"
        "resultDiv.style.display='block';"
        "resultDiv.innerHTML='<div style=\"background:#d1ecf1;padding:10px;border-radius:4px;color:#0c5460;margin-bottom:10px\">● Polling Slave '+slaveId+', Reg '+startReg+'-'+(startReg+quantity-1)+' ('+regType+') every '+(interval/1000)+'s</div>';"
        "const pollFunc=()=>{"
        "fetch('/api/modbus_poll?slave='+slaveId+'&reg='+startReg+'&qty='+quantity+'&type='+regType)"
        ".then(r=>r.json())"
        ".then(data=>{"
        "if(data.status==='success'){"
        "let html='<table style=\"width:100%;border-collapse:collapse;background:white;border:1px solid #dee2e6\">';"
        "html+='<thead><tr style=\"background:#28a745;color:white\"><th style=\"padding:8px;text-align:left\">Register</th><th style=\"padding:8px;text-align:right\">Value (Dec)</th><th style=\"padding:8px;text-align:right\">Value (Hex)</th></tr></thead><tbody>';"
        "for(let i=0;i<data.values.length;i++){"
        "html+='<tr style=\"border-bottom:1px solid #dee2e6\"><td style=\"padding:6px;font-weight:bold\">'+(startReg+i)+'</td><td style=\"padding:6px;text-align:right;font-family:monospace\">'+data.values[i]+'</td><td style=\"padding:6px;text-align:right;font-family:monospace;color:#007bff\">0x'+data.values[i].toString(16).toUpperCase().padStart(4,'0')+'</td></tr>';"
        "}"
        "html+='</tbody></table>';"
        "html+='<div style=\"margin-top:8px;font-size:12px;color:#6c757d\">Last update: '+new Date().toLocaleTimeString()+'</div>';"
        "resultDiv.innerHTML='<div style=\"background:#d1ecf1;padding:10px;border-radius:4px;color:#0c5460;margin-bottom:10px\">● Polling Active - Slave '+slaveId+', Reg '+startReg+'-'+(startReg+quantity-1)+'</div>'+html;"
        "}else{"
        "resultDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\">ERROR: '+data.message+'</div>';"
        "}"
        "}).catch(err=>{"
        "resultDiv.innerHTML='<div style=\"background:#f8d7da;padding:10px;border-radius:4px;color:#721c24\">NETWORK ERROR: '+err.message+'</div>';"
        "});"
        "};"
        "pollFunc();"
        "modbusPollInterval=setInterval(pollFunc,interval);"
        "}"
        "function stopModbusPoll(){"
        "if(modbusPollInterval){"
        "clearInterval(modbusPollInterval);"
        "modbusPollInterval=null;}"
        "document.getElementById('start_poll_btn').style.display='block';"
        "document.getElementById('stop_poll_btn').style.display='none';"
        "document.getElementById('poll_result').innerHTML='<div style=\"background:#fff3cd;padding:10px;border-radius:4px;color:#856404\">Polling stopped</div>';"
        "}"
        "console.log('Script loaded successfully. addSensor function defined:', typeof addSensor);"
        "</script>");
    

    // Write Operations section
    httpd_resp_sendstr_chunk(req,
        "<div id='write_ops' class='section'>"
        "<h2 class='section-title'><i>✏️</i>Write Operations</h2>"
        ""
        "<div class='sensor-card'>"
        "<h3>Write Single Register (Function Code 06)</h3>"
        "<p>Write a single value to a Modbus holding register for device control and configuration.</p>"
    );
    
    // Write single register form
    httpd_resp_sendstr_chunk(req,
        "<div class='form-grid'>"
        "<label>Slave ID:</label>"
        "<input type='number' id='write_single_slave' min='0' max='247' value='1'>"
        "<label>Register Address:</label>"
        "<input type='number' id='write_single_addr' min='0' max='65535' value='0'>"
        "<label>Value (decimal):</label>"
        "<input type='number' id='write_single_value' min='0' max='65535' value='0'>"
        "</div>"
        "<button onclick='writeSingleRegister()' class='btn' style='background:var(--color-primary);color:white;width:auto;min-width:200px'>Write Single Register</button>"
        "<div id='write_single_result' style='margin-top:var(--space-md);padding:var(--space-md);background:var(--color-bg-secondary);border-radius:var(--radius-md);display:none'></div>"
        "</div>"
    );
    
    // Write multiple registers section
    httpd_resp_sendstr_chunk(req,
        ""
        "<div class='sensor-card'>"
        "<h3>Write Multiple Registers (Function Code 16)</h3>"
        "<p>Write multiple consecutive values to Modbus holding registers for bulk configuration.</p>"
        "<div class='form-grid'>"
        "<label>Slave ID:</label>"
        "<input type='number' id='write_multi_slave' min='0' max='247' value='1'>"
        "<label>Start Register:</label>"
        "<input type='number' id='write_multi_start' min='0' max='65535' value='0'>"
        "<label>Values (comma-separated):</label>"
        "<textarea id='write_multi_values' placeholder='Example: 1000,2000,3000' rows='3'></textarea>"
        "</div>"
        "<button onclick='writeMultipleRegisters()' class='btn' style='background:var(--color-success);color:white;width:auto;min-width:200px'>Write Multiple Registers</button>"
        "<div id='write_multi_result' style='margin-top:var(--space-md);padding:var(--space-md);background:var(--color-bg-secondary);border-radius:var(--radius-md);display:none'></div>"
        "</div>"
    );
    
    // Write operations notes and monitoring section
    httpd_resp_sendstr_chunk(req,
        ""
        "<div class='sensor-card'>"
        "<h3>Write Operation Notes</h3>"
        "<ul style='margin:10px 0;padding-left:20px'>"
        "<li><strong>Function Code 06:</strong> Write Single Register - For individual register writes</li>"
        "<li><strong>Function Code 16:</strong> Write Multiple Registers - For bulk register writes (more efficient)</li>"
        "<li><strong>Slave ID 0:</strong> Broadcast mode - sends command to all devices (no response expected)</li>"
        "<li><strong>Holding Registers:</strong> Read/Write registers used for device configuration and control</li>"
        "<li><strong>Values:</strong> All values are 16-bit unsigned integers (0-65535)</li>"
        "<li><strong>Industrial Safety:</strong> Verify register addresses and values before writing to avoid equipment damage</li>"
        "</ul>"
        "</div>"
        "</div>"
    );

    // Modbus Explorer section
    snprintf(chunk, sizeof(chunk),
        "<div id='monitoring' class='section'>"
        "<h2 class='section-title'><i>🖥️</i>System Monitor</h2>"
        ""
        "<div class='sensor-card'>"
        "<h3>RS485 Configuration</h3>"
        "<p><strong>RX Pin:</strong> <span>GPIO16</span></p>"
        "<p><strong>TX Pin:</strong> <span>GPIO17</span></p>"
        "<p><strong>RTS Pin:</strong> <span>GPIO4</span></p>"
        "<p><strong>Baud Rate:</strong> <span>9600</span></p>"
        "<p><strong>Parity:</strong> <span>None</span></p>"
        "</div>"
        ""
        "<div class='sensor-card'>"
        "<h3>System Status</h3>"
        "<p><strong>Firmware:</strong> <span>v1.1.0-final</span></p>"
        "<p><strong>MAC Address:</strong> <span id='mac_address'>Loading...</span></p>"
        "<p><strong>Uptime:</strong> <span id='uptime'>Loading...</span></p>"
        "<p><strong>Flash Memory:</strong> <span id='flash_total'>Loading...</span></p>"
        "<p><strong>Active Tasks:</strong> <span id='tasks'>Loading...</span></p>"
        "</div>"
        ""
        "<div class='sensor-card'>"
        "<h3>Memory Usage</h3>"
        "<p><strong>Heap Usage:</strong> <span id='heap_usage'>Loading...</span></p>"
        "<p><strong>Free Heap:</strong> <span id='heap'>Loading...</span></p>"
        "<p><strong>Internal RAM:</strong> <span id='internal_heap'>Loading...</span></p>"
        "<p><strong>SPIRAM:</strong> <span id='spiram_heap'>Loading...</span></p>"
        "<p><strong>Largest Block:</strong> <span id='largest_block'>Loading...</span></p>"
        "</div>"
        ""
        "<div class='sensor-card' id='wifi-network-status' style='display:%s'>"
        "<h3>WiFi Network Status</h3>"
        "<p><strong>WiFi Status:</strong> <span id='wifi_status'>Loading...</span></p>"
        "<p><strong>WiFi RSSI:</strong> <span id='rssi'>Loading...</span></p>"
        "<p><strong>SSID:</strong> <span id='ssid'>Loading...</span></p>"
        "</div>"
        ""
        "<div class='sensor-card' id='sim-network-status' style='display:%s'>"
        "<h3>SIM Network Status</h3>"
        "<p><strong>SIM Status:</strong> <span id='sim_status'>Loading...</span></p>"
        "<p><strong>Signal Quality:</strong> <span id='sim_signal'>Loading...</span></p>"
        "<p><strong>Network:</strong> <span id='sim_network'>Loading...</span></p>"
        "<p><strong>IP Address:</strong> <span id='sim_ip'>Loading...</span></p>"
        "</div>"
        ""
        "<div class='sensor-card'>"
        "<h3>Modbus Communication</h3>"
        "<p><strong>Total Reads:</strong> <span id='modbus_total_reads'>Loading...</span></p>"
        "<p><strong>Successful:</strong> <span id='modbus_success'>Loading...</span></p>"
        "<p><strong>Failed:</strong> <span id='modbus_failed'>Loading...</span></p>"
        "<p><strong>Success Rate:</strong> <span id='modbus_success_rate'>Loading...</span></p>"
        "<p><strong>CRC Errors:</strong> <span id='modbus_crc_errors'>Loading...</span></p>"
        "<p><strong>Timeout Errors:</strong> <span id='modbus_timeout_errors'>Loading...</span></p>"
        "</div>"
        ""
        "<div class='sensor-card'>"
        "<h3>Azure IoT Hub</h3>"
        "<p><strong>Connection:</strong> <span id='azure_connection'>Loading...</span></p>"
        "<p><strong>Uptime:</strong> <span id='azure_uptime'>Loading...</span></p>"
        "<p><strong>Messages Sent:</strong> <span id='azure_messages'>Loading...</span></p>"
        "<p><strong>Last Telemetry:</strong> <span id='azure_last_telemetry'>Loading...</span></p>"
        "<p><strong>Reconnects:</strong> <span id='azure_reconnects'>Loading...</span></p>"
        "<p><strong>Device ID:</strong> <span id='azure_device_id'>Loading...</span></p>"
        "</div>"
        "</div>",
        g_system_config.network_mode == 0 ? "block" : "none",
        g_system_config.network_mode == 1 ? "block" : "none");
    httpd_resp_sendstr_chunk(req, chunk);
    
    // Send HTML footer and end chunked transfer
    httpd_resp_sendstr_chunk(req, html_footer);
    httpd_resp_sendstr_chunk(req, NULL);
    
    return ESP_OK;
}

// URL decode helper function
static void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if (*src == '%' && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

// Parse form parameter
static void parse_form_param(const char *param, const char *value) {
    char decoded_value[256];
    url_decode(decoded_value, value);
    
    ESP_LOGI(TAG, "Parsing: %s = %s", param, decoded_value);
    
    if (strcmp(param, "wifi_ssid") == 0) {
        strncpy(g_system_config.wifi_ssid, decoded_value, sizeof(g_system_config.wifi_ssid) - 1);
    } else if (strcmp(param, "wifi_password") == 0) {
        strncpy(g_system_config.wifi_password, decoded_value, sizeof(g_system_config.wifi_password) - 1);
    } else if (strcmp(param, "telemetry_interval") == 0) {
        g_system_config.telemetry_interval = atoi(decoded_value);
    } else if (strncmp(param, "sensor_", 7) == 0) {
        // Parse sensor parameters
        int sensor_idx = atoi(param + 7);
        if (sensor_idx >= 0 && sensor_idx < 8) {
            char *param_type = strchr(param + 7, '_');
            if (param_type) {
                param_type++; // Skip the underscore
                
                if (strcmp(param_type, "name") == 0) {
                    strncpy(g_system_config.sensors[sensor_idx].name, decoded_value, sizeof(g_system_config.sensors[sensor_idx].name) - 1);
                    g_system_config.sensors[sensor_idx].enabled = true;
                    // Initialize default data_type if not already set
                    if (strlen(g_system_config.sensors[sensor_idx].data_type) == 0) {
                        strcpy(g_system_config.sensors[sensor_idx].data_type, "UINT16_HI");
                        ESP_LOGI(TAG, "Set default data_type to UINT16_HI for new sensor %d", sensor_idx);
                    }
                    // Update sensor count if this is a new sensor
                    if (sensor_idx >= g_system_config.sensor_count) {
                        g_system_config.sensor_count = sensor_idx + 1;
                    }
                } else if (strcmp(param_type, "unit_id") == 0) {
                    strncpy(g_system_config.sensors[sensor_idx].unit_id, decoded_value, sizeof(g_system_config.sensors[sensor_idx].unit_id) - 1);
                } else if (strcmp(param_type, "slave_id") == 0) {
                    g_system_config.sensors[sensor_idx].slave_id = atoi(decoded_value);
                } else if (strcmp(param_type, "register_address") == 0) {
                    g_system_config.sensors[sensor_idx].register_address = atoi(decoded_value);
                } else if (strcmp(param_type, "quantity") == 0) {
                    g_system_config.sensors[sensor_idx].quantity = atoi(decoded_value);
                } else if (strcmp(param_type, "data_type") == 0) {
                    strncpy(g_system_config.sensors[sensor_idx].data_type, decoded_value, sizeof(g_system_config.sensors[sensor_idx].data_type) - 1);
                } else if (strcmp(param_type, "baud_rate") == 0) {
                    g_system_config.sensors[sensor_idx].baud_rate = atoi(decoded_value);
                }
            }
        }
    }
}

// Save Azure configuration handler
static esp_err_t save_azure_config_handler(httpd_req_t *req)
{
    char buf[2048];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    ESP_LOGI(TAG, "Azure config data: %s", buf);
    
    // Parse form data
    char *param = strtok(buf, "&");
    while (param != NULL) {
        if (strncmp(param, "azure_device_id=", 16) == 0) {
            char decoded_value[256];
            url_decode(decoded_value, param + 16);
            strncpy(g_system_config.azure_device_id, decoded_value, sizeof(g_system_config.azure_device_id) - 1);
            g_system_config.azure_device_id[sizeof(g_system_config.azure_device_id) - 1] = '\0';
            ESP_LOGI(TAG, "Azure device ID updated: %s", g_system_config.azure_device_id);
        } else if (strncmp(param, "azure_device_key=", 17) == 0) {
            char decoded_value[256];
            url_decode(decoded_value, param + 17);
            strncpy(g_system_config.azure_device_key, decoded_value, sizeof(g_system_config.azure_device_key) - 1);
            g_system_config.azure_device_key[sizeof(g_system_config.azure_device_key) - 1] = '\0';
            ESP_LOGI(TAG, "Azure device key updated (len=%d, first4=%.4s, last4=%.4s)",
                     (int)strlen(g_system_config.azure_device_key),
                     g_system_config.azure_device_key,
                     g_system_config.azure_device_key + strlen(g_system_config.azure_device_key) - 4);
        } else if (strncmp(param, "telemetry_interval=", 19) == 0) {
            g_system_config.telemetry_interval = atoi(param + 19);
            ESP_LOGI(TAG, "Telemetry interval: %d", g_system_config.telemetry_interval);
        }
        param = strtok(NULL, "&");
    }
    
    // Save to NVS
    esp_err_t save_result = config_save_to_nvs(&g_system_config);
    if (save_result == ESP_OK) {
        ESP_LOGI(TAG, "Azure configuration saved successfully");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"success\",\"message\":\"Azure configuration saved successfully\"}");
    } else {
        ESP_LOGE(TAG, "Failed to save Azure configuration: %s", esp_err_to_name(save_result));
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Failed to save Azure configuration\"}");
    }
    
    return ESP_OK;
}

// Save modem configuration handler
static esp_err_t save_modem_config_handler(httpd_req_t *req)
{
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    ESP_LOGI(TAG, "Modem config data: %s", buf);
    
    // Reset modem_reset_enabled flag first
    g_system_config.modem_reset_enabled = false;
    
    // Parse form data
    char *param = strtok(buf, "&");
    while (param != NULL) {
        if (strncmp(param, "modem_reset_enabled=", 20) == 0) {
            g_system_config.modem_reset_enabled = (strcmp(param + 20, "1") == 0);
            ESP_LOGI(TAG, "Modem reset enabled: %s", g_system_config.modem_reset_enabled ? "true" : "false");
        } else if (strncmp(param, "modem_reset_gpio_pin=", 21) == 0) {
            int gpio_pin = atoi(param + 21);
            if (gpio_pin >= 2 && gpio_pin <= 39 && 
                gpio_pin != 6 && gpio_pin != 7 && gpio_pin != 8 && gpio_pin != 9 && 
                gpio_pin != 10 && gpio_pin != 11) {
                g_system_config.modem_reset_gpio_pin = gpio_pin;
                ESP_LOGI(TAG, "Modem reset GPIO pin: %d", g_system_config.modem_reset_gpio_pin);
            } else {
                ESP_LOGW(TAG, "Invalid modem reset GPIO pin %d, using default GPIO 2", gpio_pin);
                g_system_config.modem_reset_gpio_pin = 2;
            }
        } else if (strncmp(param, "modem_boot_delay=", 17) == 0) {
            int delay = atoi(param + 17);
            if (delay >= 5 && delay <= 60) {
                g_system_config.modem_boot_delay = delay;
                ESP_LOGI(TAG, "Modem boot delay: %d seconds", g_system_config.modem_boot_delay);
            } else {
                ESP_LOGW(TAG, "Invalid modem boot delay %d, using default 15 seconds", delay);
                g_system_config.modem_boot_delay = 15;
            }
        }
        param = strtok(NULL, "&");
    }
    
    // Save to NVS
    esp_err_t save_result = config_save_to_nvs(&g_system_config);
    if (save_result == ESP_OK) {
        ESP_LOGI(TAG, "Modem configuration saved successfully");
        
        // Update GPIO pin if it changed (only in operation mode)
        if (get_config_state() == CONFIG_STATE_OPERATION) {
            esp_err_t gpio_result = update_modem_gpio_pin(g_system_config.modem_reset_gpio_pin);
            if (gpio_result != ESP_OK) {
                ESP_LOGW(TAG, "Failed to update modem GPIO pin, will apply on next restart");
            }
        }
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"success\",\"message\":\"Modem configuration saved successfully\"}");
    } else {
        ESP_LOGE(TAG, "Failed to save modem configuration: %s", esp_err_to_name(save_result));
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Failed to save modem configuration\"}");
    }
    
    return ESP_OK;
}

// Save system configuration handler
static esp_err_t save_system_config_handler(httpd_req_t *req)
{
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    ESP_LOGI(TAG, "System config data: %s", buf);
    
    // Initialize defaults
    int trigger_gpio_pin = 34;
    
    // Parse form data
    char *param = strtok(buf, "&");
    while (param != NULL) {
        if (strncmp(param, "trigger_gpio_pin=", 17) == 0) {
            trigger_gpio_pin = atoi(param + 17);
            ESP_LOGI(TAG, "Trigger GPIO pin set to: %d", trigger_gpio_pin);
        }
        param = strtok(NULL, "&");
    }
    
    // Validate GPIO pin
    if (trigger_gpio_pin < 0 || trigger_gpio_pin > 39) {
        ESP_LOGW(TAG, "Invalid trigger GPIO pin: %d", trigger_gpio_pin);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Invalid GPIO pin. Use 0-39\"}");
        return ESP_OK;
    }
    
    // Update system config
    system_config_t *config = get_system_config();
    config->trigger_gpio_pin = trigger_gpio_pin;
    
    // Save to NVS
    esp_err_t save_result = config_save_to_nvs(config);
    
    ESP_LOGI(TAG, "Configuration trigger GPIO pin: %d", trigger_gpio_pin);
    
    if (save_result == ESP_OK) {
        ESP_LOGI(TAG, "Trigger GPIO configuration saved successfully");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"success\",\"message\":\"Configuration trigger saved successfully\"}");
    } else {
        ESP_LOGE(TAG, "Failed to save trigger GPIO configuration: %s", esp_err_to_name(save_result));
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Failed to save configuration\"}");
    }
    
    return ESP_OK;
}

// Save configuration handler
static esp_err_t save_config_handler(httpd_req_t *req)
{
    char buf[4096];
    bool attempt_wifi_connection = false;  // Flag to attempt WiFi connection after HTTP response
    
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    ESP_LOGI(TAG, "Received configuration data: %s", buf);
    
    // Parse form data
    char *param_start = buf;
    while (param_start) {
        char *param_end = strchr(param_start, '&');
        if (param_end) {
            *param_end = '\0';
        }
        
        char *equals = strchr(param_start, '=');
        if (equals) {
            *equals = '\0';
            char *param_name = param_start;
            char *param_value = equals + 1;
            
            parse_form_param(param_name, param_value);
        }
        
        if (param_end) {
            param_start = param_end + 1;
        } else {
            break;
        }
    }
    
    // Save configuration to NVS
    esp_err_t err = config_save_to_nvs(&g_system_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save configuration: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Configuration saved successfully");
        ESP_LOGI(TAG, "WiFi SSID: %s", g_system_config.wifi_ssid);
        ESP_LOGI(TAG, "Telemetry interval: %d", g_system_config.telemetry_interval);
        ESP_LOGI(TAG, "Sensor count: %d", g_system_config.sensor_count);
    }
    
    // Mark WiFi connection to be attempted after HTTP response is sent (only if save was successful)
    attempt_wifi_connection = (err == ESP_OK && 
                              strlen(g_system_config.wifi_ssid) > 0 && 
                              strlen(g_system_config.wifi_password) > 0);
    
    // Send success response using chunked response
    httpd_resp_set_type(req, "text/html; charset=UTF-8");
    
    httpd_resp_sendstr_chunk(req, html_header);
    httpd_resp_sendstr_chunk(req, "<h1>SUCCESS: Configuration Saved</h1>");
    
    char temp_buf[256];
    snprintf(temp_buf, sizeof(temp_buf), "<p>WiFi SSID: %s</p>", g_system_config.wifi_ssid);
    httpd_resp_sendstr_chunk(req, temp_buf);
    
    snprintf(temp_buf, sizeof(temp_buf), "<p>Telemetry Interval: %d seconds</p>", g_system_config.telemetry_interval);
    httpd_resp_sendstr_chunk(req, temp_buf);
    
    snprintf(temp_buf, sizeof(temp_buf), "<p>Sensors configured: %d</p>", g_system_config.sensor_count);
    httpd_resp_sendstr_chunk(req, temp_buf);
    
    // Add enhanced WiFi connection status message
    if (strlen(g_system_config.wifi_ssid) > 0) {
        httpd_resp_sendstr_chunk(req, "<div style='background:#e1f5fe;border:1px solid #0288d1;border-radius:4px;padding:15px;margin:15px 0'>");
        httpd_resp_sendstr_chunk(req, "<p style='margin:0;color:#0277bd;font-size:16px'><strong>📶 WiFi Connection Status</strong></p>");
        
        // Check current WiFi connection status
        wifi_ap_record_t ap_info;
        esp_err_t conn_status = esp_wifi_sta_get_ap_info(&ap_info);
        if (conn_status == ESP_OK) {
            // Successfully connected
            httpd_resp_sendstr_chunk(req, "<p style='margin:8px 0;color:#2e7d32;font-weight:bold'>✅ Connected Successfully!</p>");
            snprintf(temp_buf, sizeof(temp_buf), "<p style='margin:5px 0;color:#0277bd'>* Network: %s</p>", ap_info.ssid);
            httpd_resp_sendstr_chunk(req, temp_buf);
            snprintf(temp_buf, sizeof(temp_buf), "<p style='margin:5px 0;color:#0277bd'>* Signal: %d dBm</p>", ap_info.rssi);
            httpd_resp_sendstr_chunk(req, temp_buf);
            httpd_resp_sendstr_chunk(req, "<p style='margin:8px 0;color:#0277bd;font-size:14px'>You can now access this interface via your main WiFi network.</p>");
        } else {
            // Connection in progress or failed
            httpd_resp_sendstr_chunk(req, "<p style='margin:8px 0;color:#f57c00'>⏳ Connecting to WiFi network...</p>");
            snprintf(temp_buf, sizeof(temp_buf), "<p style='margin:5px 0;color:#0277bd'>* Target: %s</p>", g_system_config.wifi_ssid);
            httpd_resp_sendstr_chunk(req, temp_buf);
            httpd_resp_sendstr_chunk(req, "<p style='margin:8px 0;color:#0277bd;font-size:14px'><strong>SoftAP Access:</strong> 'ModbusIoT-Config' remains active at 192.168.4.1</p>");
            httpd_resp_sendstr_chunk(req, "<p style='margin:5px 0;color:#666;font-size:13px'>If connection fails, you can still access this interface via the SoftAP.</p>");
        }
        httpd_resp_sendstr_chunk(req, "</div>");
    }
    
    httpd_resp_sendstr_chunk(req, "<button onclick='location.href=\"/\"'>[BACK] Back to Configuration</button>");
    httpd_resp_sendstr_chunk(req, "<button onclick='startOperation()'>[START] Start Operation Mode</button>");
    httpd_resp_sendstr_chunk(req, "<script>");
    httpd_resp_sendstr_chunk(req, "function startOperation() {");
    httpd_resp_sendstr_chunk(req, "  if(confirm('Switch to operation mode? This will restart the device.')) {");
    httpd_resp_sendstr_chunk(req, "    fetch('/start_operation', {method: 'POST'})");
    httpd_resp_sendstr_chunk(req, "    .then(() => {");
    httpd_resp_sendstr_chunk(req, "      alert('Device will restart in operation mode...');");
    httpd_resp_sendstr_chunk(req, "      setTimeout(() => location.reload(), 3000);");
    httpd_resp_sendstr_chunk(req, "    });");
    httpd_resp_sendstr_chunk(req, "  }");
    httpd_resp_sendstr_chunk(req, "}");
    httpd_resp_sendstr_chunk(req, "</script>");
    httpd_resp_sendstr_chunk(req, html_footer);
    
    httpd_resp_sendstr_chunk(req, NULL);
    
    // Attempt WiFi connection AFTER HTTP response is completely sent
    if (attempt_wifi_connection) {
        ESP_LOGI(TAG, "[WIFI] HTTP response sent, now attempting WiFi connection to: %s", g_system_config.wifi_ssid);
        
        // Skip watchdog reset in HTTP handler (task not registered with watchdog)
        
        // Give a brief delay to ensure HTTP response is fully transmitted
        vTaskDelay(pdMS_TO_TICKS(500));
        
        esp_err_t wifi_result = connect_to_wifi_network();
        if (wifi_result == ESP_OK) {
            ESP_LOGI(TAG, "[WIFI] WiFi connection process initiated successfully");
        } else {
            ESP_LOGW(TAG, "[WIFI] Failed to initiate WiFi connection: %s", esp_err_to_name(wifi_result));
        }
    }
    
    return ESP_OK;
}

// Test sensor endpoint
static esp_err_t test_sensor_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        const char* error_response = "{\"status\":\"error\",\"message\":\"No data received\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, error_response, strlen(error_response));
        return ESP_OK;
    }
    buf[ret] = '\0';
    
    // Parse sensor ID from request (expect sensor_id=X)
    int sensor_id = -1;
    char *equals = strchr(buf, '=');
    if (equals) {
        sensor_id = atoi(equals + 1);
    }
    
    system_config_t* config = get_system_config();
    
    if (sensor_id >= 0 && sensor_id < config->sensor_count && config->sensors[sensor_id].enabled) {
        sensor_config_t* sensor = &config->sensors[sensor_id];
        
        ESP_LOGI(TAG, "Testing sensor %d: %s (Slave: %d, Reg: %d, RegType: %s, DataType: %s)", 
                 sensor_id + 1, sensor->name, sensor->slave_id, 
                 sensor->register_address, sensor->register_type[0] ? sensor->register_type : "HOLDING", sensor->data_type);
        
        // Always attempt real Modbus communication if sensor is configured
        // (Modbus should be initialized in both setup and operation modes)
        ESP_LOGI(TAG, "Attempting real RS485 Modbus communication...");
        
        // Set the baud rate for this sensor before testing
        int baud_rate = sensor->baud_rate > 0 ? sensor->baud_rate : 9600;
        ESP_LOGI(TAG, "Setting baud rate to %d bps for testing sensor '%s'", baud_rate, sensor->name);
        esp_err_t baud_err = modbus_set_baud_rate(baud_rate);
        if (baud_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set baud rate: %s", esp_err_to_name(baud_err));
        }
        
        // Perform real Modbus communication
        // Allocate format_table buffer before modbus operation (needed for both success/error paths)
        char *format_table = (char*)malloc(10000);  // Increased buffer for 4-register sensors like ZEST
        if (format_table == NULL) {
            ESP_LOGE(TAG, "Failed to allocate format_table buffer");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        memset(format_table, 0, 10000);  // Initialize buffer to prevent undefined behavior

        // Perform Modbus read based on register type
        modbus_result_t result;
        const char* reg_type = sensor->register_type;
        if (!reg_type || strlen(reg_type) == 0) {
            reg_type = "HOLDING";
        }
        
        if (strcmp(reg_type, "INPUT") == 0) {
            ESP_LOGI(TAG, "[MODBUS] Reading INPUT registers (function 04) for sensor '%s'", sensor->name);
            result = modbus_read_input_registers(sensor->slave_id, sensor->register_address, sensor->quantity);
        } else {
            ESP_LOGI(TAG, "[MODBUS] Reading HOLDING registers (function 03) for sensor '%s' - type '%s'", sensor->name, reg_type);
            result = modbus_read_holding_registers(sensor->slave_id, sensor->register_address, sensor->quantity);
        }
        
        if (result == MODBUS_SUCCESS) {
            // Use the same comprehensive logic as test_rs485_handler
            // Get the raw register values
            uint16_t registers[4]; // Limit to 4 registers to prevent overflow
            int reg_count = modbus_get_response_length();
            if (reg_count > 4 || reg_count <= 0) {
                ESP_LOGW(TAG, "Invalid register count: %d, limiting to safe range", reg_count);
                reg_count = (reg_count > 4) ? 4 : 1; // Safety limit
            }
            
            for (int i = 0; i < reg_count && i < 4; i++) {
                registers[i] = modbus_get_response_buffer(i);
            }
            
            // Create comprehensive ScadaCore format interpretation table
            // Use heap allocation for large content to prevent stack overflow
            
            // Build the comprehensive data format table
            snprintf(format_table, 10000,
                     "<div class='test-result'>"
                     "<h4>✓ RS485 Success - %d Registers Read</h4>", reg_count);
            
            // Add primary configured value first
            double primary_value = 0.0;
            if (reg_count >= 4 && strstr(sensor->data_type, "FLOAT64")) {
                // FLOAT64 handling - 4 registers (64-bit double precision)
                uint64_t raw_val64 = 0;
                if (strstr(sensor->data_type, "12345678")) {
                    // FLOAT64_12345678 (ABCDEFGH) - Standard big endian
                    raw_val64 = ((uint64_t)registers[0] << 48) | ((uint64_t)registers[1] << 32) | 
                               ((uint64_t)registers[2] << 16) | registers[3];
                } else if (strstr(sensor->data_type, "87654321")) {
                    // FLOAT64_87654321 (HGFEDCBA) - Full little endian
                    raw_val64 = ((uint64_t)registers[3] << 48) | ((uint64_t)registers[2] << 32) | 
                               ((uint64_t)registers[1] << 16) | registers[0];
                } else {
                    // Default to standard big endian if specific order not found
                    raw_val64 = ((uint64_t)registers[0] << 48) | ((uint64_t)registers[1] << 32) | 
                               ((uint64_t)registers[2] << 16) | registers[3];
                }
                union { uint64_t i; double d; } conv64;
                conv64.i = raw_val64;
                primary_value = conv64.d * sensor->scale_factor;
            } else if (reg_count >= 2 && strstr(sensor->data_type, "FLOAT32")) {
                uint32_t raw_val = strstr(sensor->data_type, "4321") ? 
                    ((uint32_t)registers[1] << 16) | registers[0] :
                    ((uint32_t)registers[0] << 16) | registers[1];
                union { uint32_t i; float f; } conv;
                conv.i = raw_val;
                primary_value = (double)conv.f * sensor->scale_factor;
            } else if (reg_count >= 2 && (strstr(sensor->data_type, "UINT32") || strstr(sensor->data_type, "INT32"))) {
                uint32_t raw_val32 = 0;
                
                // Handle comprehensive INT32/UINT32 byte order patterns
                if (strstr(sensor->data_type, "4321") || strstr(sensor->data_type, "DCBA")) {
                    // UINT32_4321 (DCBA) - Little endian
                    raw_val32 = ((uint32_t)registers[1] << 16) | registers[0];
                } else if (strstr(sensor->data_type, "3412") || strstr(sensor->data_type, "CDAB")) {
                    // UINT32_3412 (CDAB) - Mixed endian
                    raw_val32 = ((uint32_t)registers[0] << 16) | registers[1];
                } else if (strstr(sensor->data_type, "2143") || strstr(sensor->data_type, "BADC")) {
                    // UINT32_2143 (BADC) - Mixed byte swap
                    uint16_t reg0_swapped = ((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF);
                    uint16_t reg1_swapped = ((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF);
                    raw_val32 = ((uint32_t)reg0_swapped << 16) | reg1_swapped;
                } else {
                    // Default: UINT32_1234 (ABCD) - Big endian
                    raw_val32 = ((uint32_t)registers[0] << 16) | registers[1];
                }
                
                if (strstr(sensor->data_type, "INT32")) {
                    primary_value = (double)(int32_t)raw_val32 * sensor->scale_factor;
                } else {
                    primary_value = (double)raw_val32 * sensor->scale_factor;
                }
            } else if (reg_count >= 1) {
                primary_value = (double)registers[0] * sensor->scale_factor;
            }
            
            char temp_str[1000];
            
            // Calculate the final display value based on sensor type
            double display_value = primary_value;
            char value_desc[100];
            
            if (strcmp(sensor->sensor_type, "Radar Level") == 0 && sensor->max_water_level > 0) {
                display_value = (primary_value / sensor->max_water_level) * 100.0;
                if (display_value < 0) display_value = 0.0;
                snprintf(value_desc, sizeof(value_desc), "Radar Level %.2f%%", display_value);
            } else if (strcmp(sensor->sensor_type, "Level") == 0 && sensor->max_water_level > 0) {
                display_value = ((sensor->sensor_height - primary_value) / sensor->max_water_level) * 100.0;
                if (display_value < 0) display_value = 0.0;
                if (display_value > 100) display_value = 100.0;
                snprintf(value_desc, sizeof(value_desc), "Level %.2f%%", display_value);
            } else if (strcmp(sensor->sensor_type, "ZEST") == 0 && reg_count >= 4) {
                // ZEST sensor special handling - 4-register format
                // Register[0]: Integer part (UINT16)
                // Register[1]: Unused (0x0000)
                // Registers[2-3]: Decimal part (FLOAT32 Big Endian ABCD)

                // Integer part from register[0]
                uint32_t integer_part = (uint32_t)registers[0];
                double value1 = (double)integer_part;

                // Decimal part from registers[2-3] as IEEE 754 float
                uint32_t float_bits = ((uint32_t)registers[2] << 16) | registers[3];
                float decimal_float;
                memcpy(&decimal_float, &float_bits, sizeof(float));
                double value2 = (double)decimal_float;

                display_value = (value1 + value2) * sensor->scale_factor;
                snprintf(value_desc, sizeof(value_desc), "ZEST: UINT16(%lu) + FLOAT32(%.6f) = %.6f",
                         (unsigned long)integer_part, value2, display_value);
            } else if (strcmp(sensor->sensor_type, "Panda_USM") == 0 && reg_count >= 4) {
                // Panda USM sensor - 64-bit double format
                // Registers[0-3]: Net Volume (FLOAT64 Big Endian)
                uint64_t combined_value64 = ((uint64_t)registers[0] << 48) |
                                           ((uint64_t)registers[1] << 32) |
                                           ((uint64_t)registers[2] << 16) |
                                           registers[3];

                double net_volume;
                memcpy(&net_volume, &combined_value64, sizeof(double));

                display_value = net_volume * sensor->scale_factor;
                snprintf(value_desc, sizeof(value_desc), "Panda USM: DOUBLE64 = %.6f m³",
                         display_value);
            } else {
                snprintf(value_desc, sizeof(value_desc), "%s×%.3f", sensor->data_type, sensor->scale_factor);
            }
            
            snprintf(temp_str, sizeof(temp_str),
                     "<div class='value-box'><b>Configured Value:</b> %.6f (%s)</div>"
                     "<div><b>Raw Hex:</b> <span class='hex-display'>", display_value, value_desc);
            
            // Add operation team comparison display for Level/Radar Level sensors
            if ((strcmp(sensor->sensor_type, "Radar Level") == 0 || strcmp(sensor->sensor_type, "Level") == 0) && sensor->max_water_level > 0) {
                char comparison_str[200];
                snprintf(comparison_str, sizeof(comparison_str),
                         "<br><b>Operation Comparison:</b> %.0f → %.1f%% ✅<br>",
                         primary_value, display_value);
                strcat(format_table, temp_str);
                strcat(format_table, comparison_str);
            } else if (strcmp(sensor->sensor_type, "ZEST") == 0 && reg_count >= 4) {
                // Add detailed ZEST calculation breakdown for 4-register format
                // ZEST Format: Register[0] = Integer part (UINT16)
                //              Register[1] = Unused (0x0000)
                //              Registers[2-3] = Decimal part (FLOAT32 Big Endian)
                strcat(format_table, temp_str);

                // Integer part from register[0]
                uint32_t integer_part = (uint32_t)registers[0];
                double value1 = (double)integer_part;

                // Decimal part from registers[2-3] as IEEE 754 float (Big Endian ABCD)
                uint32_t float_bits = ((uint32_t)registers[2] << 16) | registers[3];
                float decimal_float;
                memcpy(&decimal_float, &float_bits, sizeof(float));
                double value2 = (double)decimal_float;

                // Calculate correct ZEST total sum with scale factor
                double zest_total = (value1 + value2) * sensor->scale_factor;

                char zest_breakdown[700];
                snprintf(zest_breakdown, sizeof(zest_breakdown),
                         "<br><div class='scada-breakdown'>"
                         "<b>ZEST Calculation Breakdown:</b><br>"
                         "* Register [0] as UINT16: 0x%04X = %lu (Integer Part)<br>"
                         "* Register [1]: 0x%04X (Unused)<br>"
                         "* Registers [2-3] as FLOAT32_ABCD: 0x%04X%04X = %.6f (Decimal Part)<br>"
                         "* <b>Total = (%.0f + %.6f) × %.3f = %.6f</b> ✅"
                         "</div>",
                         registers[0], (unsigned long)integer_part,
                         registers[1],
                         registers[2], registers[3], value2,
                         value1, value2, sensor->scale_factor, zest_total);
                strcat(format_table, zest_breakdown);
            } else {
                strcat(format_table, temp_str);
            }
            
            for (int i = 0; i < reg_count && i < 4; i++) {
                snprintf(temp_str, sizeof(temp_str), "%04X ", registers[i]);
                strcat(format_table, temp_str);
            }
            strcat(format_table, "</span></div>");
            
            // Add comprehensive format interpretations table
            strcat(format_table,
                   "<div style='overflow-x:auto;margin-top:var(--space-md)'>"
                   "<table class='scada-table'>"
                   "<tr class='scada-header-main'><th colspan='4'>ALL SCADACORE DATA FORMAT INTERPRETATIONS</th></tr>");
            
            // 16-bit formats (if 1+ registers)
            if (reg_count >= 1) {
                uint16_t reg0 = registers[0];
                snprintf(temp_str, sizeof(temp_str),
                         "<tr><td>UINT16_BE:</td><td>%u</td><td>INT16_BE:</td><td>%d</td></tr>"
                         "<tr><td>UINT16_LE:</td><td>%u</td><td>INT16_LE:</td><td>%d</td></tr>",
                         reg0, (int16_t)reg0,
                         ((reg0 & 0xFF) << 8) | (reg0 >> 8), (int16_t)(((reg0 & 0xFF) << 8) | (reg0 >> 8)));
                strcat(format_table, temp_str);
            }
            
            // 32-bit formats (if 2+ registers) - Comprehensive ScadaCore variations
            if (reg_count >= 2) {
                // All 32-bit byte order interpretations for ScadaCore compatibility
                uint32_t val_1234_abcd = ((uint32_t)registers[0] << 16) | registers[1];
                uint32_t val_4321_dcba = ((uint32_t)registers[1] << 16) | registers[0];
                uint32_t val_2143_badc = ((uint32_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 16) | 
                                        (((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF));
                uint32_t val_3412_cdab = ((uint32_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) | 
                                        (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
                
                // 32-bit FLOAT conversions
                union { uint32_t i; float f; } float_conv;
                float_conv.i = val_1234_abcd; float float_1234_abcd = float_conv.f;
                float_conv.i = val_4321_dcba; float float_4321_dcba = float_conv.f;
                float_conv.i = val_2143_badc; float float_2143_badc = float_conv.f;
                float_conv.i = val_3412_cdab; float float_3412_cdab = float_conv.f;
                
                // FLOAT32 comprehensive variations - ScadaCore compatible
                snprintf(temp_str, sizeof(temp_str),
                         "<tr class='scada-header-float'><th colspan='4'>FLOAT32 FORMAT INTERPRETATIONS</th></tr>"
                         "<tr><td><strong>FLOAT32_1234 (ABCD):</strong></td><td>%.6f</td><td><strong>FLOAT32_4321 (DCBA):</strong></td><td>%.6f</td></tr>"
                         "<tr><td><strong>FLOAT32_2143 (BADC):</strong></td><td>%.6f</td><td><strong>FLOAT32_3412 (CDAB):</strong></td><td>%.6f</td></tr>",
                         float_1234_abcd, float_4321_dcba, float_2143_badc, float_3412_cdab);
                strcat(format_table, temp_str);

                // INT32 comprehensive variations - ScadaCore compatible
                snprintf(temp_str, sizeof(temp_str),
                         "<tr class='scada-header-int'><th colspan='4'>INT32 FORMAT INTERPRETATIONS</th></tr>"
                         "<tr><td><strong>INT32_1234 (ABCD):</strong></td><td>%ld</td><td><strong>INT32_4321 (DCBA):</strong></td><td>%ld</td></tr>"
                         "<tr><td><strong>INT32_2143 (BADC):</strong></td><td>%ld</td><td><strong>INT32_3412 (CDAB):</strong></td><td>%ld</td></tr>",
                         (int32_t)val_1234_abcd, (int32_t)val_4321_dcba, (int32_t)val_2143_badc, (int32_t)val_3412_cdab);
                strcat(format_table, temp_str);

                // UINT32 comprehensive variations - ScadaCore compatible
                snprintf(temp_str, sizeof(temp_str),
                         "<tr class='scada-header-uint'><th colspan='4'>UINT32 FORMAT INTERPRETATIONS</th></tr>"
                         "<tr><td><strong>UINT32_1234 (ABCD):</strong></td><td>%lu</td><td><strong>UINT32_4321 (DCBA):</strong></td><td>%lu</td></tr>"
                         "<tr><td><strong>UINT32_2143 (BADC):</strong></td><td>%lu</td><td><strong>UINT32_3412 (CDAB):</strong></td><td>%lu</td></tr>",
                         val_1234_abcd, val_4321_dcba, val_2143_badc, val_3412_cdab);
                strcat(format_table, temp_str);
            }
            
            // 64-bit formats (if 4 registers) - Comprehensive ScadaCore variations
            if (reg_count >= 4) {
                // All 64-bit byte order interpretations for ScadaCore compatibility
                uint64_t val64_12345678 = ((uint64_t)registers[0] << 48) | ((uint64_t)registers[1] << 32) | 
                                         ((uint64_t)registers[2] << 16) | registers[3];
                uint64_t val64_87654321 = ((uint64_t)registers[3] << 48) | ((uint64_t)registers[2] << 32) | 
                                         ((uint64_t)registers[1] << 16) | registers[0];
                uint64_t val64_21436587 = ((uint64_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 48) |
                                         ((uint64_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 32) |
                                         ((uint64_t)(((registers[2] & 0xFF) << 8) | ((registers[2] >> 8) & 0xFF)) << 16) |
                                         (((registers[3] & 0xFF) << 8) | ((registers[3] >> 8) & 0xFF));
                uint64_t val64_78563412 = ((uint64_t)(((registers[3] & 0xFF) << 8) | ((registers[3] >> 8) & 0xFF)) << 48) |
                                         ((uint64_t)(((registers[2] & 0xFF) << 8) | ((registers[2] >> 8) & 0xFF)) << 32) |
                                         ((uint64_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) |
                                         (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
                
                // 64-bit FLOAT conversions
                union { uint64_t i; double d; } float64_conv;
                
                float64_conv.i = val64_12345678; double float64_12345678 = float64_conv.d;
                float64_conv.i = val64_87654321; double float64_87654321 = float64_conv.d;
                float64_conv.i = val64_21436587; double float64_21436587 = float64_conv.d;
                float64_conv.i = val64_78563412; double float64_78563412 = float64_conv.d;
                
                // FLOAT64 comprehensive variations - ScadaCore compatible
                snprintf(temp_str, sizeof(temp_str),
                         "<tr class='scada-header-float64'><th colspan='4'>FLOAT64 FORMAT INTERPRETATIONS</th></tr>"
                         "<tr><td><strong>FLOAT64_12345678 (ABCDEFGH):</strong></td><td>%.3f</td><td><strong>FLOAT64_87654321 (HGFEDCBA):</strong></td><td>%.3f</td></tr>"
                         "<tr><td><strong>FLOAT64_21436587 (BADCFEHG):</strong></td><td>%.3f</td><td><strong>FLOAT64_78563412 (GHEFCDAB):</strong></td><td>%.3f</td></tr>",
                         float64_12345678, float64_87654321, float64_21436587, float64_78563412);
                strcat(format_table, temp_str);

                // INT64 comprehensive variations - ScadaCore compatible
                snprintf(temp_str, sizeof(temp_str),
                         "<tr class='scada-header-int64'><th colspan='4'>INT64 FORMAT INTERPRETATIONS</th></tr>"
                         "<tr><td><strong>INT64_12345678 (ABCDEFGH):</strong></td><td>%lld</td><td><strong>INT64_87654321 (HGFEDCBA):</strong></td><td>%lld</td></tr>"
                         "<tr><td><strong>INT64_21436587 (BADCFEHG):</strong></td><td>%lld</td><td><strong>INT64_78563412 (GHEFCDAB):</strong></td><td>%lld</td></tr>",
                         (int64_t)val64_12345678, (int64_t)val64_87654321, (int64_t)val64_21436587, (int64_t)val64_78563412);
                strcat(format_table, temp_str);

                // UINT64 comprehensive variations - ScadaCore compatible
                snprintf(temp_str, sizeof(temp_str),
                         "<tr class='scada-header-uint64'><th colspan='4'>UINT64 FORMAT INTERPRETATIONS</th></tr>"
                         "<tr><td><strong>UINT64_12345678 (ABCDEFGH):</strong></td><td>%llu</td><td><strong>UINT64_87654321 (HGFEDCBA):</strong></td><td>%llu</td></tr>"
                         "<tr><td><strong>UINT64_21436587 (BADCFEHG):</strong></td><td>%llu</td><td><strong>UINT64_78563412 (GHEFCDAB):</strong></td><td>%llu</td></tr>",
                         val64_12345678, val64_87654321, val64_21436587, val64_78563412);
                strcat(format_table, temp_str);
                
                // Raw hex display for reference
                snprintf(temp_str, sizeof(temp_str),
                         "<tr style='background:#f0f0f0'><td><strong>Raw Hex 64-bit:</strong></td><td colspan='3'>0x%04X%04X%04X%04X</td></tr>",
                         registers[0], registers[1], registers[2], registers[3]);
                strcat(format_table, temp_str);
            }
            
            strcat(format_table, "</table></div></div>");
            
            // Send HTML response directly (avoids JSON escaping issues) 
            httpd_resp_set_type(req, "text/html");
            httpd_resp_send(req, format_table, strlen(format_table));
            free(format_table);  // Free the heap-allocated format table
            return ESP_OK;
        } else {
            // Modbus communication failed - provide detailed troubleshooting
            const char* error_msg;
            
            switch (result) {
                case MODBUS_TIMEOUT:
                    error_msg = "RS485 Communication Timeout - No response from device";
                    break;
                case MODBUS_INVALID_CRC:
                    error_msg = "Invalid CRC - Data corruption in RS485 communication";
                    break;
                case MODBUS_ILLEGAL_DATA_ADDRESS:
                    error_msg = "Invalid register address - Register not available on device";
                    break;
                case MODBUS_SLAVE_DEVICE_FAILURE:
                    error_msg = "Sensor device internal failure";
                    break;
                default:
                    error_msg = "RS485 communication failed";
                    break;
            }
            
            // Create HTML error response to match success format
            snprintf(format_table, 10000,
                "<div style='background:#f8d7da;padding:15px;border-radius:5px;margin:5px 0;border-left:4px solid #dc3545'>"
                "<h4 style='color:#721c24;margin:0 0 10px 0'>❌ RS485 Communication Failed</h4>"
                "<div style='color:#721c24;font-weight:bold'>Error: %s</div>"
                "<div style='margin-top:10px;font-size:14px'>Slave ID: %d | Register: %d | Type: %s | Baud: %d</div>"
                "<div style='margin-top:15px;padding:10px;background:rgba(255,255,255,0.8);border-radius:4px'>"
                "<strong>🔧 Troubleshooting Steps:</strong>"
                "<ul style='margin:5px 0 0 20px;padding:0'>"
                "<li>Check physical RS485 connections (A, B, GND)</li>"
                "<li>Verify device power and slave ID configuration</li>"
                "<li>Confirm register address and data type settings</li>"
                "<li>Try different baud rates (9600, 19200, 38400)</li>"
                "<li>Ensure proper RS485 termination resistors</li>"
                "</ul></div></div>",
                error_msg, sensor->slave_id, sensor->register_address, sensor->register_type[0] ? sensor->register_type : "HOLDING", baud_rate);
            
            httpd_resp_set_type(req, "text/html");
            httpd_resp_send(req, format_table, strlen(format_table));
            free(format_table);  // Free the heap-allocated format table (even on error)
            return ESP_OK;
        }
    } else {
        char *format_table = (char*)malloc(1000);  // Allocate buffer for error response
        if (format_table == NULL) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        snprintf(format_table, 1000,
            "<div style='background:#f8d7da;padding:15px;border-radius:5px;margin:5px 0;border-left:4px solid #dc3545'>"
            "<h4 style='color:#721c24;margin:0 0 10px 0'>❌ Sensor Configuration Error</h4>"
            "<div style='color:#721c24;font-weight:bold'>Invalid sensor ID: %d</div>"
            "<div style='margin-top:10px;font-size:14px'>Please refresh the page and try again.</div>"
            "</div>", sensor_id);
            
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, format_table, strlen(format_table));
        free(format_table);
        return ESP_OK;
    }
}

// Save single sensor handler
static esp_err_t save_single_sensor_handler(httpd_req_t *req)
{
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        const char* error_response = "{\"status\":\"error\",\"message\":\"No data received\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, error_response, strlen(error_response));
        return ESP_OK;
    }
    buf[ret] = '\0';
    
    ESP_LOGI(TAG, "Received single sensor data: %s", buf);
    
    // Parse form data
    char *param_start = buf;
    int sensor_id = -1;
    bool has_name = false, has_unit_id = false;
    
    while (param_start) {
        char *param_end = strchr(param_start, '&');
        if (param_end) {
            *param_end = '\0';
        }
        
        char *equals = strchr(param_start, '=');
        if (equals) {
            *equals = '\0';
            char *param_name = param_start;
            char *param_value = equals + 1;
            
            // URL decode the value
            char decoded_value[256];
            url_decode(decoded_value, param_value);
            
            ESP_LOGI(TAG, "Processing: %s = %s", param_name, decoded_value);
            
            // Parse sensor parameters
            if (strncmp(param_name, "sensor_", 7) == 0) {
                // Check if this is a sub-sensor parameter first (contains "_sub_")
                char *sub_start = strstr(param_name, "_sub_");
                if (sub_start) {
                    // Handle sub-sensor parameters (pattern: sensor_{index}_sub_{subIndex}_{param})
                    int sensor_idx = atoi(param_name + 7);
                    int sub_idx = atoi(sub_start + 5);
                    char *param_type = strchr(sub_start + 5, '_');
                    
                    if (param_type && sensor_idx >= 0 && sensor_idx < 8 && sub_idx >= 0 && sub_idx < 8) {
                        param_type++; // Skip the underscore
                        ESP_LOGI(TAG, "Processing sub-sensor: sensor[%d].sub[%d].%s = %s", sensor_idx, sub_idx, param_type, decoded_value);
                        
                        // Ensure sensor is marked as enabled and is QUALITY type
                        if (strcmp(g_system_config.sensors[sensor_idx].sensor_type, "QUALITY") == 0) {
                            // Enable the sub-sensor when we process any of its parameters
                            g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].enabled = true;
                            
                            // Initialize default values if not already set
                            if (g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].quantity == 0) {
                                g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].quantity = 1;
                            }
                            if (strlen(g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].register_type) == 0) {
                                strcpy(g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].register_type, "HOLDING_REGISTER");
                            }
                            if (strlen(g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].data_type) == 0) {
                                strcpy(g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].data_type, "UINT16_HI");
                            }
                            if (g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].scale_factor == 0.0) {
                                g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].scale_factor = 1.0;
                            }
                            
                            if (strcmp(param_type, "parameter") == 0) {
                                strncpy(g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].parameter_name, decoded_value, 
                                       sizeof(g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].parameter_name) - 1);
                                g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].parameter_name[sizeof(g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].parameter_name) - 1] = '\0';
                            } else if (strcmp(param_type, "slave_id") == 0) {
                                g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].slave_id = atoi(decoded_value);
                            } else if (strcmp(param_type, "register") == 0) {
                                g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].register_address = atoi(decoded_value);
                            } else if (strcmp(param_type, "quantity") == 0) {
                                g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].quantity = atoi(decoded_value);
                            } else if (strcmp(param_type, "register_type") == 0) {
                                strncpy(g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].register_type, decoded_value,
                                       sizeof(g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].register_type) - 1);
                                g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].register_type[sizeof(g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].register_type) - 1] = '\0';
                            } else if (strcmp(param_type, "data_type") == 0) {
                                strncpy(g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].data_type, decoded_value,
                                       sizeof(g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].data_type) - 1);
                                g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].data_type[sizeof(g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].data_type) - 1] = '\0';
                            } else if (strcmp(param_type, "scale_factor") == 0) {
                                g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].scale_factor = atof(decoded_value);
                            } else if (strcmp(param_type, "scale") == 0) {
                                g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].scale_factor = atof(decoded_value);
                            }
                            
                            // Update sub-sensor count if this is the highest index
                            if (sub_idx >= g_system_config.sensors[sensor_idx].sub_sensor_count) {
                                g_system_config.sensors[sensor_idx].sub_sensor_count = sub_idx + 1;
                            }
                            
                            ESP_LOGI(TAG, "Updated sensor[%d] sub_sensor_count to %d", sensor_idx, g_system_config.sensors[sensor_idx].sub_sensor_count);
                            ESP_LOGI(TAG, "Sub-sensor[%d][%d]: param='%s', slave_id=%d, reg=%d, data_type='%s', scale=%.3f", 
                                    sensor_idx, sub_idx,
                                    g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].parameter_name,
                                    g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].slave_id,
                                    g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].register_address,
                                    g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].data_type,
                                    g_system_config.sensors[sensor_idx].sub_sensors[sub_idx].scale_factor);
                        }
                    }
                } else {
                    // Handle regular sensor parameters
                    int current_sensor_idx = atoi(param_name + 7);
                    if (sensor_id == -1) sensor_id = current_sensor_idx;
                    
                    if (current_sensor_idx >= 0 && current_sensor_idx < 8) {
                        char *param_type = strchr(param_name + 7, '_');
                        if (param_type) {
                            param_type++; // Skip the underscore
                        
                        if (strcmp(param_type, "name") == 0) {
                            strncpy(g_system_config.sensors[current_sensor_idx].name, decoded_value, sizeof(g_system_config.sensors[current_sensor_idx].name) - 1);
                            g_system_config.sensors[current_sensor_idx].enabled = true;
                            has_name = (strlen(decoded_value) > 0);
                            // Initialize sub-sensor count for new sensors
                            g_system_config.sensors[current_sensor_idx].sub_sensor_count = 0;
                            // Initialize default data_type if not already set
                            if (strlen(g_system_config.sensors[current_sensor_idx].data_type) == 0) {
                                strcpy(g_system_config.sensors[current_sensor_idx].data_type, "UINT16_HI");
                                ESP_LOGI(TAG, "Save_single: Set default data_type to UINT16_HI for new sensor %d", current_sensor_idx);
                            }
                            // Update sensor count if this is a new sensor
                            if (current_sensor_idx >= g_system_config.sensor_count) {
                                g_system_config.sensor_count = current_sensor_idx + 1;
                            }
                        } else if (strcmp(param_type, "unit_id") == 0) {
                            strncpy(g_system_config.sensors[current_sensor_idx].unit_id, decoded_value, sizeof(g_system_config.sensors[current_sensor_idx].unit_id) - 1);
                            has_unit_id = (strlen(decoded_value) > 0);
                        } else if (strcmp(param_type, "slave_id") == 0) {
                            g_system_config.sensors[current_sensor_idx].slave_id = atoi(decoded_value);
                        } else if (strcmp(param_type, "register_address") == 0) {
                            g_system_config.sensors[current_sensor_idx].register_address = atoi(decoded_value);
                        } else if (strcmp(param_type, "quantity") == 0) {
                            g_system_config.sensors[current_sensor_idx].quantity = atoi(decoded_value);
                        } else if (strcmp(param_type, "data_type") == 0) {
                            ESP_LOGI(TAG, "Save_single: Processing data_type - Original: '%s' (len=%d)", decoded_value, strlen(decoded_value));
                            strncpy(g_system_config.sensors[current_sensor_idx].data_type, decoded_value, sizeof(g_system_config.sensors[current_sensor_idx].data_type) - 1);
                            g_system_config.sensors[current_sensor_idx].data_type[sizeof(g_system_config.sensors[current_sensor_idx].data_type) - 1] = '\0';
                            ESP_LOGI(TAG, "Save_single: Stored data_type: '%s' (len=%d) [Buffer size: %d]", 
                                   g_system_config.sensors[current_sensor_idx].data_type, 
                                   strlen(g_system_config.sensors[current_sensor_idx].data_type),
                                   sizeof(g_system_config.sensors[current_sensor_idx].data_type));
                        } else if (strcmp(param_type, "baud_rate") == 0) {
                            g_system_config.sensors[current_sensor_idx].baud_rate = atoi(decoded_value);
                        } else if (strcmp(param_type, "parity") == 0) {
                            strncpy(g_system_config.sensors[current_sensor_idx].parity, decoded_value, sizeof(g_system_config.sensors[current_sensor_idx].parity) - 1);
                        } else if (strcmp(param_type, "scale_factor") == 0) {
                            float parsed_scale = atof(decoded_value);
                            g_system_config.sensors[current_sensor_idx].scale_factor = (parsed_scale == 0.0) ? 1.0 : parsed_scale;
                            ESP_LOGI(TAG, "Save_single: Parsed scale_factor: '%s' -> %.3f (final: %.3f) for sensor %d", 
                                     decoded_value, parsed_scale, g_system_config.sensors[current_sensor_idx].scale_factor, current_sensor_idx);
                        } else if (strcmp(param_type, "register_type") == 0) {
                            strncpy(g_system_config.sensors[current_sensor_idx].register_type, decoded_value, sizeof(g_system_config.sensors[current_sensor_idx].register_type) - 1);
                            g_system_config.sensors[current_sensor_idx].register_type[sizeof(g_system_config.sensors[current_sensor_idx].register_type) - 1] = '\0';
                        } else if (strcmp(param_type, "sensor_type") == 0) {
                            strncpy(g_system_config.sensors[current_sensor_idx].sensor_type, decoded_value, sizeof(g_system_config.sensors[current_sensor_idx].sensor_type) - 1);
                            g_system_config.sensors[current_sensor_idx].sensor_type[sizeof(g_system_config.sensors[current_sensor_idx].sensor_type) - 1] = '\0';
                            ESP_LOGI(TAG, "Save_single: Stored sensor_type: '%s' for sensor %d", g_system_config.sensors[current_sensor_idx].sensor_type, current_sensor_idx);
                        } else if (strcmp(param_type, "sensor_height") == 0) {
                            g_system_config.sensors[current_sensor_idx].sensor_height = atof(decoded_value);
                        } else if (strcmp(param_type, "max_water_level") == 0) {
                            g_system_config.sensors[current_sensor_idx].max_water_level = atof(decoded_value);
                        } else if (strcmp(param_type, "meter_type") == 0) {
                            strncpy(g_system_config.sensors[current_sensor_idx].meter_type, decoded_value, sizeof(g_system_config.sensors[current_sensor_idx].meter_type) - 1);
                            g_system_config.sensors[current_sensor_idx].meter_type[sizeof(g_system_config.sensors[current_sensor_idx].meter_type) - 1] = '\0';
                        }
                        }
                    }
                }
            }
        }
        
        if (param_end) {
            param_start = param_end + 1;
        } else {
            break;
        }
    }
    
    // Validate required fields
    if (!has_name || !has_unit_id) {
        char response[256];
        snprintf(response, sizeof(response),
            "{\"status\":\"error\",\"message\":\"Missing required fields: %s%s\"}",
            !has_name ? "Name " : "",
            !has_unit_id ? "Unit ID" : ""
        );
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }
    
    // Save configuration to NVS
    esp_err_t err = config_save_to_nvs(&g_system_config);
    
    char response[512];
    if (err == ESP_OK) {
        snprintf(response, sizeof(response),
            "{\"status\":\"success\",\"message\":\"Sensor %d saved successfully\",\"sensor_name\":\"%s\",\"unit_id\":\"%s\"}",
            sensor_id + 1,
            g_system_config.sensors[sensor_id].name,
            g_system_config.sensors[sensor_id].unit_id
        );
        ESP_LOGI(TAG, "Single sensor %d saved successfully", sensor_id);
    } else {
        snprintf(response, sizeof(response),
            "{\"status\":\"error\",\"message\":\"Failed to save sensor configuration\"}"
        );
        ESP_LOGE(TAG, "Failed to save single sensor configuration: %s", esp_err_to_name(err));
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// Delete sensor handler
static esp_err_t delete_sensor_handler(httpd_req_t *req)
{
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        const char* error_response = "{\"status\":\"error\",\"message\":\"No data received\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, error_response, strlen(error_response));
        return ESP_OK;
    }
    buf[ret] = '\0';
    
    int sensor_id = -1;
    char *equals = strchr(buf, '=');
    if (equals) {
        sensor_id = atoi(equals + 1);
    }
    
    char response[256];
    
    if (sensor_id >= 0 && sensor_id < g_system_config.sensor_count) {
        // Shift all sensors after this one down by 1
        for (int i = sensor_id; i < g_system_config.sensor_count - 1; i++) {
            g_system_config.sensors[i] = g_system_config.sensors[i + 1];
        }
        
        // Clear the last sensor and decrement count
        memset(&g_system_config.sensors[g_system_config.sensor_count - 1], 0, sizeof(sensor_config_t));
        g_system_config.sensor_count--;
        
        // Save to NVS
        esp_err_t err = config_save_to_nvs(&g_system_config);
        if (err == ESP_OK) {
            snprintf(response, sizeof(response),
                "{\"status\":\"success\",\"message\":\"Sensor %d deleted successfully\"}", sensor_id + 1);
            ESP_LOGI(TAG, "Sensor %d deleted, total sensors: %d", sensor_id + 1, g_system_config.sensor_count);
        } else {
            snprintf(response, sizeof(response),
                "{\"status\":\"error\",\"message\":\"Failed to save configuration after deletion\"}");
        }
    } else {
        snprintf(response, sizeof(response),
            "{\"status\":\"error\",\"message\":\"Invalid sensor ID: %d\"}", sensor_id);
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// Edit sensor endpoint
static esp_err_t edit_sensor_handler(httpd_req_t *req)
{
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        const char* error_response = "{\"status\":\"error\",\"message\":\"No data received\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, error_response, strlen(error_response));
        return ESP_OK;
    }
    buf[ret] = '\0';
    
    ESP_LOGI(TAG, "Edit sensor data: %s", buf);
    
    // Parse form data to extract sensor_id first
    int sensor_id = -1;
    char *sensor_id_param = strstr(buf, "sensor_id=");
    if (sensor_id_param) {
        sensor_id = atoi(sensor_id_param + 10);  // Skip "sensor_id="
    }
    
    // Initialize defaults from existing sensor configuration
    char name[64] = "", unit_id[32] = "", data_type[32] = "", register_type[16] = "HOLDING", parity[8] = "none";
    int slave_id = 1, register_address = 0, quantity = 1, baud_rate = 9600;
    float scale_factor = 1.0;
    
    if (sensor_id >= 0 && sensor_id < g_system_config.sensor_count) {
        strncpy(name, g_system_config.sensors[sensor_id].name, sizeof(name) - 1);
        strncpy(unit_id, g_system_config.sensors[sensor_id].unit_id, sizeof(unit_id) - 1);
        strncpy(data_type, g_system_config.sensors[sensor_id].data_type, sizeof(data_type) - 1);
        strncpy(register_type, g_system_config.sensors[sensor_id].register_type[0] ? g_system_config.sensors[sensor_id].register_type : "HOLDING", sizeof(register_type) - 1);
        strncpy(parity, g_system_config.sensors[sensor_id].parity[0] ? g_system_config.sensors[sensor_id].parity : "none", sizeof(parity) - 1);
        slave_id = g_system_config.sensors[sensor_id].slave_id;
        register_address = g_system_config.sensors[sensor_id].register_address;
        quantity = g_system_config.sensors[sensor_id].quantity;
        baud_rate = g_system_config.sensors[sensor_id].baud_rate;
        scale_factor = g_system_config.sensors[sensor_id].scale_factor;
    }
    
    ESP_LOGI(TAG, "Edit sensor %d - Initial values: quantity=%d, data_type=%s, scale_factor=%.2f", 
             sensor_id, quantity, data_type, scale_factor);
    
    // Parse all form parameters
    char *param_start = buf;
    while (param_start) {
        char *param_end = strchr(param_start, '&');
        if (param_end) {
            *param_end = '\0';
        }
        
        char *equals = strchr(param_start, '=');
        if (equals) {
            *equals = '\0';
            char *param_name = param_start;
            char *param_value = equals + 1;
            
            char decoded_value[256];
            url_decode(decoded_value, param_value);
            
            if (strcmp(param_name, "sensor_id") == 0) {
                // Already processed
            } else if (strcmp(param_name, "name") == 0) {
                strncpy(name, decoded_value, sizeof(name) - 1);
            } else if (strcmp(param_name, "unit_id") == 0) {
                strncpy(unit_id, decoded_value, sizeof(unit_id) - 1);
            } else if (strcmp(param_name, "slave_id") == 0) {
                slave_id = atoi(decoded_value);
            } else if (strcmp(param_name, "register_address") == 0) {
                register_address = atoi(decoded_value);
            } else if (strcmp(param_name, "quantity") == 0) {
                ESP_LOGI(TAG, "Parsing quantity: %s -> %d", decoded_value, atoi(decoded_value));
                if (strlen(decoded_value) > 0) {
                    quantity = atoi(decoded_value);
                }
            } else if (strcmp(param_name, "data_type") == 0) {
                ESP_LOGI(TAG, "Parsing data_type: %s", decoded_value);
                if (strlen(decoded_value) > 0) {
                    // Normalize data type variations
                    if (strcmp(decoded_value, "UINT32_DCB") == 0) {
                        strncpy(data_type, "UINT32_4321", sizeof(data_type) - 1);
                        ESP_LOGI(TAG, "Normalized UINT32_DCB to UINT32_4321");
                    } else if (strcmp(decoded_value, "INT32_DCB") == 0) {
                        strncpy(data_type, "INT32_4321", sizeof(data_type) - 1);
                        ESP_LOGI(TAG, "Normalized INT32_DCB to INT32_4321");
                    } else if (strcmp(decoded_value, "FLOAT32_DCB") == 0) {
                        strncpy(data_type, "FLOAT32_4321", sizeof(data_type) - 1);
                        ESP_LOGI(TAG, "Normalized FLOAT32_DCB to FLOAT32_4321");
                    } else if (strcmp(decoded_value, "UINT32") == 0) {
                        strncpy(data_type, "UINT32_1234", sizeof(data_type) - 1);
                        ESP_LOGI(TAG, "Normalized UINT32 to UINT32_1234");
                    } else if (strcmp(decoded_value, "INT32") == 0) {
                        strncpy(data_type, "INT32_1234", sizeof(data_type) - 1);
                        ESP_LOGI(TAG, "Normalized INT32 to INT32_1234");
                    } else if (strcmp(decoded_value, "FLOAT32") == 0) {
                        strncpy(data_type, "FLOAT32_1234", sizeof(data_type) - 1);
                        ESP_LOGI(TAG, "Normalized FLOAT32 to FLOAT32_1234");
                    } else if (strcmp(decoded_value, "UINT16") == 0) {
                        strncpy(data_type, "UINT16_HI", sizeof(data_type) - 1);
                        ESP_LOGI(TAG, "Normalized UINT16 to UINT16_HI");
                    } else if (strcmp(decoded_value, "INT16") == 0) {
                        strncpy(data_type, "INT16_HI", sizeof(data_type) - 1);
                        ESP_LOGI(TAG, "Normalized INT16 to INT16_HI");
                    } else if (strcmp(decoded_value, "FLOAT32_AB") == 0) {
                        strncpy(data_type, "FLOAT32_1234", sizeof(data_type) - 1);
                        ESP_LOGI(TAG, "Normalized FLOAT32_AB to FLOAT32_1234");
                    } else if (strcmp(decoded_value, "UINT32_AB") == 0) {
                        strncpy(data_type, "UINT32_1234", sizeof(data_type) - 1);
                        ESP_LOGI(TAG, "Normalized UINT32_AB to UINT32_1234");
                    } else if (strcmp(decoded_value, "INT32_AB") == 0) {
                        strncpy(data_type, "INT32_1234", sizeof(data_type) - 1);
                        ESP_LOGI(TAG, "Normalized INT32_AB to INT32_1234");
                    } else {
                        strncpy(data_type, decoded_value, sizeof(data_type) - 1);
                    }
                } else {
                    // Set default if empty data_type
                    strncpy(data_type, "UINT16_HI", sizeof(data_type) - 1);
                    ESP_LOGW(TAG, "data_type was empty, using default: UINT16_HI");
                }
            } else if (strcmp(param_name, "baud_rate") == 0) {
                baud_rate = atoi(decoded_value);
            } else if (strcmp(param_name, "scale_factor") == 0) {
                ESP_LOGI(TAG, "Parsing scale_factor: %s -> %.2f", decoded_value, atof(decoded_value));
                if (strlen(decoded_value) > 0) {
                    scale_factor = atof(decoded_value);
                }
            } else if (strcmp(param_name, "parity") == 0) {
                strncpy(parity, decoded_value, sizeof(parity) - 1);
            } else if (strcmp(param_name, "register_type") == 0) {
                strncpy(register_type, decoded_value, sizeof(register_type) - 1);
            } else if (strcmp(param_name, "meter_type") == 0) {
                // Handle meter_type parameter for ENERGY sensors
                if (sensor_id >= 0 && sensor_id < g_system_config.sensor_count) {
                    strncpy(g_system_config.sensors[sensor_id].meter_type, decoded_value, sizeof(g_system_config.sensors[sensor_id].meter_type) - 1);
                    g_system_config.sensors[sensor_id].meter_type[sizeof(g_system_config.sensors[sensor_id].meter_type) - 1] = '\0';
                    ESP_LOGI(TAG, "Updated meter_type for sensor %d: %s", sensor_id, decoded_value);
                }
            } else if (strcmp(param_name, "sensor_type") == 0) {
                if (sensor_id >= 0 && sensor_id < g_system_config.sensor_count) {
                    strncpy(g_system_config.sensors[sensor_id].sensor_type, decoded_value, sizeof(g_system_config.sensors[sensor_id].sensor_type) - 1);
                    g_system_config.sensors[sensor_id].sensor_type[sizeof(g_system_config.sensors[sensor_id].sensor_type) - 1] = '\0';
                }
            } else if (strcmp(param_name, "sensor_height") == 0) {
                if (sensor_id >= 0 && sensor_id < g_system_config.sensor_count) {
                    g_system_config.sensors[sensor_id].sensor_height = atof(decoded_value);
                }
            } else if (strcmp(param_name, "max_water_level") == 0) {
                if (sensor_id >= 0 && sensor_id < g_system_config.sensor_count) {
                    g_system_config.sensors[sensor_id].max_water_level = atof(decoded_value);
                }
            }
        }
        
        if (param_end) {
            param_start = param_end + 1;
        } else {
            break;
        }
    }
    
    ESP_LOGI(TAG, "Edit sensor %d - Final values: quantity=%d, data_type=%s, scale_factor=%.2f", 
             sensor_id, quantity, data_type, scale_factor);
    
    // Final validation: ensure data_type is not empty
    if (strlen(data_type) == 0) {
        strncpy(data_type, "UINT16_HI", sizeof(data_type) - 1);
        ESP_LOGW(TAG, "Final validation: data_type was empty, using default: UINT16_HI");
    }
    
    char response[512];
    
    if (sensor_id >= 0 && sensor_id < g_system_config.sensor_count) {
        // Update sensor configuration
        strncpy(g_system_config.sensors[sensor_id].name, name, sizeof(g_system_config.sensors[sensor_id].name) - 1);
        strncpy(g_system_config.sensors[sensor_id].unit_id, unit_id, sizeof(g_system_config.sensors[sensor_id].unit_id) - 1);
        strncpy(g_system_config.sensors[sensor_id].data_type, data_type, sizeof(g_system_config.sensors[sensor_id].data_type) - 1);
        g_system_config.sensors[sensor_id].data_type[sizeof(g_system_config.sensors[sensor_id].data_type) - 1] = '\0';
        strncpy(g_system_config.sensors[sensor_id].register_type, register_type, sizeof(g_system_config.sensors[sensor_id].register_type) - 1);
        g_system_config.sensors[sensor_id].register_type[sizeof(g_system_config.sensors[sensor_id].register_type) - 1] = '\0';
        ESP_LOGI(TAG, "Stored data_type: '%s' (len=%d)", g_system_config.sensors[sensor_id].data_type, strlen(g_system_config.sensors[sensor_id].data_type));
        g_system_config.sensors[sensor_id].slave_id = slave_id;
        g_system_config.sensors[sensor_id].register_address = register_address;
        g_system_config.sensors[sensor_id].quantity = quantity;
        g_system_config.sensors[sensor_id].baud_rate = baud_rate;
        strncpy(g_system_config.sensors[sensor_id].parity, parity, sizeof(g_system_config.sensors[sensor_id].parity) - 1);
        g_system_config.sensors[sensor_id].scale_factor = scale_factor;
        g_system_config.sensors[sensor_id].enabled = true;
        
        // Save to NVS
        esp_err_t err = config_save_to_nvs(&g_system_config);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Sensor %d saved - Final stored values: quantity=%d, data_type=%s, scale_factor=%.2f", 
                     sensor_id, g_system_config.sensors[sensor_id].quantity, g_system_config.sensors[sensor_id].data_type, g_system_config.sensors[sensor_id].scale_factor);
            snprintf(response, sizeof(response),
                "{\"status\":\"success\",\"message\":\"Sensor %d updated successfully\"}", sensor_id + 1);
            ESP_LOGI(TAG, "Sensor %d updated: %s (Unit: %s, Slave: %d, Reg: %d)", 
                     sensor_id + 1, name, unit_id, slave_id, register_address);
        } else {
            snprintf(response, sizeof(response),
                "{\"status\":\"error\",\"message\":\"Failed to save sensor configuration\"}");
        }
    } else {
        snprintf(response, sizeof(response),
            "{\"status\":\"error\",\"message\":\"Invalid sensor ID: %d\"}", sensor_id);
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// Start operation mode
static esp_err_t start_operation_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[START_OP] User clicked 'Start Operation' button");
    ESP_LOGI(TAG, "[START_OP] Setting config_complete = TRUE");

    g_system_config.config_complete = true;

    ESP_LOGI(TAG, "[START_OP] Saving configuration to NVS...");
    esp_err_t save_result = config_save_to_nvs(&g_system_config);

    if (save_result == ESP_OK) {
        ESP_LOGI(TAG, "[START_OP] ✅ Configuration saved successfully to NVS");
    } else {
        ESP_LOGE(TAG, "[START_OP] ❌ FAILED to save configuration: %s", esp_err_to_name(save_result));
    }

    const char* response = "{\"status\":\"success\",\"message\":\"Switching to operation mode\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));

    ESP_LOGI(TAG, "[START_OP] System will restart in 2 seconds...");
    ESP_LOGI(TAG, "[START_OP] After restart, system should enter OPERATION MODE");

    // Restart in 2 seconds
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

// Reboot system handler
static esp_err_t reboot_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[REBOOT] User clicked 'Reboot to Normal Mode' button");
    ESP_LOGI(TAG, "[REBOOT] Setting config_complete = TRUE");

    // Mark configuration as complete before reboot
    g_system_config.config_complete = true;

    ESP_LOGI(TAG, "[REBOOT] Saving configuration to NVS...");
    esp_err_t save_result = config_save_to_nvs(&g_system_config);

    if (save_result == ESP_OK) {
        ESP_LOGI(TAG, "[REBOOT] ✅ Configuration saved successfully to NVS");
    } else {
        ESP_LOGE(TAG, "[REBOOT] ❌ FAILED to save configuration: %s", esp_err_to_name(save_result));
    }

    const char* response = "{\"status\":\"success\",\"message\":\"System rebooting to operation mode...\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));

    ESP_LOGI(TAG, "[REBOOT] System will restart in 2 seconds...");
    ESP_LOGI(TAG, "[REBOOT] After restart, system should enter OPERATION MODE");

    // Restart in 2 seconds
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

// RS485 Test handler with SCADA-compatible format output
static esp_err_t test_rs485_handler(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // Parse parameters
    int slave_id = 1, register_address = 0, quantity = 1, baud_rate = 9600;
    float scale_factor = 1.0, sensor_height = 0.0, max_water_level = 0.0;
    char data_type[32] = "", sensor_type[16] = "", register_type[16] = "HOLDING";
    
    char* param = strtok(buf, "&");
    while (param != NULL) {
        if (strncmp(param, "slave_id=", 9) == 0) {
            slave_id = atoi(param + 9);
        } else if (strncmp(param, "register_address=", 17) == 0) {
            register_address = atoi(param + 17);
        } else if (strncmp(param, "quantity=", 9) == 0) {
            quantity = atoi(param + 9);
        } else if (strncmp(param, "data_type=", 10) == 0) {
            url_decode(data_type, param + 10);
        } else if (strncmp(param, "baud_rate=", 10) == 0) {
            baud_rate = atoi(param + 10);
        } else if (strncmp(param, "scale_factor=", 13) == 0) {
            scale_factor = atof(param + 13);
        } else if (strncmp(param, "sensor_type=", 12) == 0) {
            url_decode(sensor_type, param + 12);
        } else if (strncmp(param, "sensor_height=", 14) == 0) {
            sensor_height = atof(param + 14);
        } else if (strncmp(param, "max_water_level=", 16) == 0) {
            max_water_level = atof(param + 16);
        } else if (strncmp(param, "register_type=", 13) == 0) {
            url_decode(register_type, param + 13);
        }
        param = strtok(NULL, "&");
    }
    
    ESP_LOGI(TAG, "RS485 Test: Slave=%d, Reg=%d, Qty=%d, RegType=%s, DataType=%s, Baud=%d, Scale=%.6f, SensorType=%s, Height=%.2f, MaxLevel=%.2f", 
             slave_id, register_address, quantity, register_type, data_type, baud_rate, scale_factor, sensor_type, sensor_height, max_water_level);
    
    // Allocate response buffer on heap to prevent stack overflow
    char *response = malloc(1700);
    if (!response) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Validate parameters before Modbus communication
    if (slave_id < 1 || slave_id > 247) {
        snprintf(response, 1500, 
                 "{\"status\":\"error\",\"message\":\"Invalid Slave ID: %d. Must be 1-247 for Modbus RTU.\"}",
                 slave_id);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        free(response);
        return ESP_OK;
    }
    
    if (quantity < 1 || quantity > 4) {
        snprintf(response, 1500,
                 "{\"status\":\"error\",\"message\":\"Invalid Quantity: %d. Must be 1-4 registers for stability.\"}",
                 quantity);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        free(response);
        return ESP_OK;
    }
    
    if (register_address > 65535) {
        snprintf(response, 1500,
                 "{\"status\":\"error\",\"message\":\"Invalid Register Address: %d. Must be 0-65535.\"}",
                 register_address);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        free(response);
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "✓ Parameters validated - Slave:%d, Reg:%d, Qty:%d, RegType:%s, DataType:%s", 
             slave_id, register_address, quantity, register_type, data_type);
    
    // Set the baud rate before performing the test
    ESP_LOGI(TAG, "Setting baud rate to %d bps for RS485 test", baud_rate);
    esp_err_t baud_err = modbus_set_baud_rate(baud_rate);
    if (baud_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set baud rate: %s", esp_err_to_name(baud_err));
    }
    
    // Perform Modbus test based on register type
    modbus_result_t result;
    if (strcmp(register_type, "INPUT") == 0) {
        ESP_LOGI(TAG, "[MODBUS] Reading INPUT registers (function 04)");
        result = modbus_read_input_registers(slave_id, register_address, quantity);
    } else {
        ESP_LOGI(TAG, "[MODBUS] Reading HOLDING registers (function 03) - default for type '%s'", register_type);
        result = modbus_read_holding_registers(slave_id, register_address, quantity);
    }
    
    if (result == MODBUS_SUCCESS) {
        // Get the raw register values
        uint16_t registers[4]; // Limit to 4 registers to prevent overflow
        int reg_count = modbus_get_response_length();
        if (reg_count > 4 || reg_count <= 0) {
            ESP_LOGW(TAG, "Invalid register count: %d, limiting to safe range", reg_count);
            reg_count = (reg_count > 4) ? 4 : 1; // Safety limit
        }
        
        for (int i = 0; i < reg_count && i < 4; i++) {
            registers[i] = modbus_get_response_buffer(i);
        }
        
        // Create comprehensive ScadaCore format interpretation table (heap allocated for large content)
        // Increased buffer size to handle ZEST sensors with 4 registers and extensive format interpretations
        char *format_table = (char*)malloc(10000);  // Increased buffer for 4-register ZEST sensors
        if (format_table == NULL) {
            ESP_LOGE(TAG, "Failed to allocate format_table buffer");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        memset(format_table, 0, 10000);  // Initialize buffer to prevent undefined behavior

        // Build the comprehensive data format table
        snprintf(format_table, 10000,
                 "<div style='background:#d4edda;padding:15px;border-radius:8px;margin:10px 0;border:1px solid #c3e6cb;box-shadow:0 2px 4px rgba(0,0,0,0.1)'>"
                 "<h4 style='color:#155724;margin:0 0 10px 0;font-weight:bold'>✓ RS485 Success - %d Registers Read</h4>", reg_count);
        
        // Add primary configured value first
        double primary_value = 0.0;
        if (reg_count >= 4 && strstr(data_type, "FLOAT64")) {
            // FLOAT64 handling - 4 registers (64-bit double precision)
            uint64_t raw_val64 = 0;
            if (strstr(data_type, "12345678")) {
                // FLOAT64_12345678 (ABCDEFGH) - Standard big endian
                raw_val64 = ((uint64_t)registers[0] << 48) | ((uint64_t)registers[1] << 32) | 
                           ((uint64_t)registers[2] << 16) | registers[3];
            } else if (strstr(data_type, "87654321")) {
                // FLOAT64_87654321 (HGFEDCBA) - Full little endian
                raw_val64 = ((uint64_t)registers[3] << 48) | ((uint64_t)registers[2] << 32) | 
                           ((uint64_t)registers[1] << 16) | registers[0];
            } else {
                // Default to standard big endian if specific order not found
                raw_val64 = ((uint64_t)registers[0] << 48) | ((uint64_t)registers[1] << 32) | 
                           ((uint64_t)registers[2] << 16) | registers[3];
            }
            union { uint64_t i; double d; } conv64;
            conv64.i = raw_val64;
            primary_value = conv64.d * scale_factor;
        } else if (reg_count >= 2 && strstr(data_type, "FLOAT32")) {
            uint32_t raw_val = strstr(data_type, "4321") ? 
                ((uint32_t)registers[1] << 16) | registers[0] :
                ((uint32_t)registers[0] << 16) | registers[1];
            union { uint32_t i; float f; } conv;
            conv.i = raw_val;
            primary_value = (double)conv.f * scale_factor;
        } else if (reg_count >= 2 && strstr(data_type, "UINT32")) {
            primary_value = (double)(strstr(data_type, "4321") ? 
                (((uint32_t)registers[1] << 16) | registers[0]) :
                (((uint32_t)registers[0] << 16) | registers[1])) * scale_factor;
        } else if (reg_count >= 1) {
            primary_value = (double)registers[0] * scale_factor;
        }
        
        char temp_str[1000];
        
        // Calculate the final test value based on sensor type
        double test_display_value = primary_value;
        char test_value_desc[100];
        
        if (strcmp(sensor_type, "Radar Level") == 0 && max_water_level > 0) {
            test_display_value = (primary_value / max_water_level) * 100.0;
            if (test_display_value < 0) test_display_value = 0.0;
            snprintf(test_value_desc, sizeof(test_value_desc), "Radar Level %.2f%%", test_display_value);
        } else if (strcmp(sensor_type, "Level") == 0 && max_water_level > 0) {
            test_display_value = ((sensor_height - primary_value) / max_water_level) * 100.0;
            if (test_display_value < 0) test_display_value = 0.0;
            if (test_display_value > 100) test_display_value = 100.0;
            snprintf(test_value_desc, sizeof(test_value_desc), "Level %.2f%%", test_display_value);
        } else {
            test_display_value = primary_value * scale_factor;
            snprintf(test_value_desc, sizeof(test_value_desc), "%s×%.3f", data_type, scale_factor);
        }
        
        snprintf(temp_str, sizeof(temp_str), 
                 "<b>Test Value:</b> %.6f (%s)<br>"
                 "<b>Raw Hex:</b> ", test_display_value, test_value_desc);
        
        // Add operation team comparison display for Level/Radar Level sensors
        if ((strcmp(sensor_type, "Radar Level") == 0 || strcmp(sensor_type, "Level") == 0) && max_water_level > 0) {
            char comparison_str[200];
            snprintf(comparison_str, sizeof(comparison_str), 
                     "<br><b>Operation Comparison:</b> %.0f → %.1f%% ✅<br>", 
                     primary_value, test_display_value);
            strcat(format_table, temp_str);
            strcat(format_table, comparison_str);
        } else {
            strcat(format_table, temp_str);
        }
        
        for (int i = 0; i < reg_count && i < 4; i++) {
            snprintf(temp_str, sizeof(temp_str), "%04X ", registers[i]);
            strcat(format_table, temp_str);
        }
        strcat(format_table, "<br>");
        
        // Add comprehensive format interpretations table
        strcat(format_table,
               "<div style='overflow-x:auto;margin-top:15px'>"
               "<table style='width:100%;font-size:11px;border-collapse:collapse;min-width:320px'>"
               "<tr style='background:#495057;color:white'><th colspan='4' style='padding:10px;font-weight:bold;text-align:center'>ALL SCADACORE DATA FORMAT INTERPRETATIONS</th></tr>");
        
        // 16-bit formats (if 1+ registers)
        if (reg_count >= 1) {
            uint16_t reg0 = registers[0];
            snprintf(temp_str, sizeof(temp_str),
                     "<tr><td>UINT16_BE:</td><td>%u</td><td>INT16_BE:</td><td>%d</td></tr>"
                     "<tr><td>UINT16_LE:</td><td>%u</td><td>INT16_LE:</td><td>%d</td></tr>",
                     reg0, (int16_t)reg0,
                     ((reg0 & 0xFF) << 8) | (reg0 >> 8), (int16_t)(((reg0 & 0xFF) << 8) | (reg0 >> 8)));
            strcat(format_table, temp_str);
        }
        
        // 32-bit formats (if 2+ registers) - Comprehensive ScadaCore variations
        if (reg_count >= 2) {
            // All 32-bit byte order interpretations for ScadaCore compatibility
            uint32_t val_1234_abcd = ((uint32_t)registers[0] << 16) | registers[1];
            uint32_t val_4321_dcba = ((uint32_t)registers[1] << 16) | registers[0];
            uint32_t val_2143_badc = ((uint32_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 16) | 
                                    (((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF));
            uint32_t val_3412_cdab = ((uint32_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) | 
                                    (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
            
            // 32-bit FLOAT conversions
            union { uint32_t i; float f; } float_conv;
            float_conv.i = val_1234_abcd; float float_1234_abcd = float_conv.f;
            float_conv.i = val_4321_dcba; float float_4321_dcba = float_conv.f;
            float_conv.i = val_2143_badc; float float_2143_badc = float_conv.f;
            float_conv.i = val_3412_cdab; float float_3412_cdab = float_conv.f;
            
            // FLOAT32 comprehensive variations - ScadaCore compatible
            snprintf(temp_str, sizeof(temp_str),
                     "<tr style='background:#0d6efd;color:white'><th colspan='4' style='padding:8px;font-weight:bold'>FLOAT32 FORMAT INTERPRETATIONS</th></tr>"
                     "<tr><td style='padding:6px;word-break:break-word'><strong>FLOAT32_1234 (ABCD):</strong></td><td style='padding:6px'>%.6f</td><td style='padding:6px;word-break:break-word'><strong>FLOAT32_4321 (DCBA):</strong></td><td style='padding:6px'>%.6f</td></tr>"
                     "<tr style='background:#f8f8f8'><td style='padding:6px;word-break:break-word'><strong>FLOAT32_2143 (BADC):</strong></td><td style='padding:6px'>%.6f</td><td style='padding:6px;word-break:break-word'><strong>FLOAT32_3412 (CDAB):</strong></td><td style='padding:6px'>%.6f</td></tr>",
                     float_1234_abcd, float_4321_dcba, float_2143_badc, float_3412_cdab);
            strcat(format_table, temp_str);

            // INT32 comprehensive variations - ScadaCore compatible
            snprintf(temp_str, sizeof(temp_str),
                     "<tr style='background:#28a745;color:white'><th colspan='4' style='padding:8px;font-weight:bold'>INT32 FORMAT INTERPRETATIONS</th></tr>"
                     "<tr><td style='padding:6px;word-break:break-word'><strong>INT32_1234 (ABCD):</strong></td><td style='padding:6px'>%ld</td><td style='padding:6px;word-break:break-word'><strong>INT32_4321 (DCBA):</strong></td><td style='padding:6px'>%ld</td></tr>"
                     "<tr style='background:#f8f8f8'><td style='padding:6px;word-break:break-word'><strong>INT32_2143 (BADC):</strong></td><td style='padding:6px'>%ld</td><td style='padding:6px;word-break:break-word'><strong>INT32_3412 (CDAB):</strong></td><td style='padding:6px'>%ld</td></tr>",
                     (int32_t)val_1234_abcd, (int32_t)val_4321_dcba, (int32_t)val_2143_badc, (int32_t)val_3412_cdab);
            strcat(format_table, temp_str);

            // UINT32 comprehensive variations - ScadaCore compatible
            snprintf(temp_str, sizeof(temp_str),
                     "<tr style='background:#fd7e14;color:white'><th colspan='4' style='padding:8px;font-weight:bold'>UINT32 FORMAT INTERPRETATIONS</th></tr>"
                     "<tr><td style='padding:6px;word-break:break-word'><strong>UINT32_1234 (ABCD):</strong></td><td style='padding:6px'>%lu</td><td style='padding:6px;word-break:break-word'><strong>UINT32_4321 (DCBA):</strong></td><td style='padding:6px'>%lu</td></tr>"
                     "<tr style='background:#f8f8f8'><td style='padding:6px;word-break:break-word'><strong>UINT32_2143 (BADC):</strong></td><td style='padding:6px'>%lu</td><td style='padding:6px;word-break:break-word'><strong>UINT32_3412 (CDAB):</strong></td><td style='padding:6px'>%lu</td></tr>",
                     val_1234_abcd, val_4321_dcba, val_2143_badc, val_3412_cdab);
            strcat(format_table, temp_str);
        }
        
        // 64-bit formats (if 4 registers) - Comprehensive ScadaCore variations
        if (reg_count >= 4) {
            // All 64-bit byte order interpretations for ScadaCore compatibility
            uint64_t val64_12345678 = ((uint64_t)registers[0] << 48) | ((uint64_t)registers[1] << 32) | 
                                     ((uint64_t)registers[2] << 16) | registers[3];
            uint64_t val64_87654321 = ((uint64_t)registers[3] << 48) | ((uint64_t)registers[2] << 32) | 
                                     ((uint64_t)registers[1] << 16) | registers[0];
            uint64_t val64_21436587 = ((uint64_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 48) |
                                     ((uint64_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 32) |
                                     ((uint64_t)(((registers[2] & 0xFF) << 8) | ((registers[2] >> 8) & 0xFF)) << 16) |
                                     (((registers[3] & 0xFF) << 8) | ((registers[3] >> 8) & 0xFF));
            uint64_t val64_78563412 = ((uint64_t)(((registers[3] & 0xFF) << 8) | ((registers[3] >> 8) & 0xFF)) << 48) |
                                     ((uint64_t)(((registers[2] & 0xFF) << 8) | ((registers[2] >> 8) & 0xFF)) << 32) |
                                     ((uint64_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) |
                                     (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
            
            // 64-bit FLOAT conversions
            union { uint64_t i; double d; } float64_conv;
            
            float64_conv.i = val64_12345678; double float64_12345678 = float64_conv.d;
            float64_conv.i = val64_87654321; double float64_87654321 = float64_conv.d;
            float64_conv.i = val64_21436587; double float64_21436587 = float64_conv.d;
            float64_conv.i = val64_78563412; double float64_78563412 = float64_conv.d;
            
            // FLOAT64 comprehensive variations - ScadaCore compatible
            snprintf(temp_str, sizeof(temp_str),
                     "<tr style='background:#6610f2;color:white'><th colspan='4' style='padding:8px;font-weight:bold'>FLOAT64 FORMAT INTERPRETATIONS</th></tr>"
                     "<tr><td style='padding:6px;word-break:break-word'><strong>FLOAT64_12345678 (ABCDEFGH):</strong></td><td style='padding:6px;word-break:break-all'>%.3f</td><td style='padding:6px;word-break:break-word'><strong>FLOAT64_87654321 (HGFEDCBA):</strong></td><td style='padding:6px;word-break:break-all'>%.3f</td></tr>"
                     "<tr style='background:#f8f8f8'><td style='padding:6px;word-break:break-word'><strong>FLOAT64_21436587 (BADCFEHG):</strong></td><td style='padding:6px;word-break:break-all'>%.3f</td><td style='padding:6px;word-break:break-word'><strong>FLOAT64_78563412 (GHEFCDAB):</strong></td><td style='padding:6px;word-break:break-all'>%.3f</td></tr>",
                     float64_12345678, float64_87654321, float64_21436587, float64_78563412);
            strcat(format_table, temp_str);

            // INT64 comprehensive variations - ScadaCore compatible
            snprintf(temp_str, sizeof(temp_str),
                     "<tr style='background:#20c997;color:white'><th colspan='4' style='padding:8px;font-weight:bold'>INT64 FORMAT INTERPRETATIONS</th></tr>"
                     "<tr><td style='padding:6px;word-break:break-word'><strong>INT64_12345678 (ABCDEFGH):</strong></td><td style='padding:6px;word-break:break-all'>%lld</td><td style='padding:6px;word-break:break-word'><strong>INT64_87654321 (HGFEDCBA):</strong></td><td style='padding:6px;word-break:break-all'>%lld</td></tr>"
                     "<tr style='background:#f8f8f8'><td style='padding:6px;word-break:break-word'><strong>INT64_21436587 (BADCFEHG):</strong></td><td style='padding:6px;word-break:break-all'>%lld</td><td style='padding:6px;word-break:break-word'><strong>INT64_78563412 (GHEFCDAB):</strong></td><td style='padding:6px;word-break:break-all'>%lld</td></tr>",
                     (int64_t)val64_12345678, (int64_t)val64_87654321, (int64_t)val64_21436587, (int64_t)val64_78563412);
            strcat(format_table, temp_str);

            // UINT64 comprehensive variations - ScadaCore compatible
            snprintf(temp_str, sizeof(temp_str),
                     "<tr style='background:#dc3545;color:white'><th colspan='4' style='padding:8px;font-weight:bold'>UINT64 FORMAT INTERPRETATIONS</th></tr>"
                     "<tr><td style='padding:6px;word-break:break-word'><strong>UINT64_12345678 (ABCDEFGH):</strong></td><td style='padding:6px;word-break:break-all;max-width:150px'>%llu</td><td style='padding:6px;word-break:break-word'><strong>UINT64_87654321 (HGFEDCBA):</strong></td><td style='padding:6px;word-break:break-all;max-width:150px'>%llu</td></tr>"
                     "<tr style='background:#f8f8f8'><td style='padding:6px;word-break:break-word'><strong>UINT64_21436587 (BADCFEHG):</strong></td><td style='padding:6px;word-break:break-all;max-width:150px'>%llu</td><td style='padding:6px;word-break:break-word'><strong>UINT64_78563412 (GHEFCDAB):</strong></td><td style='padding:6px;word-break:break-all;max-width:150px'>%llu</td></tr>",
                     val64_12345678, val64_87654321, val64_21436587, val64_78563412);
            strcat(format_table, temp_str);
            
            // Additional 64-bit interpretations with mixed byte orders
            uint64_t val64_43218765 = ((uint64_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 48) |
                                     ((uint64_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 32) |
                                     ((uint64_t)(((registers[3] & 0xFF) << 8) | ((registers[3] >> 8) & 0xFF)) << 16) |
                                     (((registers[2] & 0xFF) << 8) | ((registers[2] >> 8) & 0xFF));
            uint64_t val64_56781234 = ((uint64_t)(((registers[2] & 0xFF) << 8) | ((registers[2] >> 8) & 0xFF)) << 48) |
                                     ((uint64_t)(((registers[3] & 0xFF) << 8) | ((registers[3] >> 8) & 0xFF)) << 32) |
                                     ((uint64_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 16) |
                                     (((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF));
            
            float64_conv.i = val64_43218765; double float64_43218765 = float64_conv.d;
            float64_conv.i = val64_56781234; double float64_56781234 = float64_conv.d;
            
            snprintf(temp_str, sizeof(temp_str),
                     "<tr style='background:#f4e8ff'><th colspan='4'>Extended 64-bit Format Interpretations</th></tr>"
                     "<tr><td><strong>FLOAT64_43218765 (DCBAHGFE):</strong></td><td>%.3f</td><td><strong>FLOAT64_56781234 (EFGHABCD):</strong></td><td>%.3f</td></tr>"
                     "<tr style='background:#f8f8f8'><td><strong>INT64_43218765 (DCBAHGFE):</strong></td><td>%lld</td><td><strong>INT64_56781234 (EFGHABCD):</strong></td><td>%lld</td></tr>"
                     "<tr><td><strong>UINT64_43218765 (DCBAHGFE):</strong></td><td>%llu</td><td><strong>UINT64_56781234 (EFGHABCD):</strong></td><td>%llu</td></tr>",
                     float64_43218765, float64_56781234, (int64_t)val64_43218765, (int64_t)val64_56781234,
                     val64_43218765, val64_56781234);
            strcat(format_table, temp_str);
            
            // Raw hex display for reference
            snprintf(temp_str, sizeof(temp_str),
                     "<tr style='background:#f0f0f0'><td><strong>Raw Hex 64-bit:</strong></td><td colspan='3'>0x%04X%04X%04X%04X</td></tr>",
                     registers[0], registers[1], registers[2], registers[3]);
            strcat(format_table, temp_str);
        }
        
        strcat(format_table, "</table></div></div>");
        
        // Send HTML response directly (avoids JSON escaping issues)
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, format_table, strlen(format_table));
        free(response);
        free(format_table);
        return ESP_OK;
        
        // Feed watchdog before intensive processing
        // Skip watchdog reset in HTTP handler (task not registered with watchdog)
        // esp_task_wdt_reset();
        
        // Build comprehensive HTML table response like saved sensors (for <= 3 registers)
        // Increased buffer size to handle ZEST sensor data properly (4 registers)
        char *data_output = malloc(2400);
        char *temp = malloc(500);
        if (!data_output || !temp) {
            ESP_LOGE(TAG, "Failed to allocate data buffers");
            if (data_output) free(data_output);
            if (temp) free(temp);
            free(response);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        memset(data_output, 0, 2400);
        memset(temp, 0, 500);
        
        // Success header with styling like saved sensors
        strcat(data_output, "<div style='background:#e8f5e8;padding:10px;border-radius:5px;margin:5px 0'>");
        strcat(data_output, "<h4 style='color:green;margin:0 0 10px 0'>RS485 Communication Success</h4>");
        
        // Primary value interpretation based on selected data type
        // primary_value already declared above
        char primary_desc[100] = "";
        
        // Interpret based on selected data type
        if (strstr(data_type, "FLOAT32") && reg_count >= 2) {
            uint32_t raw_val;
            if (strstr(data_type, "4321") || strstr(data_type, "DCBA")) {
                raw_val = ((uint32_t)registers[1] << 16) | registers[0];
            } else if (strstr(data_type, "2143") || strstr(data_type, "BADC")) {
                raw_val = ((uint32_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 16) | 
                         (((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF));
            } else if (strstr(data_type, "3412") || strstr(data_type, "CDAB")) {
                raw_val = ((uint32_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) | 
                         (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
            } else {
                raw_val = ((uint32_t)registers[0] << 16) | registers[1]; // 1234/ABCD
            }
            union { uint32_t i; float f; } conv;
            conv.i = raw_val;
            primary_value = (double)conv.f;
            snprintf(primary_desc, sizeof(primary_desc), "FLOAT32 (%s)", data_type);
        } else if (strstr(data_type, "UINT32") && reg_count >= 2) {
            if (strstr(data_type, "4321") || strstr(data_type, "DCBA")) {
                primary_value = (double)(((uint32_t)registers[1] << 16) | registers[0]);
            } else if (strstr(data_type, "2143") || strstr(data_type, "BADC")) {
                primary_value = (double)(((uint32_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 16) | 
                                        (((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)));
            } else if (strstr(data_type, "3412") || strstr(data_type, "CDAB")) {
                primary_value = (double)(((uint32_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) | 
                                        (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)));
            } else {
                primary_value = (double)(((uint32_t)registers[0] << 16) | registers[1]);
            }
            snprintf(primary_desc, sizeof(primary_desc), "UINT32 (%s)", data_type);
        } else if (strstr(data_type, "INT32") && reg_count >= 2) {
            uint32_t raw_val;
            if (strstr(data_type, "4321") || strstr(data_type, "DCBA")) {
                raw_val = ((uint32_t)registers[1] << 16) | registers[0];
            } else if (strstr(data_type, "2143") || strstr(data_type, "BADC")) {
                raw_val = ((uint32_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 16) | 
                         (((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF));
            } else if (strstr(data_type, "3412") || strstr(data_type, "CDAB")) {
                raw_val = ((uint32_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) | 
                         (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
            } else {
                raw_val = ((uint32_t)registers[0] << 16) | registers[1];
            }
            primary_value = (double)(int32_t)raw_val;
            snprintf(primary_desc, sizeof(primary_desc), "INT32 (%s)", data_type);
        } else if (strstr(data_type, "INT64") && reg_count >= 4) {
            int64_t int64_val;
            if (strstr(data_type, "_87654321")) {
                int64_val = ((int64_t)registers[3] << 48) | ((int64_t)registers[2] << 32) | 
                           ((int64_t)registers[1] << 16) | registers[0];
            } else if (strstr(data_type, "_21436587")) {
                int64_val = ((int64_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 48) |
                           ((int64_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 32) |
                           ((int64_t)(((registers[2] & 0xFF) << 8) | ((registers[2] >> 8) & 0xFF)) << 16) |
                           (((registers[3] & 0xFF) << 8) | ((registers[3] >> 8) & 0xFF));
            } else if (strstr(data_type, "_78563412")) {
                int64_val = ((int64_t)(((registers[3] & 0xFF) << 8) | ((registers[3] >> 8) & 0xFF)) << 48) |
                           ((int64_t)(((registers[2] & 0xFF) << 8) | ((registers[2] >> 8) & 0xFF)) << 32) |
                           ((int64_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) |
                           (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
            } else {
                int64_val = ((int64_t)registers[0] << 48) | ((int64_t)registers[1] << 32) | 
                           ((int64_t)registers[2] << 16) | registers[3]; // 12345678
            }
            primary_value = (double)int64_val;
            snprintf(primary_desc, sizeof(primary_desc), "INT64 (%s)", data_type);
        } else if (strstr(data_type, "UINT64") && reg_count >= 4) {
            uint64_t uint64_val;
            if (strstr(data_type, "_87654321")) {
                uint64_val = ((uint64_t)registers[3] << 48) | ((uint64_t)registers[2] << 32) | 
                            ((uint64_t)registers[1] << 16) | registers[0];
            } else if (strstr(data_type, "_21436587")) {
                uint64_val = ((uint64_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 48) |
                            ((uint64_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 32) |
                            ((uint64_t)(((registers[2] & 0xFF) << 8) | ((registers[2] >> 8) & 0xFF)) << 16) |
                            (((registers[3] & 0xFF) << 8) | ((registers[3] >> 8) & 0xFF));
            } else if (strstr(data_type, "_78563412")) {
                uint64_val = ((uint64_t)(((registers[3] & 0xFF) << 8) | ((registers[3] >> 8) & 0xFF)) << 48) |
                            ((uint64_t)(((registers[2] & 0xFF) << 8) | ((registers[2] >> 8) & 0xFF)) << 32) |
                            ((uint64_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) |
                            (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
            } else {
                uint64_val = ((uint64_t)registers[0] << 48) | ((uint64_t)registers[1] << 32) | 
                            ((uint64_t)registers[2] << 16) | registers[3]; // 12345678
            }
            primary_value = (double)uint64_val;
            snprintf(primary_desc, sizeof(primary_desc), "UINT64 (%s)", data_type);
        } else if (strstr(data_type, "FLOAT64") && reg_count >= 4) {
            uint64_t raw_bits;
            if (strstr(data_type, "_87654321")) {
                raw_bits = ((uint64_t)registers[3] << 48) | ((uint64_t)registers[2] << 32) | 
                          ((uint64_t)registers[1] << 16) | registers[0];
            } else if (strstr(data_type, "_21436587")) {
                raw_bits = ((uint64_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 48) |
                          ((uint64_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 32) |
                          ((uint64_t)(((registers[2] & 0xFF) << 8) | ((registers[2] >> 8) & 0xFF)) << 16) |
                          (((registers[3] & 0xFF) << 8) | ((registers[3] >> 8) & 0xFF));
            } else if (strstr(data_type, "_78563412")) {
                raw_bits = ((uint64_t)(((registers[3] & 0xFF) << 8) | ((registers[3] >> 8) & 0xFF)) << 48) |
                          ((uint64_t)(((registers[2] & 0xFF) << 8) | ((registers[2] >> 8) & 0xFF)) << 32) |
                          ((uint64_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) |
                          (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
            } else {
                raw_bits = ((uint64_t)registers[0] << 48) | ((uint64_t)registers[1] << 32) | 
                          ((uint64_t)registers[2] << 16) | registers[3]; // 12345678
            }
            union { uint64_t i; double d; } conv64;
            conv64.i = raw_bits;
            primary_value = conv64.d;
            snprintf(primary_desc, sizeof(primary_desc), "FLOAT64 (%s)", data_type);
        } else if (strstr(data_type, "INT8") && reg_count >= 1) {
            if (strstr(data_type, "High")) {
                primary_value = (double)(int8_t)((registers[0] >> 8) & 0xFF);
            } else {
                primary_value = (double)(int8_t)(registers[0] & 0xFF);
            }
            snprintf(primary_desc, sizeof(primary_desc), "INT8 (%s)", data_type);
        } else if (strstr(data_type, "UINT8") && reg_count >= 1) {
            if (strstr(data_type, "High")) {
                primary_value = (double)((registers[0] >> 8) & 0xFF);
            } else {
                primary_value = (double)(registers[0] & 0xFF);
            }
            snprintf(primary_desc, sizeof(primary_desc), "UINT8 (%s)", data_type);
        } else if (strstr(data_type, "UINT16") && reg_count >= 1) {
            if (strstr(data_type, "LO") || strstr(data_type, "LE")) {
                primary_value = (double)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
            } else {
                primary_value = (double)registers[0];
            }
            snprintf(primary_desc, sizeof(primary_desc), "UINT16 (%s)", data_type);
        } else if (strstr(data_type, "INT16") && reg_count >= 1) {
            if (strstr(data_type, "LO") || strstr(data_type, "LE")) {
                primary_value = (double)(int16_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
            } else {
                primary_value = (double)(int16_t)registers[0];
            }
            snprintf(primary_desc, sizeof(primary_desc), "INT16 (%s)", data_type);
        } else if (strstr(data_type, "ZEST_FIXED") && reg_count >= 4) {
            // ZEST_FIXED: Special format - INT32_BE (first 2 registers) + INT32_LE_SWAP (last 2 registers)
            // First value: INT32 big-endian from registers[0] and registers[1]
            int32_t first_value = (int32_t)(((uint32_t)registers[0] << 16) | registers[1]);
            // Second value: INT32 with byte swap from registers[2] and registers[3]
            int32_t second_value = (int32_t)(((uint32_t)registers[3] << 16) | registers[2]);
            // Use first value as primary for display
            primary_value = (double)first_value;
            snprintf(primary_desc, sizeof(primary_desc), "ZEST_FIXED (INT32_BE: %ld, INT32_SWAP: %ld)", first_value, second_value);
        } else if (strstr(data_type, "ASCII") && reg_count >= 1) {
            // ASCII - show first character's ASCII value
            primary_value = (double)((registers[0] >> 8) & 0xFF);
            snprintf(primary_desc, sizeof(primary_desc), "ASCII (%s)", data_type);
        } else if (strstr(data_type, "HEX") && reg_count >= 1) {
            // HEX - show raw hex value
            primary_value = (double)registers[0];
            snprintf(primary_desc, sizeof(primary_desc), "HEX (%s)", data_type);
        } else if (strstr(data_type, "BOOL") && reg_count >= 1) {
            // Boolean - show 0 or 1
            primary_value = registers[0] ? 1.0 : 0.0;
            snprintf(primary_desc, sizeof(primary_desc), "BOOL (%s)", data_type);
        } else if (strstr(data_type, "PDU") && reg_count >= 1) {
            // PDU - show raw value
            primary_value = (double)registers[0];
            snprintf(primary_desc, sizeof(primary_desc), "PDU (%s)", data_type);
        } else {
            // Default to UINT16 for any other type
            primary_value = (double)registers[0];
            snprintf(primary_desc, sizeof(primary_desc), "UINT16 (default)");
        }
        
        double scaled_value;
        char scale_desc[50];
        
        // Apply sensor type-specific calculation
        if (strcmp(sensor_type, "Level") == 0 && max_water_level > 0) {
            // Level sensor calculation: (Sensor Height - Raw Value) / Maximum Water Level * 100
            double raw_scaled_value = primary_value * scale_factor;
            scaled_value = ((sensor_height - raw_scaled_value) / max_water_level) * 100.0;
            // Ensure percentage is within reasonable bounds
            if (scaled_value < 0) scaled_value = 0.0;
            if (scaled_value > 100) scaled_value = 100.0;
            snprintf(scale_desc, sizeof(scale_desc), "Level: %.2f%%", scaled_value);
        } else if (strcmp(sensor_type, "Radar Level") == 0 && max_water_level > 0) {
            // Radar Level sensor calculation: (Primary Value / Maximum Water Level) * 100
            scaled_value = (primary_value / max_water_level) * 100.0;
            // Ensure percentage is not negative (but allow over 100% to show overflow)
            if (scaled_value < 0) scaled_value = 0.0;
            snprintf(scale_desc, sizeof(scale_desc), "Radar Level: %.2f%%", scaled_value);
        } else {
            // Flow-Meter or other sensor types use direct scale factor
            scaled_value = primary_value * scale_factor;
            snprintf(scale_desc, sizeof(scale_desc), "×%.3f", scale_factor);
        }
        
        snprintf(temp, 500, "<strong>Primary Value:</strong> %.6f (%s) | <strong>Scaled Value:</strong> %.6f (%s)<br>",
                 primary_value, primary_desc, scaled_value, scale_desc);
        strcat(data_output, temp);

        // Add Level Filled display for Level and Radar Level sensors
        if (strcmp(sensor_type, "Level") == 0 && max_water_level > 0) {
            snprintf(temp, 500, "<strong>Level Filled:</strong> %.2f%% (Calculated using formula: (%.2f - %.6f) / %.2f * 100)<br>",
                     scaled_value, sensor_height, primary_value, max_water_level);
            strcat(data_output, temp);
        } else if (strcmp(sensor_type, "Radar Level") == 0 && max_water_level > 0) {
            snprintf(temp, 500, "<strong>Level Filled:</strong> %.2f%% (Calculated using formula: %.6f / %.2f * 100)<br>",
                     scaled_value, primary_value, max_water_level);
            strcat(data_output, temp);
        }

        // Raw registers
        strcat(data_output, "<strong>Raw Registers:</strong> [");
        for (int i = 0; i < reg_count; i++) {
            snprintf(temp, 500, "%s%d", i > 0 ? ", " : "", registers[i]);
            strcat(data_output, temp);
        }
        strcat(data_output, "]<br>");

        // Hex string
        strcat(data_output, "<strong>HexString:</strong> ");
        for (int i = 0; i < reg_count; i++) {
            snprintf(temp, 500, "%04X", registers[i]);
            strcat(data_output, temp);
        }
        strcat(data_output, "<br>");
        
        // HTML table for comprehensive data format interpretations
        strcat(data_output, "<table class='format-table' style='width:100%;border-collapse:collapse;font-size:10px;font-family:monospace'>");
        strcat(data_output, "<tr style='background:#ddd'><th colspan='4'>All Available Data Format Interpretations (Industrial Grade)</th></tr>");
        
        // 8-bit interpretations (0.5 register)
        if (reg_count >= 1) {
            uint8_t high_byte = (registers[0] >> 8) & 0xFF;
            uint8_t low_byte = registers[0] & 0xFF;
            snprintf(temp, 500,
                     "<tr><td><strong>INT8 (High):</strong> %d</td><td><strong>INT8 (Low):</strong> %d</td><td><strong>UINT8 (High):</strong> %u</td><td><strong>UINT8 (Low):</strong> %u</td></tr>",
                     (int8_t)high_byte, (int8_t)low_byte, high_byte, low_byte);
            strcat(data_output, temp);
            vTaskDelay(pdMS_TO_TICKS(1)); // Yield after section
        }

        // 16-bit interpretations (1 register)
        if (reg_count >= 1) {
            uint16_t reg0 = registers[0];
            uint16_t reg0_le = ((reg0 & 0xFF) << 8) | ((reg0 >> 8) & 0xFF);
            snprintf(temp, 500,
                     "<tr style='background:#f8f8f8'><td><strong>INT16_HI:</strong> %d</td><td><strong>INT16_LO:</strong> %d</td><td><strong>UINT16_HI:</strong> %u</td><td><strong>UINT16_LO:</strong> %u</td></tr>",
                     (int16_t)reg0, (int16_t)reg0_le, reg0, reg0_le);
            strcat(data_output, temp);
            vTaskDelay(pdMS_TO_TICKS(1)); // Yield after section
        }

        // 32-bit interpretations (2 registers)
        if (reg_count >= 2) {
            uint32_t val_1234 = ((uint32_t)registers[0] << 16) | registers[1];
            uint32_t val_4321 = ((uint32_t)registers[1] << 16) | registers[0];
            uint32_t val_2143 = ((uint32_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 16) |
                               (((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF));
            uint32_t val_3412 = ((uint32_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) |
                               (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));

            // 32-bit Float interpretations
            union { uint32_t i; float f; } conv_1234, conv_4321, conv_2143, conv_3412;
            conv_1234.i = val_1234; conv_4321.i = val_4321; conv_2143.i = val_2143; conv_3412.i = val_3412;

            snprintf(temp, 500,
                     "<tr><td><strong>FLOAT32_1234:</strong> %.3e</td><td><strong>FLOAT32_4321:</strong> %.3e</td><td><strong>FLOAT32_2143:</strong> %.3e</td><td><strong>FLOAT32_3412:</strong> %.3e</td></tr>",
                     conv_1234.f, conv_4321.f, conv_2143.f, conv_3412.f);
            strcat(data_output, temp);
            
            // 32-bit Integer interpretations
            snprintf(temp, 500,
                     "<tr style='background:#f8f8f8'><td><strong>INT32_1234:</strong> %ld</td><td><strong>INT32_3412:</strong> %ld</td><td><strong>INT32_2143:</strong> %ld</td><td><strong>INT32_4321:</strong> %ld</td></tr>",
                     (int32_t)val_1234, (int32_t)val_4321, (int32_t)val_2143, (int32_t)val_3412);
            strcat(data_output, temp);

            // 32-bit Unsigned Integer interpretations
            snprintf(temp, 500,
                     "<tr><td><strong>UINT32_1234:</strong> %lu</td><td><strong>UINT32_4321:</strong> %lu</td><td><strong>UINT32_2143:</strong> %lu</td><td><strong>UINT32_3412:</strong> %lu</td></tr>",
                     val_1234, val_4321, val_2143, val_3412);
            strcat(data_output, temp);
        }
        
        // 64-bit interpretations (4 registers)
        if (reg_count >= 4) {
            uint64_t val64_12345678 = ((uint64_t)registers[0] << 48) | ((uint64_t)registers[1] << 32) | 
                                     ((uint64_t)registers[2] << 16) | registers[3];
            uint64_t val64_87654321 = ((uint64_t)registers[3] << 48) | ((uint64_t)registers[2] << 32) | 
                                     ((uint64_t)registers[1] << 16) | registers[0];
            
            snprintf(temp, 300, 
                     "<tr style='background:#f0f0f0'><td><strong>INT64_12345678:</strong> %lld</td><td><strong>INT64_87654321:</strong> %lld</td><td><strong>UINT64_12345678:</strong> %llu</td><td><strong>UINT64_87654321:</strong> %llu</td></tr>",
                     (int64_t)val64_12345678, (int64_t)val64_87654321, val64_12345678, val64_87654321);
            strcat(data_output, temp);
            
            // 64-bit Float interpretations
            union { uint64_t i; double d; } conv64_12345678, conv64_87654321;
            conv64_12345678.i = val64_12345678; conv64_87654321.i = val64_87654321;
            
            snprintf(temp, 500,
                     "<tr><td><strong>FLOAT64_12345678:</strong> %.3e</td><td><strong>FLOAT64_87654321:</strong> %.3e</td><td><strong>ASCII (4 chars):</strong> %c%c%c%c</td><td><strong>HEX:</strong> 0x%04X%04X%04X%04X</td></tr>",
                     conv64_12345678.d, conv64_87654321.d,
                     (char)(registers[0] >> 8), (char)(registers[0] & 0xFF), (char)(registers[1] >> 8), (char)(registers[1] & 0xFF),
                     registers[0], registers[1], registers[2], registers[3]);
            strcat(data_output, temp);
        } else if (reg_count >= 2) {
            // ASCII and HEX for 2 registers
            snprintf(temp, 500,
                     "<tr style='background:#f0f0f0'><td><strong>ASCII (2 chars):</strong> %c%c</td><td><strong>HEX:</strong> 0x%04X%04X</td><td><strong>BOOL (R0):</strong> %s</td><td><strong>BOOL (R1):</strong> %s</td></tr>",
                     (char)(registers[0] >> 8), (char)(registers[0] & 0xFF),
                     registers[0], registers[1],
                     registers[0] ? "True" : "False", registers[1] ? "True" : "False");
            strcat(data_output, temp);
        } else if (reg_count >= 1) {
            // Single register special formats
            snprintf(temp, 500,
                     "<tr style='background:#f0f0f0'><td><strong>ASCII (1 char):</strong> %c</td><td><strong>HEX:</strong> 0x%04X</td><td><strong>BOOL:</strong> %s</td><td><strong>PDU:</strong> Raw</td></tr>",
                     (char)(registers[0] >> 8), registers[0],
                     registers[0] ? "True" : "False");
            strcat(data_output, temp);
        }

        // Close table and div
        strcat(data_output, "</table></div>");

        // Feed watchdog before largest memory allocation and JSON escaping
        // Skip watchdog reset in HTTP handler (task not registered with watchdog)
        // esp_task_wdt_reset();

        // Build final JSON response - properly escape HTML for JSON
        // Increased buffer size for ZEST sensor handling
        char *escaped_output = malloc(4000);
        if (!escaped_output) {
            ESP_LOGE(TAG, "Failed to allocate escape buffer");
            free(data_output);
            free(temp);
            free(response);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        // Enhanced JSON escaping for HTML content
        int j = 0;
        for (int i = 0; data_output[i] && j < 3980; i++) {
            // Yield frequently during intensive JSON escaping to prevent watchdog timeout
            if (i % 100 == 0) {
                vTaskDelay(pdMS_TO_TICKS(1)); // Brief yield to prevent blocking
            }
            
            unsigned char c = (unsigned char)data_output[i];
            if (c == '"') {
                escaped_output[j++] = '\\';
                escaped_output[j++] = '"';
            } else if (c == '\\') {
                escaped_output[j++] = '\\';
                escaped_output[j++] = '\\';
            } else if (c == '\n') {
                escaped_output[j++] = '\\';
                escaped_output[j++] = 'n';
            } else if (c == '\r') {
                escaped_output[j++] = '\\';
                escaped_output[j++] = 'r';
            } else if (c == '\t') {
                escaped_output[j++] = '\\';
                escaped_output[j++] = 't';
            } else if (c == '\b') {
                escaped_output[j++] = '\\';
                escaped_output[j++] = 'b';
            } else if (c == '\f') {
                escaped_output[j++] = '\\';
                escaped_output[j++] = 'f';
            } else if (c < 32 || c > 126) {
                // Escape non-printable characters
                escaped_output[j++] = '\\';
                escaped_output[j++] = 'u';
                escaped_output[j++] = '0';
                escaped_output[j++] = '0';
                escaped_output[j++] = (c >> 4) < 10 ? '0' + (c >> 4) : 'A' + (c >> 4) - 10;
                escaped_output[j++] = (c & 15) < 10 ? '0' + (c & 15) : 'A' + (c & 15) - 10;
            } else {
                escaped_output[j++] = c;
            }
        }
        escaped_output[j] = '\0';
        
        snprintf(response, 2048,
                 "{\"status\":\"success\",\"message\":\"RS485 communication successful\","
                 "\"scada_formats\":\"%s\"}", escaped_output);
        
        free(escaped_output);
        
        // Clean up allocated buffers
        free(data_output);
        free(temp);
    } else {
        snprintf(response, 1500,
                 "{\"status\":\"error\",\"message\":\"RS485 communication failed. Error code: %d. "
                 "Check wiring: GPIO16(RX), GPIO17(TX), GPIO4(RTS)\"}",
                 result);
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    // Clean up allocated memory
    free(response);
    return ESP_OK;
}

// Water Quality Sensor Test handler
static esp_err_t test_water_quality_sensor_handler(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        const char* error_response = "{\"status\":\"error\",\"message\":\"No data received\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, error_response, strlen(error_response));
        return ESP_OK;
    }
    buf[ret] = '\0';
    
    // Parse parameters
    int slave_id = 1, register_address = 0, quantity = 1;
    char data_type[32] = "UINT16_HI";
    
    char* param = strtok(buf, "&");
    while (param != NULL) {
        if (strncmp(param, "slave_id=", 9) == 0) {
            slave_id = atoi(param + 9);
        } else if (strncmp(param, "register_address=", 17) == 0) {
            register_address = atoi(param + 17);
        } else if (strncmp(param, "quantity=", 9) == 0) {
            quantity = atoi(param + 9);
        } else if (strncmp(param, "data_type=", 10) == 0) {
            strncpy(data_type, param + 10, sizeof(data_type) - 1);
            data_type[sizeof(data_type) - 1] = '\0';
        }
        param = strtok(NULL, "&");
    }
    
    ESP_LOGI(TAG, "Testing water quality sensor - Slave: %d, Register: %d, Quantity: %d, Type: %s", 
             slave_id, register_address, quantity, data_type);
    
    // Test RS485 communication
    modbus_result_t result = modbus_read_holding_registers(slave_id, register_address, quantity);
    
    char response[512];
    httpd_resp_set_type(req, "application/json");
    
    if (result == MODBUS_SUCCESS) {
        // Get the raw register values
        uint16_t registers[10];
        int reg_count = modbus_get_response_length();
        if (reg_count > 10) reg_count = 10; // Safety limit
        
        for (int i = 0; i < reg_count; i++) {
            registers[i] = modbus_get_response_buffer(i);
        }
        // Process the data based on data type
        float processed_value = 0.0;
        char unit[16] = "";
        char raw_data[64] = "";
        
        
        // Build raw data string
        snprintf(raw_data, sizeof(raw_data), "[");
        for (int i = 0; i < reg_count; i++) {
            char temp[16];
            snprintf(temp, sizeof(temp), "%s%d", i > 0 ? "," : "", registers[i]);
            strcat(raw_data, temp);
        }
        strcat(raw_data, "]");
        
        // Process based on data type
        if (strcmp(data_type, "UINT16_BE") == 0 || strcmp(data_type, "UINT16_LE") == 0) {
            processed_value = (float)registers[0];
            if (strcmp(data_type, "UINT16_LE") == 0) {
                processed_value = (float)((registers[0] << 8) | (registers[0] >> 8));
            }
        } else if (strcmp(data_type, "INT16_BE") == 0 || strcmp(data_type, "INT16_LE") == 0) {
            processed_value = (float)(int16_t)registers[0];
            if (strcmp(data_type, "INT16_LE") == 0) {
                processed_value = (float)(int16_t)((registers[0] << 8) | (registers[0] >> 8));
            }
        } else if (reg_count >= 2 && (strcmp(data_type, "UINT32_BE") == 0 || strcmp(data_type, "UINT32_LE") == 0)) {
            if (strcmp(data_type, "UINT32_BE") == 0) {
                processed_value = (float)((uint32_t)registers[0] << 16 | registers[1]);
            } else {
                processed_value = (float)((uint32_t)registers[1] << 16 | registers[0]);
            }
        } else if (reg_count >= 2 && (strcmp(data_type, "FLOAT32_BE") == 0 || strcmp(data_type, "FLOAT32_LE") == 0)) {
            uint32_t combined;
            if (strcmp(data_type, "FLOAT32_BE") == 0) {
                combined = ((uint32_t)registers[0] << 16) | registers[1];
            } else {
                combined = ((uint32_t)registers[1] << 16) | registers[0];
            }
            processed_value = *(float*)&combined;
        } else {
            processed_value = (float)registers[0];
        }
        
        // Determine unit based on typical water quality ranges
        if (processed_value >= 0 && processed_value <= 14) {
            strcpy(unit, "pH");
        } else if (processed_value > 14 && processed_value <= 2000) {
            strcpy(unit, "ppm");
        } else if (processed_value > 2000 && processed_value <= 50000) {
            strcpy(unit, "µS/cm");
        } else if (processed_value >= -10 && processed_value <= 100) {
            strcpy(unit, "degC");
        } else if (processed_value >= 0 && processed_value <= 1000) {
            strcpy(unit, "NTU");
        } else if (processed_value >= 0 && processed_value <= 20) {
            strcpy(unit, "mg/L");
        } else {
            strcpy(unit, "raw");
        }
        
        snprintf(response, sizeof(response),
            "{\"status\":\"success\",\"value\":%.2f,\"unit\": \"%s\",\"raw_data\":\"%s\",\"slave_id\":%d,\"register\":%d,\"data_type\":\"%s\"}",
            processed_value, unit, raw_data, slave_id, register_address, data_type);
            
        ESP_LOGI(TAG, "Water quality sensor test SUCCESS - Value: %.2f %s", processed_value, unit);
    } else {
        snprintf(response, sizeof(response),
            "{\"status\":\"error\",\"message\":\"RS485 communication failed\",\"error_code\":%d,\"slave_id\":%d,\"register\":%d}",
            result, slave_id, register_address);
            
        ESP_LOGE(TAG, "Water quality sensor test FAILED - Slave: %d, Register: %d, Error: %d", 
                 slave_id, register_address, result);
    }
    
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// Save Water Quality Sensor handler
static esp_err_t save_water_quality_sensor_handler(httpd_req_t *req)
{
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        const char* error_response = "{\"status\":\"error\",\"message\":\"No data received\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, error_response, strlen(error_response));
        return ESP_OK;
    }
    buf[ret] = '\0';
    
    // Parse parameters
    char sensor_name[64] = "";
    char unit_id[32] = "";
    char data_type[32] = "UINT16_HI";
    char sensor_type[16] = "QUALITY";
    char parity[8] = "none";
    int slave_id = 1, register_address = 0, quantity = 1, baud_rate = 9600;
    float scale_factor = 1.0;
    
    char* param = strtok(buf, "&");
    while (param != NULL) {
        if (strncmp(param, "sensor_name=", 12) == 0) {
            // URL decode the sensor name
            char *src = param + 12;
            char *dst = sensor_name;
            while (*src && (dst - sensor_name) < sizeof(sensor_name) - 1) {
                if (*src == '%' && src[1] && src[2]) {
                    // Simple URL decoding for common characters
                    if (strncmp(src, "%20", 3) == 0) { *dst++ = ' '; src += 3; }
                    else if (strncmp(src, "%5F", 3) == 0) { *dst++ = '_'; src += 3; }
                    else { *dst++ = *src++; }
                } else {
                    *dst++ = *src++;
                }
            }
            *dst = '\0';
        } else if (strncmp(param, "unit_id=", 8) == 0) {
            // URL decode the unit_id
            char *src = param + 8;
            char *dst = unit_id;
            while (*src && (dst - unit_id) < sizeof(unit_id) - 1) {
                if (*src == '%' && src[1] && src[2]) {
                    if (strncmp(src, "%5F", 3) == 0) { *dst++ = '_'; src += 3; }
                    else { *dst++ = *src++; }
                } else {
                    *dst++ = *src++;
                }
            }
            *dst = '\0';
        } else if (strncmp(param, "slave_id=", 9) == 0) {
            slave_id = atoi(param + 9);
        } else if (strncmp(param, "register_address=", 17) == 0) {
            register_address = atoi(param + 17);
        } else if (strncmp(param, "quantity=", 9) == 0) {
            quantity = atoi(param + 9);
        } else if (strncmp(param, "data_type=", 10) == 0) {
            strncpy(data_type, param + 10, sizeof(data_type) - 1);
            data_type[sizeof(data_type) - 1] = '\0';
        } else if (strncmp(param, "sensor_type=", 12) == 0) {
            strncpy(sensor_type, param + 12, sizeof(sensor_type) - 1);
            sensor_type[sizeof(sensor_type) - 1] = '\0';
        } else if (strncmp(param, "baud_rate=", 10) == 0) {
            baud_rate = atoi(param + 10);
        } else if (strncmp(param, "parity=", 7) == 0) {
            strncpy(parity, param + 7, sizeof(parity) - 1);
            parity[sizeof(parity) - 1] = '\0';
        } else if (strncmp(param, "scale_factor=", 13) == 0) {
            scale_factor = atof(param + 13);
        }
        param = strtok(NULL, "&");
    }
    
    ESP_LOGI(TAG, "Saving water quality sensor: %s (Unit: %s, Slave: %d, Reg: %d, Type: %s)", 
             sensor_name, unit_id, slave_id, register_address, data_type);
    
    // Find the next available sensor slot
    int sensor_index = -1;
    for (int i = 0; i < 8; i++) {
        if (!g_system_config.sensors[i].enabled) {
            sensor_index = i;
            break;
        }
    }
    
    char response[512];
    httpd_resp_set_type(req, "application/json");
    
    if (sensor_index == -1) {
        snprintf(response, sizeof(response),
            "{\"status\":\"error\",\"message\":\"Maximum sensor limit reached. Please delete unused sensors first.\"}");
        ESP_LOGI(TAG, "Cannot save water quality sensor - maximum sensor limit reached");
    } else {
        // Save the sensor configuration
        strncpy(g_system_config.sensors[sensor_index].name, sensor_name, sizeof(g_system_config.sensors[sensor_index].name) - 1);
        strncpy(g_system_config.sensors[sensor_index].unit_id, unit_id, sizeof(g_system_config.sensors[sensor_index].unit_id) - 1);
        strncpy(g_system_config.sensors[sensor_index].data_type, data_type, sizeof(g_system_config.sensors[sensor_index].data_type) - 1);
        strncpy(g_system_config.sensors[sensor_index].sensor_type, sensor_type, sizeof(g_system_config.sensors[sensor_index].sensor_type) - 1);
        strncpy(g_system_config.sensors[sensor_index].parity, parity, sizeof(g_system_config.sensors[sensor_index].parity) - 1);
        strncpy(g_system_config.sensors[sensor_index].register_type, "HOLDING", sizeof(g_system_config.sensors[sensor_index].register_type) - 1);
        g_system_config.sensors[sensor_index].slave_id = slave_id;
        g_system_config.sensors[sensor_index].register_address = register_address;
        g_system_config.sensors[sensor_index].quantity = quantity;
        g_system_config.sensors[sensor_index].baud_rate = baud_rate;
        g_system_config.sensors[sensor_index].scale_factor = scale_factor;
        g_system_config.sensors[sensor_index].enabled = true;
        
        // Update sensor count if this is the highest index
        if (sensor_index >= g_system_config.sensor_count) {
            g_system_config.sensor_count = sensor_index + 1;
        }
        
        // Save to NVS
        esp_err_t err = config_save_to_nvs(&g_system_config);
        if (err == ESP_OK) {
            snprintf(response, sizeof(response),
                "{\"status\":\"success\",\"message\":\"Water quality sensor saved successfully\",\"sensor_name\":\"%s\",\"unit_id\":\"%s\",\"sensor_index\":%d}",
                sensor_name, unit_id, sensor_index);
            ESP_LOGI(TAG, "Water quality sensor saved successfully: %s (Index: %d)", sensor_name, sensor_index);
        } else {
            snprintf(response, sizeof(response),
                "{\"status\":\"error\",\"message\":\"Failed to save sensor configuration to storage\"}");
            ESP_LOGE(TAG, "Failed to save water quality sensor to NVS: %s", esp_err_to_name(err));
        }
    }
    
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}


// WiFi reconnection state
static int s_wifi_retry_count = 0;
static const int MAX_WIFI_RETRY = 5;

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        s_wifi_retry_count = 0;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_wifi_retry_count < MAX_WIFI_RETRY) {
            ESP_LOGI(TAG, "WiFi disconnected, retry %d/%d", s_wifi_retry_count + 1, MAX_WIFI_RETRY);
            // IMPORTANT: Never use vTaskDelay() in event handlers - it blocks the WiFi task!
            // The event loop will handle reconnection timing automatically
            esp_wifi_connect();
            s_wifi_retry_count++;
        } else {
            ESP_LOGW(TAG, "WiFi connection failed after %d attempts", MAX_WIFI_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_retry_count = 0;  // Reset retry counter on successful connection

        // Set DNS servers explicitly (in case DHCP didn't provide them)
        // This is critical for Azure IoT Hub connectivity
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif) {
            esp_netif_dns_info_t dns_info;

            // Set primary DNS (Google DNS)
            dns_info.ip.u_addr.ip4.addr = ESP_IP4TOADDR(8, 8, 8, 8);
            dns_info.ip.type = ESP_IPADDR_TYPE_V4;
            esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);

            // Set backup DNS (Google DNS)
            dns_info.ip.u_addr.ip4.addr = ESP_IP4TOADDR(8, 8, 4, 4);
            esp_netif_set_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info);

            ESP_LOGI(TAG, "DNS servers configured: 8.8.8.8 (primary), 8.8.4.4 (backup)");
        }
    }
}

// System status API handler
static esp_err_t system_status_handler(httpd_req_t *req)
{
    char response[4096];
    time_t now = time(NULL);
    struct tm timeinfo;
    char timestamp[64];
    
    gmtime_r(&now, &timeinfo);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    
    // Get system information
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t min_free_heap = esp_get_minimum_free_heap_size();
    uint32_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    
    // Get uptime in seconds
    uint64_t uptime_us = esp_timer_get_time();
    uint32_t uptime_seconds = (uint32_t)(uptime_us / 1000000);
    uint32_t hours = uptime_seconds / 3600;
    uint32_t minutes = (uptime_seconds % 3600) / 60;
    uint32_t seconds = uptime_seconds % 60;
    
    // Get WiFi status
    wifi_ap_record_t ap_info;
    esp_err_t wifi_err = esp_wifi_sta_get_ap_info(&ap_info);
    const char* wifi_status = (wifi_err == ESP_OK) ? "connected" : "disconnected";
    int32_t rssi = (wifi_err == ESP_OK) ? ap_info.rssi : 0;
    
    // Get task information
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    
    // Calculate heap usage percentage
    float heap_usage_percent = ((float)(total_heap - free_heap) / total_heap) * 100.0;
    
    // Get flash memory information
    uint32_t flash_total;
    esp_flash_get_size(NULL, &flash_total);
    
    // Get current core ID and CPU information
    int core_id = xPortGetCoreID();
    
    // Get MAC address
    uint8_t mac[6];
    char mac_str[18];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // Get detailed RAM information
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);
    size_t internal_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t spiram_heap = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    
    // Get partition information
    const esp_partition_t* app_partition = esp_ota_get_running_partition();
    const esp_partition_t* nvs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
    
    size_t app_partition_size = app_partition ? app_partition->size : 0;
    size_t nvs_partition_size = nvs_partition ? nvs_partition->size : 0;
    
    // Calculate partition usage (approximate)
    size_t app_used = app_partition_size / 2; // Rough estimate
    size_t nvs_used = nvs_partition_size / 4; // Rough estimate
    
    // Build JSON response
    snprintf(response, sizeof(response),
        "{"
        "\"timestamp\":\"%s\","
        "\"system\":{"
            "\"uptime_seconds\":%lu,"
            "\"uptime_formatted\":\"%02lu:%02lu:%02lu\","
            "\"mac_address\":\"%s\","
            "\"flash_total\":%lu,"
            "\"core_id\":%d,"
            "\"firmware_version\":\"1.1.0-final\""
        "},"
        "\"memory\":{"
            "\"free_heap\":%lu,"
            "\"min_free_heap\":%lu,"
            "\"total_heap\":%lu,"
            "\"heap_usage_percent\":%.1f,"
            "\"internal_heap\":%lu,"
            "\"spiram_heap\":%lu,"
            "\"largest_free_block\":%lu,"
            "\"total_allocated\":%lu"
        "},"
        "\"partitions\":{"
            "\"app_partition_size\":%lu,"
            "\"app_partition_used\":%lu,"
            "\"app_usage_percent\":%.1f,"
            "\"nvs_partition_size\":%lu,"
            "\"nvs_partition_used\":%lu,"
            "\"nvs_usage_percent\":%.1f"
        "},"
        "\"wifi\":{"
            "\"status\":\"%s\","
            "\"rssi\":%ld,"
            "\"ssid\":\"%s\""
        "},"
        "\"sensors\":{"
            "\"count\":%d,"
            "\"configured\":%s"
        "},"
        "\"tasks\":{"
            "\"count\":%u"
        "}"
        "}",
        timestamp,
        (unsigned long)uptime_seconds,
        (unsigned long)hours, (unsigned long)minutes, (unsigned long)seconds,
        mac_str,
        (unsigned long)flash_total,
        core_id,
        (unsigned long)free_heap,
        (unsigned long)min_free_heap,
        (unsigned long)total_heap,
        heap_usage_percent,
        (unsigned long)internal_heap,
        (unsigned long)spiram_heap,
        (unsigned long)heap_info.largest_free_block,
        (unsigned long)heap_info.total_allocated_bytes,
        (unsigned long)app_partition_size,
        (unsigned long)app_used,
        app_partition_size > 0 ? ((float)app_used / app_partition_size) * 100.0 : 0.0,
        (unsigned long)nvs_partition_size,
        (unsigned long)nvs_used,
        nvs_partition_size > 0 ? ((float)nvs_used / nvs_partition_size) * 100.0 : 0.0,
        wifi_status,
        (long)rssi,
        (wifi_err == ESP_OK) ? (char*)ap_info.ssid : "N/A",
        g_system_config.sensor_count,
        (g_system_config.sensor_count > 0) ? "true" : "false",
        task_count
    );
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// Write Single Register Handler
static esp_err_t write_single_register_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Write single register request received");
    
    // Read the POST data
    size_t recv_size = MIN(req->content_len, 512);
    char content[513] = {0};
    
    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Received write single register data: %s", content);
    
    // Parse parameters
    uint8_t slave_id = 1;
    uint16_t register_addr = 0;
    uint16_t value = 0;
    char response[256];
    
    // Parse slave_id
    char *param = strstr(content, "slave_id=");
    if (param) {
        slave_id = (uint8_t)atoi(param + 9);
    }
    
    // Parse register_addr
    param = strstr(content, "register_addr=");
    if (param) {
        register_addr = (uint16_t)atoi(param + 14);
    }
    
    // Parse value
    param = strstr(content, "value=");
    if (param) {
        value = (uint16_t)atoi(param + 6);
    }
    
    ESP_LOGI(TAG, "Parsed: Slave=%d, Register=%d, Value=%d", slave_id, register_addr, value);
    
    // Validate parameters
    if (slave_id > 247) {
        snprintf(response, sizeof(response), 
                "{\"status\":\"error\",\"message\":\"Invalid slave ID: %d (must be 0-247, 0 for broadcast)\"}", slave_id);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }
    
    // register_addr and value are uint16_t, so they're already limited to 0-65535
    
    // Execute Modbus write
    ESP_LOGI(TAG, "[PROC] Executing Modbus write single register...");
    modbus_result_t result = modbus_write_single_register(slave_id, register_addr, value);
    
    if (result == MODBUS_SUCCESS) {
        ESP_LOGI(TAG, "Write single register successful");
        snprintf(response, sizeof(response), 
                "{\"status\":\"success\",\"message\":\"Register %d = %d written to slave %d successfully\"}", 
                register_addr, value, slave_id);
    } else {
        ESP_LOGE(TAG, "Write single register failed with code: 0x%02X", result);
        const char* error_msg = (result == MODBUS_TIMEOUT) ? "Communication timeout" :
                               (result == MODBUS_INVALID_CRC) ? "CRC error" :
                               (result == MODBUS_ILLEGAL_FUNCTION) ? "Illegal function" :
                               (result == MODBUS_ILLEGAL_DATA_ADDRESS) ? "Illegal data address" :
                               (result == MODBUS_ILLEGAL_DATA_VALUE) ? "Illegal data value" :
                               "Communication error";
        snprintf(response, sizeof(response), 
                "{\"status\":\"error\",\"message\":\"Write failed: %s (Code: 0x%02X)\"}", 
                error_msg, result);
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// Write Multiple Registers Handler
static esp_err_t write_multiple_registers_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Write multiple registers request received");
    
    // Read the POST data
    size_t recv_size = MIN(req->content_len, 1024);
    char content[1025] = {0};
    
    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Received write multiple registers data: %s", content);
    
    // Parse parameters
    uint8_t slave_id = 1;
    uint16_t start_addr = 0;
    char response[512];
    uint16_t values[125]; // Maximum registers that can be written in one request
    uint16_t num_regs = 0;
    
    // Parse slave_id
    char *param = strstr(content, "slave_id=");
    if (param) {
        slave_id = (uint8_t)atoi(param + 9);
    }
    
    // Parse start_addr
    param = strstr(content, "start_addr=");
    if (param) {
        start_addr = (uint16_t)atoi(param + 11);
    }
    
    // Parse values
    param = strstr(content, "values=");
    if (param) {
        char *values_str = param + 7;
        char *end_of_values = strchr(values_str, '&');
        if (end_of_values) *end_of_values = '\0';
        
        // Parse comma-separated values
        char *token = strtok(values_str, ",");
        while (token && num_regs < 125) {
            values[num_regs] = (uint16_t)atoi(token);
            ESP_LOGI(TAG, "Value[%d]: %d", num_regs, values[num_regs]);
            num_regs++;
            token = strtok(NULL, ",");
        }
    }
    
    ESP_LOGI(TAG, "Parsed: Slave=%d, Start=%d, Count=%d", slave_id, start_addr, num_regs);
    
    // Validate parameters
    if (slave_id > 247) {
        snprintf(response, sizeof(response), 
                "{\"status\":\"error\",\"message\":\"Invalid slave ID: %d (must be 0-247, 0 for broadcast)\"}", slave_id);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }
    
    // start_addr is uint16_t, so it's already limited to 0-65535
    
    if (num_regs == 0 || num_regs > 125) {
        snprintf(response, sizeof(response), 
                "{\"status\":\"error\",\"message\":\"Invalid register count: %d (must be 1-125)\"}", num_regs);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }
    
    // Execute Modbus write
    ESP_LOGI(TAG, "[PROC] Executing Modbus write multiple registers...");
    modbus_result_t result = modbus_write_multiple_registers(slave_id, start_addr, num_regs, values);
    
    if (result == MODBUS_SUCCESS) {
        ESP_LOGI(TAG, "Write multiple registers successful");
        snprintf(response, sizeof(response), 
                "{\"status\":\"success\",\"message\":\"%d registers written starting at %d on slave %d successfully\"}", 
                num_regs, start_addr, slave_id);
    } else {
        ESP_LOGE(TAG, "Write multiple registers failed with code: 0x%02X", result);
        const char* error_msg = (result == MODBUS_TIMEOUT) ? "Communication timeout" :
                               (result == MODBUS_INVALID_CRC) ? "CRC error" :
                               (result == MODBUS_ILLEGAL_FUNCTION) ? "Illegal function" :
                               (result == MODBUS_ILLEGAL_DATA_ADDRESS) ? "Illegal data address" :
                               (result == MODBUS_ILLEGAL_DATA_VALUE) ? "Illegal data value" :
                               "Communication error";
        snprintf(response, sizeof(response), 
                "{\"status\":\"error\",\"message\":\"Write failed: %s (Code: 0x%02X)\"}", 
                error_msg, result);
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// Watchdog control handler
static esp_err_t watchdog_control_handler(httpd_req_t *req)
{
    char buf[512];
    size_t recv_size = MIN(req->content_len, sizeof(buf) - 1);
    
    int ret = httpd_req_recv(req, buf, recv_size);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // Parse action parameter
    char action[32] = {0};
    char *action_start = strstr(buf, "action=");
    if (action_start) {
        sscanf(action_start, "action=%31s", action);
    }
    
    cJSON *response_json = cJSON_CreateObject();
    
    if (strcmp(action, "reset") == 0) {
        ESP_LOGI(TAG, "Manual system reset requested via web interface");
        cJSON_AddStringToObject(response_json, "status", "success");
        cJSON_AddStringToObject(response_json, "message", "System reset initiated");
        cJSON_AddNumberToObject(response_json, "restart_in", 3);
        
        char *response = cJSON_Print(response_json);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        free(response);
        cJSON_Delete(response_json);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
        return ESP_OK;
        
    } else if (strcmp(action, "status") == 0) {
        cJSON_AddStringToObject(response_json, "status", "success");
        cJSON_AddStringToObject(response_json, "watchdog_enabled", "true");
        cJSON_AddNumberToObject(response_json, "timeout_seconds", 5);
        cJSON_AddStringToObject(response_json, "cpu0_monitored", "true");
        cJSON_AddStringToObject(response_json, "cpu1_monitored", "true");
        cJSON_AddNumberToObject(response_json, "uptime_ms", (uint32_t)(esp_timer_get_time() / 1000));
        
    } else {
        cJSON_AddStringToObject(response_json, "status", "error");
        cJSON_AddStringToObject(response_json, "message", "Invalid action. Use 'reset' or 'status'");
    }
    
    char *response = cJSON_Print(response_json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    free(response);
    cJSON_Delete(response_json);
    return ESP_OK;
}

// GPIO trigger handler
static esp_err_t gpio_trigger_handler(httpd_req_t *req)
{
    char buf[512];
    size_t recv_size = MIN(req->content_len, sizeof(buf) - 1);
    
    int ret = httpd_req_recv(req, buf, recv_size);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // Parse GPIO parameters
    char action[32] = {0};
    int gpio_pin = -1;
    int gpio_value = -1;
    
    // Add debug logging
    ESP_LOGI(TAG, "GPIO request body: %s", buf);
    
    char *action_start = strstr(buf, "action=");
    if (action_start) {
        // Improved parsing to handle URL-encoded parameters properly
        char *action_end = strchr(action_start + 7, '&');
        if (action_end) {
            int len = MIN(action_end - (action_start + 7), 31);
            strncpy(action, action_start + 7, len);
            action[len] = '\0';
        } else {
            sscanf(action_start, "action=%31s", action);
        }
    }
    
    char *pin_start = strstr(buf, "pin=");
    if (pin_start) {
        sscanf(pin_start, "pin=%d", &gpio_pin);
    }
    
    char *value_start = strstr(buf, "value=");
    if (value_start) {
        sscanf(value_start, "value=%d", &gpio_value);
    }
    
    ESP_LOGI(TAG, "Parsed GPIO params - action: '%s', pin: %d, value: %d", action, gpio_pin, gpio_value);
    
    cJSON *response_json = cJSON_CreateObject();
    
    // Validate GPIO pin (avoid system pins)
    if (gpio_pin < 0 || gpio_pin > 39 || gpio_pin == 1 || gpio_pin == 3 || 
        gpio_pin == 6 || gpio_pin == 7 || gpio_pin == 8 || gpio_pin == 9 || 
        gpio_pin == 10 || gpio_pin == 11) {
        cJSON_AddStringToObject(response_json, "status", "error");
        cJSON_AddStringToObject(response_json, "message", "Invalid GPIO pin or system reserved pin");
    } else if (strcmp(action, "set") == 0 && (gpio_value == 0 || gpio_value == 1)) {
        // Set GPIO output
        esp_err_t gpio_err = gpio_reset_pin(gpio_pin);
        if (gpio_err == ESP_OK) {
            gpio_err = gpio_set_direction(gpio_pin, GPIO_MODE_OUTPUT);
        }
        if (gpio_err == ESP_OK) {
            gpio_err = gpio_set_level(gpio_pin, gpio_value);
        }
        
        if (gpio_err == ESP_OK) {
            cJSON_AddStringToObject(response_json, "status", "success");
            cJSON_AddStringToObject(response_json, "message", "GPIO set successfully");
            cJSON_AddNumberToObject(response_json, "pin", gpio_pin);
            cJSON_AddNumberToObject(response_json, "value", gpio_value);
            ESP_LOGI(TAG, "GPIO %d set to %d via web interface", gpio_pin, gpio_value);
        } else {
            cJSON_AddStringToObject(response_json, "status", "error");
            cJSON_AddStringToObject(response_json, "message", "Failed to configure GPIO");
        }
        
    } else if (strcmp(action, "read") == 0) {
        // Read GPIO input
        esp_err_t gpio_err = gpio_reset_pin(gpio_pin);
        if (gpio_err == ESP_OK) {
            gpio_err = gpio_set_direction(gpio_pin, GPIO_MODE_INPUT);
        }
        if (gpio_err == ESP_OK) {
            gpio_err = gpio_set_pull_mode(gpio_pin, GPIO_PULLUP_ONLY);
        }
        
        if (gpio_err == ESP_OK) {
            int level = gpio_get_level(gpio_pin);
            cJSON_AddStringToObject(response_json, "status", "success");
            cJSON_AddStringToObject(response_json, "message", "GPIO read successfully");
            cJSON_AddNumberToObject(response_json, "pin", gpio_pin);
            cJSON_AddNumberToObject(response_json, "value", level);
            ESP_LOGI(TAG, "GPIO %d read: %d via web interface", gpio_pin, level);
        } else {
            cJSON_AddStringToObject(response_json, "status", "error");
            cJSON_AddStringToObject(response_json, "message", "Failed to read GPIO");
        }
        
    } else {
        cJSON_AddStringToObject(response_json, "status", "error");
        if (strlen(action) == 0) {
            cJSON_AddStringToObject(response_json, "message", "Missing action parameter");
        } else if (strcmp(action, "set") == 0 && (gpio_value != 0 && gpio_value != 1)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Invalid GPIO value: %d. Use 0 (LOW) or 1 (HIGH)", gpio_value);
            cJSON_AddStringToObject(response_json, "message", msg);
        } else if (strcmp(action, "set") != 0 && strcmp(action, "read") != 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Invalid action: '%s'. Use 'set' or 'read'", action);
            cJSON_AddStringToObject(response_json, "message", msg);
        } else {
            cJSON_AddStringToObject(response_json, "message", "Invalid parameters. Use action=set&pin=X&value=Y or action=read&pin=X");
        }
    }
    
    char *response = cJSON_Print(response_json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    free(response);
    cJSON_Delete(response_json);
    return ESP_OK;
}

// Start HTTP server
static esp_err_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 40; // Increased to accommodate all SIM/SD/RTC endpoints (34+ handlers)
    config.max_open_sockets = 7;      // Maximum allowed by LWIP configuration
    config.stack_size = 16384;        // Increased to 16KB to handle large stack buffers safely
    config.task_priority = 5;
    config.recv_wait_timeout = 20;    // Reduced from 60 to 20 seconds for faster error detection
    config.send_wait_timeout = 20;    // Reduced from 60 to 20 seconds for faster error detection
    config.lru_purge_enable = true;   // Enable LRU purging of connections
    config.backlog_conn = 8;          // TCP listen backlog queue
    config.enable_so_linger = false;  // Disable socket lingering to prevent TIME_WAIT issues
    config.keep_alive_enable = true;  // Enable HTTP keep-alive for connection reuse
    config.keep_alive_idle = 30;      // Keep-alive idle time (30 seconds)
    config.keep_alive_interval = 5;   // Keep-alive probe interval (5 seconds)
    config.keep_alive_count = 3;      // Keep-alive probe count (3 attempts)

    if (httpd_start(&g_server, &config) == ESP_OK) {
        // Main configuration page
        httpd_uri_t config_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = config_page_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &config_uri);

        // Save configuration
        httpd_uri_t save_uri = {
            .uri = "/save_config",
            .method = HTTP_POST,
            .handler = save_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &save_uri);

        // Save Azure configuration
        httpd_uri_t save_azure_uri = {
            .uri = "/save_azure_config",
            .method = HTTP_POST,
            .handler = save_azure_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &save_azure_uri);

        // Save modem configuration
        httpd_uri_t save_modem_uri = {
            .uri = "/save_modem_config",
            .method = HTTP_POST,
            .handler = save_modem_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &save_modem_uri);

        // Save system configuration
        httpd_uri_t save_system_uri = {
            .uri = "/save_system_config",
            .method = HTTP_POST,
            .handler = save_system_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &save_system_uri);

        // Test sensor
        httpd_uri_t test_uri = {
            .uri = "/test_sensor",
            .method = HTTP_POST,
            .handler = test_sensor_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &test_uri);

        // Start operation mode
        httpd_uri_t operation_uri = {
            .uri = "/start_operation",
            .method = HTTP_POST,
            .handler = start_operation_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &operation_uri);

        // WiFi scan endpoint
        httpd_uri_t scan_uri = {
            .uri = "/scan_wifi",
            .method = HTTP_GET,
            .handler = wifi_scan_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &scan_uri);

        // Live data endpoint  
        httpd_uri_t live_uri = {
            .uri = "/live_data",
            .method = HTTP_GET,
            .handler = live_data_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &live_uri);

        // Edit sensor endpoint
        httpd_uri_t edit_uri = {
            .uri = "/edit_sensor",
            .method = HTTP_POST,
            .handler = edit_sensor_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &edit_uri);

        // Save single sensor endpoint
        httpd_uri_t save_single_uri = {
            .uri = "/save_single_sensor",
            .method = HTTP_POST,
            .handler = save_single_sensor_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &save_single_uri);

        // Delete sensor endpoint
        httpd_uri_t delete_uri = {
            .uri = "/delete_sensor",
            .method = HTTP_POST,
            .handler = delete_sensor_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &delete_uri);

        // RS485 Test endpoint
        httpd_uri_t test_rs485_uri = {
            .uri = "/test_rs485",
            .method = HTTP_POST,
            .handler = test_rs485_handler,
            .user_ctx = NULL
        };
        esp_err_t test_rs485_reg = httpd_register_uri_handler(g_server, &test_rs485_uri);
        if (test_rs485_reg == ESP_OK) {
            ESP_LOGI(TAG, "SUCCESS: /test_rs485 endpoint registered successfully");
        } else {
            ESP_LOGE(TAG, "ERROR: Failed to register /test_rs485 endpoint: %s", esp_err_to_name(test_rs485_reg));
        }

        // Water Quality Sensor Test endpoint
        httpd_uri_t test_water_quality_uri = {
            .uri = "/test_water_quality_sensor",
            .method = HTTP_POST,
            .handler = test_water_quality_sensor_handler,
            .user_ctx = NULL
        };
        esp_err_t test_wq_reg = httpd_register_uri_handler(g_server, &test_water_quality_uri);
        if (test_wq_reg == ESP_OK) {
            ESP_LOGI(TAG, "SUCCESS: /test_water_quality_sensor endpoint registered successfully");
        } else {
            ESP_LOGE(TAG, "ERROR: Failed to register /test_water_quality_sensor endpoint: %s", esp_err_to_name(test_wq_reg));
        }

        // Save Water Quality Sensor endpoint
        httpd_uri_t save_water_quality_uri = {
            .uri = "/save_water_quality_sensor",
            .method = HTTP_POST,
            .handler = save_water_quality_sensor_handler,
            .user_ctx = NULL
        };
        esp_err_t save_wq_reg = httpd_register_uri_handler(g_server, &save_water_quality_uri);
        if (save_wq_reg == ESP_OK) {
            ESP_LOGI(TAG, "SUCCESS: /save_water_quality_sensor endpoint registered successfully");
        } else {
            ESP_LOGE(TAG, "ERROR: Failed to register /save_water_quality_sensor endpoint: %s", esp_err_to_name(save_wq_reg));
        }


        // System status API endpoint
        httpd_uri_t system_status_uri = {
            .uri = "/api/system_status",
            .method = HTTP_GET,
            .handler = system_status_handler,
            .user_ctx = NULL
        };
        esp_err_t system_status_reg = httpd_register_uri_handler(g_server, &system_status_uri);
        if (system_status_reg == ESP_OK) {
            ESP_LOGI(TAG, "SUCCESS: /api/system_status endpoint registered successfully");
        } else {
            ESP_LOGE(TAG, "ERROR: Failed to register /api/system_status endpoint: %s", esp_err_to_name(system_status_reg));
        }

        // Write single register endpoint
        httpd_uri_t write_single_uri = {
            .uri = "/write_single_register",
            .method = HTTP_POST,
            .handler = write_single_register_handler,
            .user_ctx = NULL
        };
        esp_err_t write_single_reg = httpd_register_uri_handler(g_server, &write_single_uri);
        if (write_single_reg == ESP_OK) {
            ESP_LOGI(TAG, "SUCCESS: /write_single_register endpoint registered successfully");
        } else {
            ESP_LOGE(TAG, "ERROR: Failed to register /write_single_register endpoint: %s", esp_err_to_name(write_single_reg));
        }

        // Write multiple registers endpoint
        httpd_uri_t write_multiple_uri = {
            .uri = "/write_multiple_registers",
            .method = HTTP_POST,
            .handler = write_multiple_registers_handler,
            .user_ctx = NULL
        };
        esp_err_t write_multiple_reg = httpd_register_uri_handler(g_server, &write_multiple_uri);
        if (write_multiple_reg == ESP_OK) {
            ESP_LOGI(TAG, "SUCCESS: /write_multiple_registers endpoint registered successfully");
        } else {
            ESP_LOGE(TAG, "ERROR: Failed to register /write_multiple_registers endpoint: %s", esp_err_to_name(write_multiple_reg));
        }

        // Logo endpoint
        httpd_uri_t logo_uri = {
            .uri = "/logo",
            .method = HTTP_GET,
            .handler = logo_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &logo_uri);


        // Favicon handler to prevent 404 errors
        httpd_uri_t favicon_uri = {
            .uri = "/favicon.ico",
            .method = HTTP_GET,
            .handler = favicon_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &favicon_uri);

        // Reboot system endpoint
        httpd_uri_t reboot_uri = {
            .uri = "/reboot",
            .method = HTTP_POST,
            .handler = reboot_handler,
            .user_ctx = NULL
        };
        esp_err_t reboot_reg = httpd_register_uri_handler(g_server, &reboot_uri);
        if (reboot_reg == ESP_OK) {
            ESP_LOGI(TAG, "SUCCESS: /reboot endpoint registered successfully");
        } else {
            ESP_LOGE(TAG, "ERROR: Failed to register /reboot endpoint: %s", esp_err_to_name(reboot_reg));
        }

        // Watchdog control endpoint
        httpd_uri_t watchdog_control_uri = {
            .uri = "/watchdog_control",
            .method = HTTP_POST,
            .handler = watchdog_control_handler,
            .user_ctx = NULL
        };
        esp_err_t watchdog_reg = httpd_register_uri_handler(g_server, &watchdog_control_uri);
        if (watchdog_reg == ESP_OK) {
            ESP_LOGI(TAG, "SUCCESS: /watchdog_control endpoint registered successfully");
        } else {
            ESP_LOGE(TAG, "ERROR: Failed to register /watchdog_control endpoint: %s", esp_err_to_name(watchdog_reg));
        }

        // GPIO trigger endpoint
        httpd_uri_t gpio_trigger_uri = {
            .uri = "/gpio_trigger",
            .method = HTTP_POST,
            .handler = gpio_trigger_handler,
            .user_ctx = NULL
        };
        esp_err_t gpio_reg = httpd_register_uri_handler(g_server, &gpio_trigger_uri);
        if (gpio_reg == ESP_OK) {
            ESP_LOGI(TAG, "SUCCESS: /gpio_trigger endpoint registered successfully");
        } else {
            ESP_LOGE(TAG, "ERROR: Failed to register /gpio_trigger endpoint: %s", esp_err_to_name(gpio_reg));
        }

        // Network mode configuration endpoint
        httpd_uri_t save_network_mode_uri = {
            .uri = "/save_network_mode",
            .method = HTTP_POST,
            .handler = save_network_mode_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &save_network_mode_uri);

        // SIM configuration endpoint
        httpd_uri_t save_sim_config_uri = {
            .uri = "/save_sim_config",
            .method = HTTP_POST,
            .handler = save_sim_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &save_sim_config_uri);

        // SD card configuration endpoint
        httpd_uri_t save_sd_config_uri = {
            .uri = "/save_sd_config",
            .method = HTTP_POST,
            .handler = save_sd_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &save_sd_config_uri);

        // RTC configuration endpoint
        httpd_uri_t save_rtc_config_uri = {
            .uri = "/save_rtc_config",
            .method = HTTP_POST,
            .handler = save_rtc_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &save_rtc_config_uri);

        // Telegram config endpoint
        httpd_uri_t save_telegram_config_uri = {
            .uri = "/save_telegram_config",
            .method = HTTP_POST,
            .handler = save_telegram_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &save_telegram_config_uri);

        // Telegram test API endpoint
        httpd_uri_t api_telegram_test_uri = {
            .uri = "/api/telegram_test",
            .method = HTTP_POST,
            .handler = api_telegram_test_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &api_telegram_test_uri);

        // Live Modbus poll API endpoint
        httpd_uri_t api_modbus_poll_uri = {
            .uri = "/api/modbus_poll",
            .method = HTTP_GET,
            .handler = api_modbus_poll_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &api_modbus_poll_uri);

        // SIM test API endpoint
        httpd_uri_t api_sim_test_uri = {
            .uri = "/api/sim_test",
            .method = HTTP_POST,
            .handler = api_sim_test_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &api_sim_test_uri);

        // SIM test status API endpoint
        httpd_uri_t api_sim_test_status_uri = {
            .uri = "/api/sim_test_status",
            .method = HTTP_GET,
            .handler = api_sim_test_status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &api_sim_test_status_uri);

        // SD status API endpoint
        httpd_uri_t api_sd_status_uri = {
            .uri = "/api/sd_status",
            .method = HTTP_GET,
            .handler = api_sd_status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &api_sd_status_uri);

        // SD clear API endpoint
        httpd_uri_t api_sd_clear_uri = {
            .uri = "/api/sd_clear",
            .method = HTTP_POST,
            .handler = api_sd_clear_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &api_sd_clear_uri);

        // SD replay API endpoint
        httpd_uri_t api_sd_replay_uri = {
            .uri = "/api/sd_replay",
            .method = HTTP_POST,
            .handler = api_sd_replay_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &api_sd_replay_uri);

        // RTC time API endpoint
        httpd_uri_t api_rtc_time_uri = {
            .uri = "/api/rtc_time",
            .method = HTTP_GET,
            .handler = api_rtc_time_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &api_rtc_time_uri);

        // RTC sync API endpoint
        httpd_uri_t api_rtc_sync_uri = {
            .uri = "/api/rtc_sync",
            .method = HTTP_POST,
            .handler = api_rtc_sync_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &api_rtc_sync_uri);

        // RTC set API endpoint
        httpd_uri_t api_rtc_set_uri = {
            .uri = "/api/rtc_set",
            .method = HTTP_POST,
            .handler = api_rtc_set_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &api_rtc_set_uri);

        // Modbus status API endpoint
        httpd_uri_t api_modbus_status_uri = {
            .uri = "/api/modbus/status",
            .method = HTTP_GET,
            .handler = api_modbus_status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &api_modbus_status_uri);

        // Azure IoT Hub status API endpoint
        httpd_uri_t api_azure_status_uri = {
            .uri = "/api/azure/status",
            .method = HTTP_GET,
            .handler = api_azure_status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &api_azure_status_uri);

        // Azure telemetry history API endpoint
        httpd_uri_t api_telemetry_history_uri = {
            .uri = "/api/telemetry/history",
            .method = HTTP_GET,
            .handler = api_telemetry_history_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &api_telemetry_history_uri);

        // Modbus Explorer: Device Scanner endpoint
        httpd_uri_t modbus_scan_uri = {
            .uri = "/modbus_scan",
            .method = HTTP_POST,
            .handler = modbus_scan_handler,
            .user_ctx = NULL
        };
        esp_err_t scan_reg = httpd_register_uri_handler(g_server, &modbus_scan_uri);
        if (scan_reg == ESP_OK) {
            ESP_LOGI(TAG, "SUCCESS: /modbus_scan endpoint registered successfully");
        } else {
            ESP_LOGE(TAG, "ERROR: Failed to register /modbus_scan endpoint: %s", esp_err_to_name(scan_reg));
        }

        // Modbus Explorer: Live Register Reader endpoint
        httpd_uri_t modbus_read_live_uri = {
            .uri = "/modbus_read_live",
            .method = HTTP_POST,
            .handler = modbus_read_live_handler,
            .user_ctx = NULL
        };
        esp_err_t live_reg = httpd_register_uri_handler(g_server, &modbus_read_live_uri);
        if (live_reg == ESP_OK) {
            ESP_LOGI(TAG, "SUCCESS: /modbus_read_live endpoint registered successfully");
        } else {
            ESP_LOGE(TAG, "ERROR: Failed to register /modbus_read_live endpoint: %s", esp_err_to_name(live_reg));
        }

        ESP_LOGI(TAG, "Web server started on port 80");
        ESP_LOGI(TAG, "[NET] All URI handlers registered successfully (including Modbus Explorer endpoints)");
        ESP_LOGI(TAG, "[CONFIG] Available endpoints: /, /save_config, /save_azure_config, /save_network_mode, /save_sim_config, /save_sd_config, /save_rtc_config, /test_sensor, /test_rs485, /start_operation, /scan_wifi, /live_data, /edit_sensor, /save_single_sensor, /delete_sensor, /api/system_status, /api/sim_test, /api/sd_status, /api/sd_clear, /api/sd_replay, /api/rtc_time, /api/rtc_sync, /api/rtc_set, /write_single_register, /write_multiple_registers, /modbus_scan, /modbus_read_live, /reboot, /watchdog_control, /gpio_trigger, /logo, /favicon.ico");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to start web server");
    return ESP_FAIL;
}

// Initialize web configuration
esp_err_t web_config_init(void)
{
    // Load configuration from NVS
    config_load_from_nvs(&g_system_config);

    // Debug: Show config_complete flag value
    ESP_LOGI(TAG, "[DEBUG] config_complete flag = %s", g_system_config.config_complete ? "TRUE" : "FALSE");
    ESP_LOGI(TAG, "[DEBUG] sensor_count = %d", g_system_config.sensor_count);
    ESP_LOGI(TAG, "[DEBUG] wifi_ssid = '%s'", g_system_config.wifi_ssid);

    // Check if configuration is complete
    if (g_system_config.config_complete) {
        g_config_state = CONFIG_STATE_OPERATION;
        ESP_LOGI(TAG, "Configuration complete, starting in operation mode");
    } else {
        g_config_state = CONFIG_STATE_SETUP;
        ESP_LOGI(TAG, "No configuration found, starting in setup mode");
        // Set flag for auto-start
        g_web_server_auto_start = true;
    }

    return ESP_OK;
}

// Start STA mode for configuration (actually starts AP mode)
esp_err_t web_config_start_sta_mode(void)
{
    ESP_LOGI(TAG, "Starting WiFi in AP mode for configuration");
    
    // Initialize WiFi (only if not already initialized)
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create event loop only if it doesn't exist
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create AP interface instead of STA for configuration mode
    esp_netif_create_default_wifi_ap();

    // Initialize WiFi driver only if not already initialized
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Set WiFi country code for proper scanning
    wifi_country_t country = {
        .cc = "US",
        .schan = 1,
        .nchan = 11,
        .policy = WIFI_COUNTRY_POLICY_AUTO
    };
    esp_wifi_set_country(&country);
    
    // Register event handlers (ignore if already registered)
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGW(TAG, "WiFi event handler registration: %s", esp_err_to_name(ret));
    }
    
    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGW(TAG, "IP event handler registration: %s", esp_err_to_name(ret));
    }
    
    // Create AP for configuration access with stronger authentication
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ModbusIoT-Config",
            .ssid_len = strlen("MenuTest"),
            .password = "config123",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,  // Use mixed WPA/WPA2 for better compatibility
            .ssid_hidden = 0,
            .beacon_interval = 100,
            .channel = 6
        },
    };
    
    // Ensure password length is correct
    if (strlen("config123") < 8) {
        ESP_LOGE(TAG, "ERROR: AP password too short, using default");
        strcpy((char*)ap_config.ap.password, "config123456");  // 12 chars
    }
    
    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi AP mode: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi AP config: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "SUCCESS: WiFi AP started successfully!");
    ESP_LOGI(TAG, "[MOBILE] SSID: MenuTest");
    ESP_LOGI(TAG, "[KEY] Password: config123");
    ESP_LOGI(TAG, "[WEB] IP: 192.168.4.1");
    ESP_LOGI(TAG, "[LINK] URL: http://192.168.4.1");
    ESP_LOGI(TAG, "[INFO] Connect your device to this AP and visit the URL");
    
    // Initialize Modbus RS485 for sensor testing in setup mode
    ESP_LOGI(TAG, "[CONFIG] Initializing Modbus RS485 for sensor testing...");
    esp_err_t modbus_ret = modbus_init();
    if (modbus_ret != ESP_OK) {
        ESP_LOGE(TAG, "ERROR: Failed to initialize Modbus in setup mode: %s", esp_err_to_name(modbus_ret));
        ESP_LOGE(TAG, "[WARN] Sensor testing will not work until Modbus is properly connected");
    } else {
        ESP_LOGI(TAG, "SUCCESS: Modbus RS485 initialized successfully in setup mode");
    }
    
    return start_webserver();
}

// ============================================================================
// REST API Handlers for Network, SIM, SD, and RTC Configuration
// ============================================================================

// Handler: /save_network_mode - Save network mode (WiFi or SIM)
static esp_err_t save_network_mode_handler(httpd_req_t *req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    int network_mode = 0;
    sscanf(buf, "network_mode=%d", &network_mode);

    g_system_config.network_mode = (network_mode_t)network_mode;
    config_save_to_nvs(&g_system_config);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"success\",\"message\":\"Network mode saved\"}");
    return ESP_OK;
}

// Handler: /save_sim_config - Save SIM configuration
static esp_err_t save_sim_config_handler(httpd_req_t *req) {
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Parse SIM configuration parameters
    char *param;
    if ((param = strstr(buf, "sim_apn=")) != NULL) {
        sscanf(param, "sim_apn=%63[^&]", g_system_config.sim_config.apn);
    }
    if ((param = strstr(buf, "sim_apn_user=")) != NULL) {
        sscanf(param, "sim_apn_user=%63[^&]", g_system_config.sim_config.apn_user);
    }
    if ((param = strstr(buf, "sim_apn_pass=")) != NULL) {
        sscanf(param, "sim_apn_pass=%63[^&]", g_system_config.sim_config.apn_pass);
    }
    if ((param = strstr(buf, "sim_uart=")) != NULL) {
        sscanf(param, "sim_uart=%d", (int*)&g_system_config.sim_config.uart_num);
    }
    if ((param = strstr(buf, "sim_tx_pin=")) != NULL) {
        sscanf(param, "sim_tx_pin=%d", &g_system_config.sim_config.uart_tx_pin);
    }
    if ((param = strstr(buf, "sim_rx_pin=")) != NULL) {
        sscanf(param, "sim_rx_pin=%d", &g_system_config.sim_config.uart_rx_pin);
    }
    if ((param = strstr(buf, "sim_pwr_pin=")) != NULL) {
        sscanf(param, "sim_pwr_pin=%d", &g_system_config.sim_config.pwr_pin);
    }
    if ((param = strstr(buf, "sim_reset_pin=")) != NULL) {
        sscanf(param, "sim_reset_pin=%d", &g_system_config.sim_config.reset_pin);
    }
    if ((param = strstr(buf, "sim_baud=")) != NULL) {
        sscanf(param, "sim_baud=%d", &g_system_config.sim_config.uart_baud_rate);
    }

    g_system_config.sim_config.enabled = true;
    config_save_to_nvs(&g_system_config);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"success\",\"message\":\"SIM configuration saved\"}");
    return ESP_OK;
}

// Handler: /save_sd_config - Save SD card configuration
static esp_err_t save_sd_config_handler(httpd_req_t *req) {
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Parse SD configuration parameters
    g_system_config.sd_config.enabled = (strstr(buf, "sd_enabled=1") != NULL);
    g_system_config.sd_config.cache_on_failure = (strstr(buf, "sd_cache_on_failure=1") != NULL);

    char *param;
    if ((param = strstr(buf, "sd_mosi=")) != NULL) {
        sscanf(param, "sd_mosi=%d", &g_system_config.sd_config.mosi_pin);
    }
    if ((param = strstr(buf, "sd_miso=")) != NULL) {
        sscanf(param, "sd_miso=%d", &g_system_config.sd_config.miso_pin);
    }
    if ((param = strstr(buf, "sd_clk=")) != NULL) {
        sscanf(param, "sd_clk=%d", &g_system_config.sd_config.clk_pin);
    }
    if ((param = strstr(buf, "sd_cs=")) != NULL) {
        sscanf(param, "sd_cs=%d", &g_system_config.sd_config.cs_pin);
    }
    if ((param = strstr(buf, "sd_spi_host=")) != NULL) {
        sscanf(param, "sd_spi_host=%d", (int*)&g_system_config.sd_config.spi_host);
    }

    config_save_to_nvs(&g_system_config);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"success\",\"message\":\"SD card configuration saved\"}");
    return ESP_OK;
}

// Handler: /save_rtc_config - Save RTC configuration
static esp_err_t save_rtc_config_handler(httpd_req_t *req) {
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Parse RTC configuration parameters
    g_system_config.rtc_config.enabled = (strstr(buf, "rtc_enabled=1") != NULL);
    g_system_config.rtc_config.sync_on_boot = (strstr(buf, "rtc_sync_on_boot=1") != NULL);
    g_system_config.rtc_config.update_from_ntp = (strstr(buf, "rtc_update_from_ntp=1") != NULL);

    char *param;
    if ((param = strstr(buf, "rtc_sda=")) != NULL) {
        sscanf(param, "rtc_sda=%d", &g_system_config.rtc_config.sda_pin);
    }
    if ((param = strstr(buf, "rtc_scl=")) != NULL) {
        sscanf(param, "rtc_scl=%d", &g_system_config.rtc_config.scl_pin);
    }
    if ((param = strstr(buf, "rtc_i2c_num=")) != NULL) {
        sscanf(param, "rtc_i2c_num=%d", (int*)&g_system_config.rtc_config.i2c_num);
    }

    config_save_to_nvs(&g_system_config);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"success\",\"message\":\"RTC configuration saved\"}");
    return ESP_OK;
}

// Handler: /save_telegram_config - Save Telegram Bot configuration
static esp_err_t save_telegram_config_handler(httpd_req_t *req) {
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    ESP_LOGI(TAG, "Saving Telegram configuration");

    // Parse Telegram configuration parameters
    g_system_config.telegram_config.enabled = (strstr(buf, "telegram_enabled=1") != NULL);
    g_system_config.telegram_config.alerts_enabled = (strstr(buf, "telegram_alerts=1") != NULL);
    g_system_config.telegram_config.startup_notification = (strstr(buf, "telegram_startup=1") != NULL);

    char *param;

    // Parse bot token
    if ((param = strstr(buf, "telegram_bot_token=")) != NULL) {
        param += strlen("telegram_bot_token=");
        char *end = strchr(param, '&');
        int len = end ? (end - param) : strlen(param);
        if (len > 0 && len < sizeof(g_system_config.telegram_config.bot_token)) {
            strncpy(g_system_config.telegram_config.bot_token, param, len);
            g_system_config.telegram_config.bot_token[len] = '\0';

            // URL decode the token
            char decoded[64];
            url_decode(decoded, g_system_config.telegram_config.bot_token);
            strncpy(g_system_config.telegram_config.bot_token, decoded, sizeof(g_system_config.telegram_config.bot_token) - 1);
        }
    }

    // Parse chat ID
    if ((param = strstr(buf, "telegram_chat_id=")) != NULL) {
        param += strlen("telegram_chat_id=");
        char *end = strchr(param, '&');
        int len = end ? (end - param) : strlen(param);
        if (len > 0 && len < sizeof(g_system_config.telegram_config.chat_id)) {
            strncpy(g_system_config.telegram_config.chat_id, param, len);
            g_system_config.telegram_config.chat_id[len] = '\0';
        }
    }

    // Parse poll interval
    if ((param = strstr(buf, "telegram_poll_interval=")) != NULL) {
        sscanf(param, "telegram_poll_interval=%d", &g_system_config.telegram_config.poll_interval);
    }

    ESP_LOGI(TAG, "Telegram enabled: %s", g_system_config.telegram_config.enabled ? "YES" : "NO");
    ESP_LOGI(TAG, "Chat ID: %s", g_system_config.telegram_config.chat_id);

    config_save_to_nvs(&g_system_config);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"success\",\"message\":\"Telegram configuration saved successfully!\"}");
    return ESP_OK;
}

// Handler: /api/telegram_test - Test Telegram bot connection
static esp_err_t api_telegram_test_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    // Read POST data (URL-encoded format)
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Failed to read request data\"}");
        return ESP_OK;
    }
    buf[ret] = '\0';

    ESP_LOGI(TAG, "Telegram test request received, data length: %d", ret);

    // Parse URL-encoded parameters: bot_token=xxx&chat_id=yyy
    char bot_token[128] = {0};
    char chat_id[64] = {0};
    char *param;

    // Parse bot_token
    if ((param = strstr(buf, "bot_token=")) != NULL) {
        param += strlen("bot_token=");
        char *end = strchr(param, '&');
        int len = end ? (end - param) : strlen(param);
        if (len > 0 && len < sizeof(bot_token)) {
            strncpy(bot_token, param, len);
            bot_token[len] = '\0';
            // URL decode (handles %3A for : and other special chars)
            char decoded[128];
            url_decode(decoded, bot_token);
            strncpy(bot_token, decoded, sizeof(bot_token) - 1);
        }
    }

    // Parse chat_id
    if ((param = strstr(buf, "chat_id=")) != NULL) {
        param += strlen("chat_id=");
        char *end = strchr(param, '&');
        int len = end ? (end - param) : strlen(param);
        if (len > 0 && len < sizeof(chat_id)) {
            strncpy(chat_id, param, len);
            chat_id[len] = '\0';
            // URL decode
            char decoded[64];
            url_decode(decoded, chat_id);
            strncpy(chat_id, decoded, sizeof(chat_id) - 1);
        }
    }

    ESP_LOGI(TAG, "Parsed bot_token length: %d, chat_id: %s", strlen(bot_token), chat_id);

    if (strlen(bot_token) == 0) {
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Bot token is empty. Please enter a bot token first.\"}");
        return ESP_OK;
    }

    if (strlen(chat_id) == 0) {
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Chat ID is empty. Please enter your chat ID first.\"}");
        return ESP_OK;
    }

    // Send test message via Telegram API
    ESP_LOGI(TAG, "Testing Telegram bot, chat_id: %s", chat_id);

    // Build URL with just the method
    char url[256];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage", bot_token);

    // Build POST data
    char post_data[256];
    snprintf(post_data, sizeof(post_data),
        "chat_id=%s&text=Test%%20message%%20from%%20ESP32%%20Gateway", chat_id);

    ESP_LOGI(TAG, "POST to: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"❌ Failed to initialize HTTP client\"}");
        return ESP_OK;
    }

    // Set headers and POST data
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // Perform request
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP Status: %d", status);

        if (status == 200) {
            httpd_resp_sendstr(req, "{\"status\":\"success\",\"message\":\"✅ Test message sent successfully! Check your Telegram.\"}");
        } else if (status == 401) {
            httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"❌ Invalid bot token. Check your token from @BotFather.\"}");
        } else if (status == 400) {
            httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"❌ Invalid chat ID. Make sure it's correct.\"}");
        } else {
            char resp[128];
            snprintf(resp, sizeof(resp), "{\"status\":\"error\",\"message\":\"❌ HTTP Error %d\"}", status);
            httpd_resp_sendstr(req, resp);
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"❌ Network error - Check internet connection\"}");
    }

    esp_http_client_cleanup(client);
    return ESP_OK;
}

// Handler: /api/modbus_poll - Live Modbus polling (Modbus Poll style)
static esp_err_t api_modbus_poll_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    // Parse query parameters: slave, reg, qty, type
    char query[128];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Missing parameters\"}");
        return ESP_OK;
    }

    char slave_str[8], reg_str[8], qty_str[8], type_str[16];

    if (httpd_query_key_value(query, "slave", slave_str, sizeof(slave_str)) != ESP_OK ||
        httpd_query_key_value(query, "reg", reg_str, sizeof(reg_str)) != ESP_OK ||
        httpd_query_key_value(query, "qty", qty_str, sizeof(qty_str)) != ESP_OK ||
        httpd_query_key_value(query, "type", type_str, sizeof(type_str)) != ESP_OK) {
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Invalid parameters\"}");
        return ESP_OK;
    }

    uint8_t slave_id = atoi(slave_str);
    uint16_t start_reg = atoi(reg_str);
    uint16_t quantity = atoi(qty_str);

    if (slave_id < 1 || slave_id > 247 || start_reg > 65535 || quantity < 1 || quantity > 20) {
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Parameter out of range\"}");
        return ESP_OK;
    }

    // Read Modbus registers
    modbus_result_t result;
    if (strcmp(type_str, "holding") == 0) {
        result = modbus_read_holding_registers(slave_id, start_reg, quantity);
    } else {
        result = modbus_read_input_registers(slave_id, start_reg, quantity);
    }

    if (result != MODBUS_SUCCESS) {
        char resp[128];
        const char *error_msg = "Unknown error";
        switch (result) {
            case MODBUS_TIMEOUT: error_msg = "Timeout - No response from slave"; break;
            case MODBUS_INVALID_CRC: error_msg = "Invalid CRC"; break;
            case MODBUS_ILLEGAL_FUNCTION: error_msg = "Illegal function"; break;
            case MODBUS_ILLEGAL_DATA_ADDRESS: error_msg = "Illegal register address"; break;
            case MODBUS_ILLEGAL_DATA_VALUE: error_msg = "Illegal data value"; break;
            default: break;
        }
        snprintf(resp, sizeof(resp), "{\"status\":\"error\",\"message\":\"Modbus Error (Slave %d): %s\"}",
                 slave_id, error_msg);
        httpd_resp_sendstr(req, resp);
        return ESP_OK;
    }

    // Build JSON response with register values
    char response[1024];
    int offset = snprintf(response, sizeof(response),
        "{\"status\":\"success\",\"slave\":%d,\"register\":%d,\"quantity\":%d,\"type\":\"%s\",\"values\":[",
        slave_id, start_reg, quantity, type_str);

    // Get values from response buffer
    for (int i = 0; i < quantity && offset < sizeof(response) - 50; i++) {
        uint16_t value = modbus_get_response_buffer(i);
        offset += snprintf(response + offset, sizeof(response) - offset,
            "%s%u", i > 0 ? "," : "", value);
    }

    offset += snprintf(response + offset, sizeof(response) - offset, "]}");

    httpd_resp_sendstr(req, response);
    return ESP_OK;
}

// Background task for SIM testing
static void sim_test_task(void *pvParameters) {
    ESP_LOGI(TAG, "SIM test task started");

    // Build modem configuration from system config
    ppp_config_t modem_config = {
        .apn = g_system_config.sim_config.apn,
        .user = g_system_config.sim_config.apn_user,
        .pass = g_system_config.sim_config.apn_pass,
        .uart_num = g_system_config.sim_config.uart_num,
        .tx_pin = g_system_config.sim_config.uart_tx_pin,
        .rx_pin = g_system_config.sim_config.uart_rx_pin,
        .pwr_pin = g_system_config.sim_config.pwr_pin,
        .reset_pin = g_system_config.sim_config.reset_pin,
        .baud_rate = g_system_config.sim_config.uart_baud_rate
    };

    // Initialize modem
    esp_err_t init_ret = a7670c_ppp_init(&modem_config);
    if (init_ret != ESP_OK) {
        xSemaphoreTake(g_sim_test_mutex, portMAX_DELAY);
        g_sim_test_status.in_progress = false;
        g_sim_test_status.completed = true;
        g_sim_test_status.success = false;
        snprintf(g_sim_test_status.error, sizeof(g_sim_test_status.error),
                 "Failed to initialize modem UART");
        xSemaphoreGive(g_sim_test_mutex);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Modem initialized, waiting 10 seconds...");
    vTaskDelay(pdMS_TO_TICKS(10000));

    // Try to connect PPP (signal strength is checked during this process)
    esp_err_t ppp_ret = a7670c_ppp_connect();

    if (ppp_ret == ESP_OK) {
        ESP_LOGI(TAG, "PPP connecting, waiting for IP...");
        int wait_count = 0;
        bool got_ip = false;
        char ip_str[32] = "";

        while (wait_count < 20 && !got_ip) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            wait_count++;

            if (a7670c_ppp_is_connected()) {
                if (a7670c_ppp_get_ip_info(ip_str, sizeof(ip_str)) == ESP_OK) {
                    got_ip = true;
                    ESP_LOGI(TAG, "Got IP: %s", ip_str);
                    break;
                }
            }
        }

        // Get stored signal strength (checked during PPP init before entering PPP mode)
        signal_strength_t signal;
        memset(&signal, 0, sizeof(signal));
        esp_err_t signal_ret = a7670c_get_stored_signal_strength(&signal);

        xSemaphoreTake(g_sim_test_mutex, portMAX_DELAY);
        g_sim_test_status.in_progress = false;
        g_sim_test_status.completed = true;

        if (got_ip) {
            g_sim_test_status.success = true;
            strncpy(g_sim_test_status.ip, ip_str, sizeof(g_sim_test_status.ip) - 1);

            // Store signal info (retrieved during PPP init)
            if (signal_ret == ESP_OK) {
                g_sim_test_status.signal = signal.rssi_dbm;
                if (signal.quality != NULL) {
                    strncpy(g_sim_test_status.signal_quality, signal.quality, sizeof(g_sim_test_status.signal_quality) - 1);
                }
                strncpy(g_sim_test_status.operator_name, signal.operator_name, sizeof(g_sim_test_status.operator_name) - 1);
            } else {
                g_sim_test_status.signal = 0;
                strncpy(g_sim_test_status.signal_quality, "Unknown", sizeof(g_sim_test_status.signal_quality) - 1);
                strncpy(g_sim_test_status.operator_name, "Unknown", sizeof(g_sim_test_status.operator_name) - 1);
            }
            strncpy(g_sim_test_status.apn, g_system_config.sim_config.apn, sizeof(g_sim_test_status.apn) - 1);
        } else {
            g_sim_test_status.success = false;
            snprintf(g_sim_test_status.error, sizeof(g_sim_test_status.error),
                     "Timeout waiting for IP address");

            // Store signal info even on failure
            if (signal_ret == ESP_OK) {
                g_sim_test_status.signal = signal.rssi_dbm;
                strncpy(g_sim_test_status.operator_name, signal.operator_name, sizeof(g_sim_test_status.operator_name) - 1);
            }
        }
        xSemaphoreGive(g_sim_test_mutex);
    } else {
        // PPP connection failed - try to get signal anyway
        signal_strength_t signal;
        memset(&signal, 0, sizeof(signal));
        esp_err_t signal_ret = a7670c_get_stored_signal_strength(&signal);

        xSemaphoreTake(g_sim_test_mutex, portMAX_DELAY);
        g_sim_test_status.in_progress = false;
        g_sim_test_status.completed = true;
        g_sim_test_status.success = false;
        snprintf(g_sim_test_status.error, sizeof(g_sim_test_status.error),
                 "PPP connection failed: %s", esp_err_to_name(ppp_ret));

        if (signal_ret == ESP_OK && signal.rssi_dbm != 0) {
            g_sim_test_status.signal = signal.rssi_dbm;
            if (signal.quality != NULL) {
                strncpy(g_sim_test_status.signal_quality, signal.quality, sizeof(g_sim_test_status.signal_quality) - 1);
            }
            strncpy(g_sim_test_status.operator_name, signal.operator_name, sizeof(g_sim_test_status.operator_name) - 1);
        }
        xSemaphoreGive(g_sim_test_mutex);
    }

    // Cleanup - disconnect and deinitialize modem
    // CRITICAL: Must wait long enough for LWIP timers to expire to prevent crash
    ESP_LOGI(TAG, "Cleaning up SIM test - disconnecting PPP...");
    a7670c_ppp_disconnect();

    // Wait for PPP/LWIP stack to fully settle before deinit
    ESP_LOGI(TAG, "Waiting for PPP stack to settle...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Deinitializing modem...");
    a7670c_ppp_deinit();
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "SIM test task completed, modem deinitialized");

    vTaskDelete(NULL);
}

// Handler: /api/sim_test - Start SIM test in background
static esp_err_t api_sim_test_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    // Initialize mutex if needed
    if (g_sim_test_mutex == NULL) {
        g_sim_test_mutex = xSemaphoreCreateMutex();
    }

    // Check if test already in progress
    xSemaphoreTake(g_sim_test_mutex, portMAX_DELAY);
    if (g_sim_test_status.in_progress) {
        xSemaphoreGive(g_sim_test_mutex);
        httpd_resp_sendstr(req, "{\"status\":\"in_progress\",\"message\":\"Test already running\"}");
        return ESP_OK;
    }

    // Reset status and start test
    memset(&g_sim_test_status, 0, sizeof(g_sim_test_status));
    g_sim_test_status.in_progress = true;
    xSemaphoreGive(g_sim_test_mutex);

    // Create background task
    xTaskCreate(sim_test_task, "sim_test", 8192, NULL, 5, NULL);

    httpd_resp_sendstr(req, "{\"status\":\"started\",\"message\":\"SIM test started\"}");
    return ESP_OK;
}

// Handler: /api/sim_test_status - Get current test status
static esp_err_t api_sim_test_status_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    if (g_sim_test_mutex == NULL) {
        httpd_resp_sendstr(req, "{\"status\":\"not_started\"}");
        return ESP_OK;
    }

    xSemaphoreTake(g_sim_test_mutex, portMAX_DELAY);

    char response[1024];
    if (g_sim_test_status.in_progress) {
        snprintf(response, sizeof(response), "{\"status\":\"in_progress\"}");
    } else if (g_sim_test_status.completed) {
        if (g_sim_test_status.success) {
            snprintf(response, sizeof(response),
                     "{\"status\":\"completed\",\"success\":true,"
                     "\"ip\":\"%s\",\"signal\":%d,\"signal_quality\":\"%s\","
                     "\"operator\":\"%s\",\"apn\":\"%s\"}",
                     g_sim_test_status.ip,
                     g_sim_test_status.signal,
                     g_sim_test_status.signal_quality,
                     g_sim_test_status.operator_name,
                     g_sim_test_status.apn);
        } else {
            snprintf(response, sizeof(response),
                     "{\"status\":\"completed\",\"success\":false,\"error\":\"%s\","
                     "\"signal\":%d,\"operator\":\"%s\"}",
                     g_sim_test_status.error,
                     g_sim_test_status.signal,
                     g_sim_test_status.operator_name);
        }
    } else {
        snprintf(response, sizeof(response), "{\"status\":\"not_started\"}");
    }

    xSemaphoreGive(g_sim_test_mutex);

    httpd_resp_sendstr(req, response);
    return ESP_OK;
}

// Handler: /api/sd_status - Get SD card status
static esp_err_t api_sd_status_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    sd_card_status_t status;
    esp_err_t ret = sd_card_get_status(&status);

    if (ret == ESP_OK && status.initialized && status.card_available) {
        uint32_t pending_count = 0;
        sd_card_get_pending_count(&pending_count);

        char response[256];
        snprintf(response, sizeof(response),
                 "{\"mounted\":true,\"size_mb\":%llu,\"free_mb\":%llu,\"cached_messages\":%lu}",
                 status.card_size_mb,
                 status.free_space_mb,
                 (unsigned long)pending_count);
        httpd_resp_sendstr(req, response);
    } else {
        httpd_resp_sendstr(req, "{\"mounted\":false}");
    }
    return ESP_OK;
}

// Handler: /api/sd_clear - Clear cached messages
static esp_err_t api_sd_clear_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    int count = 0;
    esp_err_t ret = sd_card_clear_all_messages();

    if (ret == ESP_OK) {
        char response[128];
        snprintf(response, sizeof(response), "{\"success\":true,\"count\":%d}", count);
        httpd_resp_sendstr(req, response);
    } else {
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Failed to clear messages\"}");
    }
    return ESP_OK;
}

// Handler: /api/sd_replay - Replay cached messages
static esp_err_t api_sd_replay_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    // This would need access to the replay callback from main.c
    // For now, return a placeholder response
    httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Replay not implemented in web interface\"}");
    return ESP_OK;
}

// Handler: /api/rtc_time - Get RTC time
static esp_err_t api_rtc_time_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    time_t rtc_time;
    float temperature;
    esp_err_t ret = ds3231_get_time(&rtc_time);

    if (ret == ESP_OK) {
        ds3231_get_temperature(&temperature);
        struct tm timeinfo;
        localtime_r(&rtc_time, &timeinfo);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

        char response[256];
        snprintf(response, sizeof(response),
                 "{\"success\":true,\"time\":\"%s\",\"temp\":%.1f}",
                 time_str, temperature);
        httpd_resp_sendstr(req, response);
    } else {
        httpd_resp_sendstr(req, "{\"success\":false}");
    }
    return ESP_OK;
}

// Handler: /api/rtc_sync - Sync RTC from NTP
static esp_err_t api_rtc_sync_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    // Get current system time (should be synced from NTP)
    time_t now;
    time(&now);

    esp_err_t ret = ds3231_set_time(now);

    if (ret == ESP_OK) {
        httpd_resp_sendstr(req, "{\"success\":true}");
    } else {
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Failed to update RTC\"}");
    }
    return ESP_OK;
}

// Handler: /api/rtc_set - Set system time from RTC
static esp_err_t api_rtc_set_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    esp_err_t ret = ds3231_sync_system_time();

    if (ret == ESP_OK) {
        httpd_resp_sendstr(req, "{\"success\":true}");
    } else {
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Failed to sync system time\"}");
    }
    return ESP_OK;
}

// Handler: /api/modbus/status - Get Modbus communication statistics
static esp_err_t api_modbus_status_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    // Get Modbus statistics
    modbus_stats_t stats;
    modbus_get_statistics(&stats);

    // Calculate success rate
    float success_rate = 0.0f;
    if (stats.total_requests > 0) {
        success_rate = (float)stats.successful_requests / (float)stats.total_requests * 100.0f;
    }

    // Get current time for timestamp
    int64_t current_time = esp_timer_get_time() / 1000000;

    char json_response[512];
    snprintf(json_response, sizeof(json_response),
        "{"
        "\"total_reads\":%lu,"
        "\"successful_reads\":%lu,"
        "\"failed_reads\":%lu,"
        "\"success_rate\":%.2f,"
        "\"crc_errors\":%lu,"
        "\"timeout_errors\":%lu,"
        "\"last_error_code\":%lu,"
        "\"sensors_configured\":%d,"
        "\"timestamp\":%lld"
        "}",
        (unsigned long)stats.total_requests,
        (unsigned long)stats.successful_requests,
        (unsigned long)stats.failed_requests,
        success_rate,
        (unsigned long)stats.crc_errors,
        (unsigned long)stats.timeout_errors,
        (unsigned long)stats.last_error_code,
        g_system_config.sensor_count,
        (long long)current_time
    );

    httpd_resp_sendstr(req, json_response);
    return ESP_OK;
}

// Handler: /api/azure/status - Get Azure IoT Hub connection status
static esp_err_t api_azure_status_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    // Get current time
    int64_t current_time = esp_timer_get_time() / 1000000;

    // Calculate connection uptime
    int64_t connection_uptime = 0;
    if (mqtt_connected && mqtt_connect_time > 0) {
        connection_uptime = current_time - mqtt_connect_time;
    }

    // Calculate time since last telemetry
    int64_t time_since_telemetry = 0;
    if (last_telemetry_time > 0) {
        time_since_telemetry = current_time - last_telemetry_time;
    }

    char json_response[512];
    snprintf(json_response, sizeof(json_response),
        "{"
        "\"connection_state\":\"%s\","
        "\"connection_uptime\":%lld,"
        "\"messages_sent\":%lu,"
        "\"reconnect_attempts\":%lu,"
        "\"last_telemetry_ago\":%lld,"
        "\"telemetry_interval\":%d,"
        "\"hub_name\":\"%s\","
        "\"device_id\":\"%s\","
        "\"timestamp\":%lld"
        "}",
        mqtt_connected ? "connected" : "disconnected",
        (long long)connection_uptime,
        (unsigned long)total_telemetry_sent,
        (unsigned long)mqtt_reconnect_count,
        (long long)time_since_telemetry,
        g_system_config.telemetry_interval,
        g_system_config.azure_hub_fqdn,
        g_system_config.azure_device_id,
        (long long)current_time
    );

    httpd_resp_sendstr(req, json_response);
    return ESP_OK;
}

// Azure telemetry history handler (for web interface)
static esp_err_t api_telemetry_history_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    // Allocate buffer for telemetry history JSON (25 messages * ~450 bytes each)
    char *history_buffer = (char *)malloc(12000);
    if (history_buffer == NULL) {
        const char* error_response = "{\"error\":\"Out of memory\"}";
        httpd_resp_sendstr(req, error_response);
        return ESP_OK;
    }

    // Get telemetry history from main.c
    int written = get_telemetry_history_json(history_buffer, 12000);

    if (written > 0) {
        httpd_resp_sendstr(req, history_buffer);
    } else {
        const char* empty_response = "[]";
        httpd_resp_sendstr(req, empty_response);
    }

    free(history_buffer);
    return ESP_OK;
}

// Modbus Explorer: Device Scanner Handler
static esp_err_t modbus_scan_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    char content[512];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Failed to receive request\"}");
        return ESP_FAIL;
    }
    content[ret] = '\0';

    // Parse parameters
    int start_id = 1, end_id = 10, test_register = 0;
    char reg_type[16] = "holding";

    char *param = strstr(content, "start_id=");
    if (param) start_id = atoi(param + 9);

    param = strstr(content, "end_id=");
    if (param) end_id = atoi(param + 7);

    param = strstr(content, "test_register=");
    if (param) test_register = atoi(param + 14);

    param = strstr(content, "reg_type=");
    if (param) {
        sscanf(param + 9, "%15[^&]", reg_type);
    }

    // Validate range
    if (start_id < 1 || start_id > 247 || end_id < 1 || end_id > 247 || start_id > end_id) {
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Invalid slave ID range\"}");
        return ESP_OK;
    }

    // Build JSON response with discovered devices
    char json_response[2048] = "{\"status\":\"success\",\"devices\":[";
    bool first = true;

    for (int slave_id = start_id; slave_id <= end_id; slave_id++) {
        modbus_result_t result;

        if (strcmp(reg_type, "input") == 0) {
            result = modbus_read_input_registers(slave_id, test_register, 1);
        } else {
            result = modbus_read_holding_registers(slave_id, test_register, 1);
        }

        if (result == MODBUS_SUCCESS) {
            char device_entry[128];
            snprintf(device_entry, sizeof(device_entry),
                     "%s{\"slave_id\":%d,\"responsive\":true}",
                     first ? "" : ",", slave_id);
            strcat(json_response, device_entry);
            first = false;
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // Small delay between scans
    }

    strcat(json_response, "]}");
    httpd_resp_sendstr(req, json_response);
    return ESP_OK;
}

// Modbus Explorer: Live Register Reader Handler
static esp_err_t modbus_read_live_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    char content[512];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Failed to receive request\"}");
        return ESP_FAIL;
    }
    content[ret] = '\0';

    // Parse parameters
    int slave_id = 1, start_register = 0, quantity = 2;
    char reg_type[16] = "holding";

    char *param = strstr(content, "slave_id=");
    if (param) slave_id = atoi(param + 9);

    param = strstr(content, "start_register=");
    if (param) start_register = atoi(param + 15);

    param = strstr(content, "quantity=");
    if (param) quantity = atoi(param + 9);

    param = strstr(content, "reg_type=");
    if (param) {
        sscanf(param + 9, "%15[^&]", reg_type);
    }

    // Validate parameters
    if (slave_id < 1 || slave_id > 247 || quantity < 1 || quantity > 10) {
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Invalid parameters\"}");
        return ESP_OK;
    }

    // Read registers
    modbus_result_t result;
    if (strcmp(reg_type, "input") == 0) {
        result = modbus_read_input_registers(slave_id, start_register, quantity);
    } else {
        result = modbus_read_holding_registers(slave_id, start_register, quantity);
    }

    if (result != MODBUS_SUCCESS) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
                 "{\"status\":\"error\",\"message\":\"Modbus read failed\",\"error_code\":%d}", result);
        httpd_resp_sendstr(req, error_msg);
        return ESP_OK;
    }

    // Build JSON response with all format interpretations
    char json_response[3072] = "{\"status\":\"success\",\"formats\":{";
    char temp[256];

    // Get register values
    uint16_t registers[10];
    for (int i = 0; i < quantity; i++) {
        registers[i] = modbus_get_response_buffer(i);
    }

    // Raw hex data
    strcat(json_response, "\"hex_string\":\"");
    for (int i = 0; i < quantity; i++) {
        char hex_reg[8];
        snprintf(hex_reg, sizeof(hex_reg), "%04X", registers[i]);
        strcat(json_response, hex_reg);
    }
    strcat(json_response, "\",");

    // UINT16 and INT16 formats (from first register)
    if (quantity >= 1) {
        uint16_t uint16_be = registers[0];
        uint16_t uint16_le = ((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF);
        int16_t int16_be = (int16_t)registers[0];
        int16_t int16_le = (int16_t)uint16_le;

        snprintf(temp, sizeof(temp),
                 "\"uint16_be\":%u,\"uint16_le\":%u,\"int16_be\":%d,\"int16_le\":%d,",
                 uint16_be, uint16_le, int16_be, int16_le);
        strcat(json_response, temp);
    }

    // 32-bit formats (if we have at least 2 registers)
    if (quantity >= 2) {
        // UINT32 formats
        uint32_t uint32_abcd = ((uint32_t)registers[0] << 16) | registers[1];
        uint32_t uint32_dcba = ((uint32_t)(((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF)) << 16) |
                               (((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF));
        uint32_t uint32_badc = ((uint32_t)registers[1] << 16) | registers[0];
        uint32_t uint32_cdab = ((uint32_t)(((registers[0] & 0xFF) << 8) | ((registers[0] >> 8) & 0xFF)) << 16) |
                               (((registers[1] & 0xFF) << 8) | ((registers[1] >> 8) & 0xFF));

        snprintf(temp, sizeof(temp),
                 "\"uint32_abcd\":%u,\"uint32_dcba\":%u,\"uint32_badc\":%u,\"uint32_cdab\":%u,",
                 (unsigned int)uint32_abcd, (unsigned int)uint32_dcba,
                 (unsigned int)uint32_badc, (unsigned int)uint32_cdab);
        strcat(json_response, temp);

        // INT32 formats
        int32_t int32_abcd = (int32_t)uint32_abcd;
        int32_t int32_dcba = (int32_t)uint32_dcba;
        int32_t int32_badc = (int32_t)uint32_badc;
        int32_t int32_cdab = (int32_t)uint32_cdab;

        snprintf(temp, sizeof(temp),
                 "\"int32_abcd\":%d,\"int32_dcba\":%d,\"int32_badc\":%d,\"int32_cdab\":%d,",
                 (int)int32_abcd, (int)int32_dcba, (int)int32_badc, (int)int32_cdab);
        strcat(json_response, temp);

        // FLOAT32 formats
        union { uint32_t u; float f; } float_conv;

        float_conv.u = uint32_abcd;
        float float32_abcd = float_conv.f;
        float_conv.u = uint32_dcba;
        float float32_dcba = float_conv.f;
        float_conv.u = uint32_badc;
        float float32_badc = float_conv.f;
        float_conv.u = uint32_cdab;
        float float32_cdab = float_conv.f;

        snprintf(temp, sizeof(temp),
                 "\"float32_abcd\":%.6f,\"float32_dcba\":%.6f,\"float32_badc\":%.6f,\"float32_cdab\":%.6f",
                 float32_abcd, float32_dcba, float32_badc, float32_cdab);
        strcat(json_response, temp);
    }

    strcat(json_response, "}}");
    httpd_resp_sendstr(req, json_response);
    return ESP_OK;
}

// ============================================================================
// End of REST API Handlers
// ============================================================================

// Start AP mode for normal operation
esp_err_t web_config_start_ap_mode(void)
{
    ESP_LOGI(TAG, "Initializing WiFi infrastructure (network mode: %s)",
             g_system_config.network_mode == NETWORK_MODE_WIFI ? "WiFi" : "SIM Module");

    // Initialize WiFi infrastructure - allow ESP_ERR_INVALID_STATE for reinit
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create STA interface - this is required for WiFi initialization
    // Check if STA interface already exists to prevent double initialization
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif == NULL) {
        sta_netif = esp_netif_create_default_wifi_sta();
        ESP_LOGI(TAG, "Created new WiFi STA interface");
    } else {
        ESP_LOGI(TAG, "WiFi STA interface already exists - reusing");
    }

    // Initialize WiFi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize WiFi driver: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register event handlers (allow ESP_ERR_INVALID_ARG if already registered)
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure WiFi STA with credentials from NVS
    wifi_config_t sta_config = {
        .sta = {
            .ssid = "",
            .password = "",
        },
    };

    strncpy((char*)sta_config.sta.ssid, g_system_config.wifi_ssid, sizeof(sta_config.sta.ssid));
    strncpy((char*)sta_config.sta.password, g_system_config.wifi_password, sizeof(sta_config.sta.password));

    // Set WiFi mode to STA
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi STA mode: %s", esp_err_to_name(ret));
        return ret;
    }

    // Apply WiFi configuration
    ret = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi STA config: %s", esp_err_to_name(ret));
        return ret;
    }

    // Only start WiFi in WiFi mode - in SIM mode, leave it initialized but stopped
    if (g_system_config.network_mode == NETWORK_MODE_WIFI) {
        ESP_LOGI(TAG, "Starting WiFi in STA mode...");
        ret = esp_wifi_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
            return ret;
        }

        // Log connection status
        if (strlen(g_system_config.wifi_ssid) == 0) {
            ESP_LOGW(TAG, "WiFi SSID not configured - WiFi started but not connecting");
            ESP_LOGI(TAG, "[TIP] Use GPIO trigger to start web config AP mode");
        } else {
            ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", g_system_config.wifi_ssid);
        }
    } else {
        ESP_LOGI(TAG, "SIM mode: WiFi driver initialized but not started");
        ESP_LOGI(TAG, "💡 WiFi will start when you trigger web config mode (GPIO pin)");
    }

    return ESP_OK;
}

// Configuration management functions
esp_err_t config_load_from_nvs(system_config_t *config)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("config", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        config_reset_to_defaults();
        return err;
    }
    
    size_t required_size = sizeof(system_config_t);
    err = nvs_get_blob(nvs_handle, "system", config, &required_size);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load config from NVS: %s", esp_err_to_name(err));
        config_reset_to_defaults();
    } else {
        ESP_LOGI(TAG, "Configuration loaded from NVS");
        
        // Migrate old format data type names to new format
        bool migration_needed = false;
        for (int i = 0; i < config->sensor_count; i++) {
            char *data_type = config->sensors[i].data_type;
            
            // Migrate FLOAT32 formats
            if (strcmp(data_type, "FLOAT32_ABCD") == 0) {
                strncpy(data_type, "FLOAT32_1234", sizeof(config->sensors[i].data_type) - 1);
                data_type[sizeof(config->sensors[i].data_type) - 1] = '\0';
                migration_needed = true;
                ESP_LOGI(TAG, "Migrated sensor %d: FLOAT32_ABCD -> FLOAT32_1234", i);
            } else if (strcmp(data_type, "FLOAT32_DCBA") == 0) {
                strncpy(data_type, "FLOAT32_4321", sizeof(config->sensors[i].data_type) - 1);
                data_type[sizeof(config->sensors[i].data_type) - 1] = '\0';
                migration_needed = true;
                ESP_LOGI(TAG, "Migrated sensor %d: FLOAT32_DCBA -> FLOAT32_4321", i);
            } else if (strcmp(data_type, "FLOAT32_BADC") == 0) {
                strncpy(data_type, "FLOAT32_2143", sizeof(config->sensors[i].data_type) - 1);
                data_type[sizeof(config->sensors[i].data_type) - 1] = '\0';
                migration_needed = true;
                ESP_LOGI(TAG, "Migrated sensor %d: FLOAT32_BADC -> FLOAT32_2143", i);
            } else if (strcmp(data_type, "FLOAT32_CDAB") == 0) {
                strncpy(data_type, "FLOAT32_3412", sizeof(config->sensors[i].data_type) - 1);
                data_type[sizeof(config->sensors[i].data_type) - 1] = '\0';
                migration_needed = true;
                ESP_LOGI(TAG, "Migrated sensor %d: FLOAT32_CDAB -> FLOAT32_3412", i);
            }
            // Migrate INT32 formats
            else if (strcmp(data_type, "INT32_ABCD") == 0) {
                strncpy(data_type, "INT32_1234", sizeof(config->sensors[i].data_type) - 1);
                data_type[sizeof(config->sensors[i].data_type) - 1] = '\0';
                migration_needed = true;
                ESP_LOGI(TAG, "Migrated sensor %d: INT32_ABCD -> INT32_1234", i);
            } else if (strcmp(data_type, "INT32_DCBA") == 0) {
                strncpy(data_type, "INT32_4321", sizeof(config->sensors[i].data_type) - 1);
                data_type[sizeof(config->sensors[i].data_type) - 1] = '\0';
                migration_needed = true;
                ESP_LOGI(TAG, "Migrated sensor %d: INT32_DCBA -> INT32_4321", i);
            } else if (strcmp(data_type, "INT32_BADC") == 0) {
                strncpy(data_type, "INT32_2143", sizeof(config->sensors[i].data_type) - 1);
                data_type[sizeof(config->sensors[i].data_type) - 1] = '\0';
                migration_needed = true;
                ESP_LOGI(TAG, "Migrated sensor %d: INT32_BADC -> INT32_2143", i);
            } else if (strcmp(data_type, "INT32_CDAB") == 0) {
                strncpy(data_type, "INT32_3412", sizeof(config->sensors[i].data_type) - 1);
                data_type[sizeof(config->sensors[i].data_type) - 1] = '\0';
                migration_needed = true;
                ESP_LOGI(TAG, "Migrated sensor %d: INT32_CDAB -> INT32_3412", i);
            }
            // Migrate UINT32 formats
            else if (strcmp(data_type, "UINT32_ABCD") == 0) {
                strncpy(data_type, "UINT32_1234", sizeof(config->sensors[i].data_type) - 1);
                data_type[sizeof(config->sensors[i].data_type) - 1] = '\0';
                migration_needed = true;
                ESP_LOGI(TAG, "Migrated sensor %d: UINT32_ABCD -> UINT32_1234", i);
            } else if (strcmp(data_type, "UINT32_DCBA") == 0) {
                strncpy(data_type, "UINT32_4321", sizeof(config->sensors[i].data_type) - 1);
                data_type[sizeof(config->sensors[i].data_type) - 1] = '\0';
                migration_needed = true;
                ESP_LOGI(TAG, "Migrated sensor %d: UINT32_DCBA -> UINT32_4321", i);
            } else if (strcmp(data_type, "UINT32_BADC") == 0) {
                strncpy(data_type, "UINT32_2143", sizeof(config->sensors[i].data_type) - 1);
                data_type[sizeof(config->sensors[i].data_type) - 1] = '\0';
                migration_needed = true;
                ESP_LOGI(TAG, "Migrated sensor %d: UINT32_BADC -> UINT32_2143", i);
            } else if (strcmp(data_type, "UINT32_CDAB") == 0) {
                strncpy(data_type, "UINT32_3412", sizeof(config->sensors[i].data_type) - 1);
                data_type[sizeof(config->sensors[i].data_type) - 1] = '\0';
                migration_needed = true;
                ESP_LOGI(TAG, "Migrated sensor %d: UINT32_CDAB -> UINT32_3412", i);
            }
            // Migrate 16-bit formats
            else if (strcmp(data_type, "UINT16_BE") == 0) {
                strncpy(data_type, "UINT16_HI", sizeof(config->sensors[i].data_type) - 1);
                data_type[sizeof(config->sensors[i].data_type) - 1] = '\0';
                migration_needed = true;
                ESP_LOGI(TAG, "Migrated sensor %d: UINT16_BE -> UINT16_HI", i);
            } else if (strcmp(data_type, "UINT16_LE") == 0) {
                strncpy(data_type, "UINT16_LO", sizeof(config->sensors[i].data_type) - 1);
                data_type[sizeof(config->sensors[i].data_type) - 1] = '\0';
                migration_needed = true;
                ESP_LOGI(TAG, "Migrated sensor %d: UINT16_LE -> UINT16_LO", i);
            } else if (strcmp(data_type, "INT16_BE") == 0) {
                strncpy(data_type, "INT16_HI", sizeof(config->sensors[i].data_type) - 1);
                data_type[sizeof(config->sensors[i].data_type) - 1] = '\0';
                migration_needed = true;
                ESP_LOGI(TAG, "Migrated sensor %d: INT16_BE -> INT16_HI", i);
            } else if (strcmp(data_type, "INT16_LE") == 0) {
                strncpy(data_type, "INT16_LO", sizeof(config->sensors[i].data_type) - 1);
                data_type[sizeof(config->sensors[i].data_type) - 1] = '\0';
                migration_needed = true;
                ESP_LOGI(TAG, "Migrated sensor %d: INT16_LE -> INT16_LO", i);
            }
        }
        
        // Save migrated configuration if changes were made
        if (migration_needed) {
            ESP_LOGI(TAG, "Saving migrated configuration to NVS");
            nvs_close(nvs_handle);
            config_save_to_nvs(config);
            return ESP_OK;
        }
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t config_save_to_nvs(const system_config_t *config)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("config", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    // First attempt to save configuration
    err = nvs_set_blob(nvs_handle, "system", config, sizeof(system_config_t));
    
    if (err == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
        ESP_LOGW(TAG, "[CONFIG] NVS space insufficient (%d bytes needed), attempting cleanup and retry", sizeof(system_config_t));
        
        // Strategy 1: Erase the old blob and retry
        ESP_LOGI(TAG, "🧹 Erasing old configuration blob to free space");
        nvs_erase_key(nvs_handle, "system");
        err = nvs_set_blob(nvs_handle, "system", config, sizeof(system_config_t));
        
        if (err == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
            ESP_LOGW(TAG, "[PROC] Still insufficient space after cleanup, performing NVS defragmentation");
            nvs_close(nvs_handle);
            
            // Strategy 2: Full NVS cleanup and defragmentation
            ESP_LOGI(TAG, "[DEL] Performing full NVS partition cleanup");
            err = nvs_flash_erase_partition("nvs");
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "[ERROR] Failed to erase NVS partition: %s", esp_err_to_name(err));
                return err;
            }
            
            err = nvs_flash_init_partition("nvs");
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "[ERROR] Failed to reinitialize NVS partition: %s", esp_err_to_name(err));
                return err;
            }
            
            // Retry save after cleanup
            err = nvs_open("config", NVS_READWRITE, &nvs_handle);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "[PROC] Retrying configuration save after NVS cleanup");
                err = nvs_set_blob(nvs_handle, "system", config, sizeof(system_config_t));
                
                if (err == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
                    ESP_LOGE(TAG, "[ERROR] Configuration too large even after NVS cleanup. Size: %d bytes", sizeof(system_config_t));
                    // Log configuration analysis for debugging
                    ESP_LOGI(TAG, "[DATA] Config Analysis - Sensors: %d, Total size: %d bytes", config->sensor_count, sizeof(system_config_t));
                    for (int i = 0; i < config->sensor_count && i < 8; i++) {
                        if (config->sensors[i].enabled) {
                            ESP_LOGI(TAG, "   Sensor[%d]: %s (%s) - Sub-sensors: %d", 
                                    i, config->sensors[i].name, config->sensors[i].sensor_type, config->sensors[i].sub_sensor_count);
                        }
                    }
                }
            }
        }
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[ERROR] Failed to save config to NVS: %s", esp_err_to_name(err));
        
        // Log NVS statistics for debugging
        nvs_stats_t nvs_stats;
        if (nvs_get_stats(NULL, &nvs_stats) == ESP_OK) {
            ESP_LOGI(TAG, "[STATS] NVS Stats - Used: %d, Free: %d, Total: %d entries", 
                     nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);
        }
    } else {
        ESP_LOGI(TAG, "[OK] Configuration saved to NVS successfully (%d bytes)", sizeof(system_config_t));
        nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t config_reset_to_defaults(void)
{
    memset(&g_system_config, 0, sizeof(system_config_t));
    
    // Set default values
    strcpy(g_system_config.wifi_ssid, "");
    strcpy(g_system_config.wifi_password, "");
    strcpy(g_system_config.azure_hub_fqdn, "your-hub.azure-devices.net");
    strcpy(g_system_config.azure_device_id, "your-device-id");
    strcpy(g_system_config.azure_device_key, "");
    g_system_config.telemetry_interval = 300;  // 5 minutes (300 seconds)
    g_system_config.sensor_count = 0;
    g_system_config.config_complete = false;
    g_system_config.modem_reset_enabled = false;
    g_system_config.modem_boot_delay = 15;
    g_system_config.modem_reset_gpio_pin = 2;
    

    // Network Mode defaults (NEW)
    g_system_config.network_mode = NETWORK_MODE_WIFI;  // Default to WiFi mode

    // SIM Module defaults (NEW)
    g_system_config.sim_config.enabled = false;
    strcpy(g_system_config.sim_config.apn, "airteliot");  // Default APN for Airtel (use "jionet" for Jio)
    strcpy(g_system_config.sim_config.apn_user, "");
    strcpy(g_system_config.sim_config.apn_pass, "");
    g_system_config.sim_config.uart_tx_pin = GPIO_NUM_33;
    g_system_config.sim_config.uart_rx_pin = GPIO_NUM_32;
    g_system_config.sim_config.pwr_pin = GPIO_NUM_4;
    g_system_config.sim_config.reset_pin = GPIO_NUM_15;
    g_system_config.sim_config.uart_num = UART_NUM_1;
    g_system_config.sim_config.uart_baud_rate = 115200;

    // SD Card defaults (VSPI configuration - avoids boot pin conflicts)
    g_system_config.sd_config.enabled = true;   // ENABLED by default for production use
    g_system_config.sd_config.cache_on_failure = true;  // Auto-cache when network fails
    g_system_config.sd_config.mosi_pin = GPIO_NUM_23;  // VSPI MOSI
    g_system_config.sd_config.miso_pin = GPIO_NUM_19;  // VSPI MISO
    g_system_config.sd_config.clk_pin = GPIO_NUM_5;    // CLK
    g_system_config.sd_config.cs_pin = GPIO_NUM_15;    // CS
    g_system_config.sd_config.spi_host = SPI3_HOST;    // VSPI
    g_system_config.sd_config.max_message_size = 1024;
    g_system_config.sd_config.min_free_space_mb = 10;

    // RTC (DS3231) defaults (NEW)
    g_system_config.rtc_config.enabled = false;
    g_system_config.rtc_config.sda_pin = GPIO_NUM_21;
    g_system_config.rtc_config.scl_pin = GPIO_NUM_22;
    g_system_config.rtc_config.i2c_num = I2C_NUM_0;

    // Telegram Bot defaults
    g_system_config.telegram_config.enabled = false;
    strcpy(g_system_config.telegram_config.bot_token, "");
    strcpy(g_system_config.telegram_config.chat_id, "");
    g_system_config.telegram_config.alerts_enabled = true;
    g_system_config.telegram_config.startup_notification = true;
    g_system_config.telegram_config.poll_interval = 10;

    // Initialize all sensors with default values
    for (int i = 0; i < 8; i++) {
        g_system_config.sensors[i].enabled = false;
        strcpy(g_system_config.sensors[i].data_type, "UINT16_HI");
        strcpy(g_system_config.sensors[i].register_type, "HOLDING");
        strcpy(g_system_config.sensors[i].parity, "none");
        g_system_config.sensors[i].scale_factor = 1.0;
        g_system_config.sensors[i].quantity = 1;
        g_system_config.sensors[i].baud_rate = 9600;
        g_system_config.sensors[i].sub_sensor_count = 0;
        
        // Initialize sub-sensors with defaults
        for (int j = 0; j < 8; j++) {
            g_system_config.sensors[i].sub_sensors[j].enabled = false;
            strcpy(g_system_config.sensors[i].sub_sensors[j].data_type, "UINT16_HI");
            strcpy(g_system_config.sensors[i].sub_sensors[j].register_type, "HOLDING_REGISTER");
            g_system_config.sensors[i].sub_sensors[j].scale_factor = 1.0;
            g_system_config.sensors[i].sub_sensors[j].quantity = 1;
        }
    }
    
    ESP_LOGI(TAG, "Configuration reset to defaults");
    return ESP_OK;
}

// Getter functions
system_config_t* get_system_config(void)
{
    return &g_system_config;
}

config_state_t get_config_state(void)
{
    return g_config_state;
}

void set_config_state(config_state_t state)
{
    g_config_state = state;
}

bool web_config_needs_auto_start(void)
{
    bool needs_start = g_web_server_auto_start && g_config_state == CONFIG_STATE_SETUP;
    if (needs_start) {
        g_web_server_auto_start = false;  // Reset flag after checking
    }
    return needs_start;
}

// Connect to the configured WiFi network while maintaining SoftAP access
esp_err_t connect_to_wifi_network(void)
{
    ESP_LOGI(TAG, "[WIFI] Configuring ESP32 to connect to WiFi network: %s", g_system_config.wifi_ssid);
    
    // Check current WiFi mode to avoid unnecessary mode switches
    wifi_mode_t current_mode;
    esp_err_t ret = esp_wifi_get_mode(&current_mode);
    
    if (ret == ESP_OK && current_mode == WIFI_MODE_APSTA) {
        ESP_LOGI(TAG, "[WIFI] Already in AP+STA mode, skipping mode switch");
    } else {
        ESP_LOGI(TAG, "[WIFI] Switching to AP+STA mode to maintain web server access");
        ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[WIFI] Failed to set AP+STA mode: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    
    // Check if SoftAP configuration already exists to avoid unnecessary reconfig
    wifi_config_t current_ap_config;
    ret = esp_wifi_get_config(WIFI_IF_AP, &current_ap_config);
    
    if (ret == ESP_OK && strcmp((char*)current_ap_config.ap.ssid, "ModbusIoT-Config") == 0) {
        ESP_LOGI(TAG, "[WIFI] SoftAP already configured, skipping AP reconfiguration");
    } else {
        ESP_LOGI(TAG, "[WIFI] Configuring SoftAP to maintain web server access");
        wifi_config_t ap_config = {
            .ap = {
                .ssid = "ModbusIoT-Config",
                .ssid_len = strlen("ModbusIoT-Config"),
                .channel = 6,
                .password = "config123",
                .max_connection = 4,
                .authmode = WIFI_AUTH_WPA_WPA2_PSK,
                .ssid_hidden = 0,
                .beacon_interval = 100
            },
        };
        
        // Set AP configuration only if needed
        ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "[WIFI] Failed to set AP config: %s", esp_err_to_name(ret));
            // Continue anyway - might not be critical
        }
    }
    
    // Configure WiFi connection parameters for STA interface
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, g_system_config.wifi_ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, g_system_config.wifi_password, sizeof(wifi_config.sta.password) - 1);
    
    // Set WiFi configuration for STA interface
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[WIFI] Failed to set WiFi STA config: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "[WIFI] Starting WiFi connection to: %s", g_system_config.wifi_ssid);
    
    // Disconnect from current WiFi if connected
    esp_wifi_disconnect();
    // Feed watchdog during disconnect delay
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for clean disconnect
    
    // Connect to the new WiFi network
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[WIFI] Failed to initiate WiFi connection: %s", esp_err_to_name(ret));
        if (ret == ESP_ERR_WIFI_CONN) {
            ESP_LOGE(TAG, "[WIFI] Connection error - possible causes:");
            ESP_LOGE(TAG, "   - Incorrect password");
            ESP_LOGE(TAG, "   - Network not found");
            ESP_LOGE(TAG, "   - Network security mismatch");
        }
        return ret;
    }
    
    ESP_LOGI(TAG, "[WIFI] WiFi connection initiated to: %s", g_system_config.wifi_ssid);
    ESP_LOGI(TAG, "[WIFI] SoftAP 'ModbusIoT-Config' remains active for configuration access");
    ESP_LOGI(TAG, "[WIFI] Connection status will be available in web interface");
    
    // Wait longer for initial connection attempt - some networks take time
    ESP_LOGI(TAG, "[WIFI] Waiting up to 10 seconds for connection...");
    
    // Check connection status multiple times over 10 seconds
    for (int i = 0; i < 10; i++) {
        // Feed watchdog to prevent reset during long WiFi connection
        // Skip watchdog reset in HTTP handler (task not registered with watchdog)
        // esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));  // Wait 1 second
        
        wifi_ap_record_t ap_info;
        esp_err_t conn_status = esp_wifi_sta_get_ap_info(&ap_info);
        if (conn_status == ESP_OK) {
            ESP_LOGI(TAG, "[WIFI] ✅ Successfully connected to: %s after %d seconds", ap_info.ssid, i + 1);
            ESP_LOGI(TAG, "[WIFI] Signal strength: %d dBm", ap_info.rssi);
            
            // Get IP address info
            esp_netif_ip_info_t ip_info;
            esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                ESP_LOGI(TAG, "[WIFI] IP Address: " IPSTR, IP2STR(&ip_info.ip));
            }
            return ESP_OK;
        }
    }
    
    // Connection failed or timeout
    ESP_LOGW(TAG, "[WIFI] ❌ Failed to connect to %s after 10 seconds", g_system_config.wifi_ssid);
    ESP_LOGW(TAG, "[WIFI] Common troubleshooting:");
    ESP_LOGW(TAG, "   1. Verify WiFi password is correct");
    ESP_LOGW(TAG, "   2. Check if network '%s' is visible and available", g_system_config.wifi_ssid);
    ESP_LOGW(TAG, "   3. Ensure network uses WPA/WPA2 security");
    ESP_LOGW(TAG, "   4. Try moving closer to the router");
    ESP_LOGW(TAG, "[WIFI] SoftAP 'ModbusIoT-Config' remains active at 192.168.4.1");
    
    return ESP_ERR_WIFI_TIMEOUT;
}

esp_err_t web_config_stop(void)
{
    if (g_server) {
        httpd_stop(g_server);
        g_server = NULL;
    }
    
    // Check if WiFi STA is connected before disabling AP
    wifi_ap_record_t ap_info;
    esp_err_t conn_status = esp_wifi_sta_get_ap_info(&ap_info);
    
    if (conn_status == ESP_OK) {
        // WiFi is connected, safe to switch to STA-only mode
        esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to switch back to STA mode: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "WiFi connected to %s, switched to STA-only mode", ap_info.ssid);
        }
    } else {
        // WiFi not connected, keep AP+STA mode for access
        ESP_LOGW(TAG, "WiFi not connected, maintaining AP+STA mode for continued access");
        ESP_LOGI(TAG, "SoftAP 'ModbusIoT-Config' remains active at 192.168.4.1");
    }
    
    return ESP_OK;
}

// Start web server with SoftAP while keeping STA connection (AP+STA mode)
esp_err_t web_config_start_server_only(void)
{
    ESP_LOGI(TAG, "Starting web server with SoftAP (AP+STA mode)");
    
    // Set WiFi mode to AP+STA (dual mode)
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set AP+STA mode: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create AP network interface if it doesn't exist
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif == NULL) {
        ap_netif = esp_netif_create_default_wifi_ap();
        ESP_LOGI(TAG, "Created new AP network interface");
    }
    
    // Configure the SoftAP
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ModbusIoT-Config",
            .ssid_len = strlen("ModbusIoT-Config"),
            .channel = 6,
            .password = "config123",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .ssid_hidden = 0,
            .beacon_interval = 100
        },
    };
    
    // Set AP configuration
    ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set AP config: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start WiFi if not already started (SIM mode scenario)
    // In WiFi mode, WiFi is already running in STA mode
    // We need to stop it first, then start in AP+STA mode
    wifi_mode_t current_mode;
    esp_wifi_get_mode(&current_mode);

    if (current_mode == WIFI_MODE_STA) {
        // WiFi already started in STA mode, need to stop and restart in AP+STA
        ESP_LOGI(TAG, "Stopping WiFi STA mode to switch to AP+STA");
        esp_wifi_stop();
        vTaskDelay(pdMS_TO_TICKS(500));  // Wait for stop to complete
    }

    // Now start WiFi in AP+STA mode
    ret = esp_wifi_start();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_STOPPED) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "SoftAP started: SSID='ModbusIoT-Config', Password='config123'");
    ESP_LOGI(TAG, "Web server will be available at: http://192.168.4.1");
    
    // Initialize Modbus if not already initialized (for sensor testing)
    ret = modbus_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[WARN] Failed to initialize Modbus: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "[WARN] Sensor testing will not work until Modbus is properly connected");
    } else {
        ESP_LOGI(TAG, "SUCCESS: Modbus RS485 initialized successfully for web server");
    }
    
    return start_webserver();
}