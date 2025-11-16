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

// LOGO CONFIGURATION - Edit these values to customize your logo
#define COMPANY_NAME "Fluxgen"
#define COMPANY_TAGLINE "Building a Water Positive Future"
#define LOGO_COLOR_1 "#0066cc"  // Fluxgen Blue
#define LOGO_COLOR_2 "#00aaff"  // Fluxgen Light Blue

// Global configuration
static system_config_t g_system_config = {0};
static config_state_t g_config_state = CONFIG_STATE_SETUP;
static httpd_handle_t g_server = NULL;
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
".scada-table th{padding:var(--space-md);font-weight:var(--weight-bold);text-align:center;color:white;font-family:Orbitron,monospace}"
".scada-table td{padding:var(--space-sm) var(--space-md);border-bottom:1px solid var(--color-border-light);text-align:left}"
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
".card,.sensor-card{background:rgba(255,255,255,0.95);border:1px solid rgba(255,255,255,0.4);border-radius:var(--radius-xl);padding:var(--space-xl);margin:var(--space-lg) 0;box-shadow:var(--shadow-lg);position:relative;transition:transform var(--transition-base),box-shadow var(--transition-base);overflow:visible;min-height:100px;display:flex;flex-direction:column;justify-content:center;will-change:transform;transform:translateZ(0)}"
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
".form-grid{display:grid;grid-template-columns:180px 1fr;gap:var(--space-md);align-items:center;margin:var(--space-md) 0}"
".form-grid label{font-weight:var(--weight-semibold);color:var(--color-text-primary);text-align:left}"
".form-grid input,.form-grid select,.form-grid textarea{width:100%;max-width:400px;padding:var(--space-sm);border:1px solid var(--color-border-medium);border-radius:var(--radius-sm);font-size:var(--text-base)}"
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
".main-content{margin-left:320px;padding:var(--space-2xl);flex:1;max-width:1400px}"
"/* ===== NAVIGATION MENU ===== */"
".menu-item{display:flex;align-items:center;gap:var(--space-md);width:100%;padding:var(--space-lg) var(--space-xl);color:var(--color-text-primary);text-decoration:none;border:none;background:transparent;cursor:pointer;text-align:left;font-size:var(--text-sm);font-family:Orbitron,monospace;font-weight:var(--weight-semibold);text-transform:uppercase;letter-spacing:0.05em;border-bottom:1px solid rgba(255,255,255,0.1);transition:background-color var(--transition-fast),color var(--transition-fast);position:relative;overflow:hidden}"
".menu-item::before{content:'';position:absolute;left:0;top:0;bottom:0;width:0;background:linear-gradient(180deg,var(--color-primary),var(--color-accent));transition:width var(--transition-fast)}"
".menu-item:hover{background:var(--color-primary-light);color:var(--color-primary)}"
".menu-item:hover::before{width:4px}"
".menu-item.active{background:linear-gradient(135deg,var(--color-primary),var(--color-accent));color:#fff;box-shadow:0 4px 16px var(--glow-primary),inset 0 1px 0 rgba(255,255,255,0.2);font-weight:var(--weight-bold)}"
".menu-item.active::before{width:4px;background:var(--color-success)}"
".menu-item.active .menu-icon{transform:scale(1.1);filter:drop-shadow(0 0 8px rgba(255,255,255,0.5))}"
".menu-icon{width:24px;height:24px;min-width:24px;display:inline-flex;align-items:center;justify-content:center;font-size:var(--text-lg);transition:all var(--transition-base);flex-shrink:0}"
"i.menu-icon:before{display:inline-block;width:100%;text-align:center}"
"/* ===== SECTIONS ===== */"
".section{display:none;background:rgba(255,255,255,0.95);border:1px solid rgba(255,255,255,0.4);border-radius:var(--radius-2xl);padding:var(--space-2xl);margin-bottom:var(--space-2xl);box-shadow:var(--shadow-lg);position:relative;overflow:hidden;animation:fadeInUp 0.3s ease-out}"
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
".main-content{max-width:1600px;margin-left:calc(320px + (100vw - 1600px) / 2)}"
"}"
"</style>"
"<script>"
"function showSection(sectionId){"
"var sections=document.querySelectorAll('.section');"
"var menuItems=document.querySelectorAll('.menu-item');"
"sections.forEach(function(s){s.classList.remove('active');});"
"menuItems.forEach(function(m){m.classList.remove('active');});"
"document.getElementById(sectionId).classList.add('active');"
"var activeBtn=document.querySelector('[onclick*=\"'+sectionId+'\"]'); if(activeBtn) activeBtn.classList.add('active');"
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
"}).catch(err=>console.log('Status update failed:',err));}"

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
"<div class='header'>"
"<img src='data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMjAwIiBoZWlnaHQ9IjYwIiB2aWV3Qm94PSIwIDAgMjAwIDYwIiB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciPjxkZWZzPjxsaW5lYXJHcmFkaWVudCBpZD0iYmciIHgxPSIwJSIgeTE9IjAlIiB4Mj0iMTAwJSIgeTI9IjAlIj48c3RvcCBvZmZzZXQ9IjAlIiBzdHlsZT0ic3RvcC1jb2xvcjojMDA5OWZmO3N0b3Atb3BhY2l0eToxIi8+PHN0b3Agb2Zmc2V0PSIxMDAlIiBzdHlsZT0ic3RvcC1jb2xvcjojMDA2NmNjO3N0b3Atb3BhY2l0eToxIi8+PC9saW5lYXJHcmFkaWVudD48L2RlZnM+PGVsbGlwc2UgY3g9IjI1IiBjeT0iMjIiIHJ4PSIxMiIgcnk9IjE0IiBmaWxsPSJ1cmwoI2JnKSIvPjxwYXRoIGQ9Ik0gMTUgMzIgUSAxOCAzOCwgMjUgNDAgUSAzMiAzOCwgMzUgMzIiIGZpbGw9InVybCgjYmcpIi8+PGVsbGlwc2UgY3g9IjI1IiBjeT0iMTkiIHJ4PSI3IiByeT0iOSIgZmlsbD0iI2ZmZmZmZiIgb3BhY2l0eT0iMC4zIi8+PHRleHQgeD0iNTAiIHk9IjM1IiBmb250LWZhbWlseT0iJ1NlZ29lIFVJJyxBcmlhbCxzYW5zLXNlcmlmIiBmb250LXNpemU9IjI4IiBmb250LXdlaWdodD0iNzAwIiBmaWxsPSIjMDA2NmNjIj5GbHV4Z2VuPC90ZXh0Pjx0ZXh0IHg9IjUwIiB5PSI0OCIgZm9udC1mYW1pbHk9IidTZWdvZSBVSScsQXJpYWwsc2Fucy1zZXJpZiIgZm9udC1zaXplPSI4IiBmaWxsPSIjNjY2Ij5CdWlsZGluZyBhIFdhdGVyIFBvc2l0aXZlIEZ1dHVyZTwvdGV4dD48L3N2Zz4=' class='logo' alt='Fluxgen'>"
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
"<button class='menu-item' onclick='showSection(\"sensors\")'>"
"<i class='menu-icon'>💾</i>MODBUS SENSORS"
"</button>"
"<button class='menu-item' onclick='showSection(\"write_ops\")'>"
"<i class='menu-icon'>✎</i>WRITE OPERATIONS"
"</button>"
"<button class='menu-item' onclick='showSection(\"monitoring\")'>"
"<i class='menu-icon'>💻</i>SYSTEM MONITOR"
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
                 "• Check RS485 wiring (A+, B-, GND)<br>"
                 "• Verify device is powered and functional<br>"
                 "• Confirm slave ID matches device configuration<br>"
                 "• Check register address is valid for this device<br>"
                 "• Ensure baud rate and parity settings match device",
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
static esp_err_t api_sim_test_handler(httpd_req_t *req);
static esp_err_t api_sim_test_status_handler(httpd_req_t *req);
static esp_err_t api_sd_status_handler(httpd_req_t *req);
static esp_err_t api_sd_clear_handler(httpd_req_t *req);
static esp_err_t api_sd_replay_handler(httpd_req_t *req);
static esp_err_t api_rtc_time_handler(httpd_req_t *req);
static esp_err_t api_rtc_sync_handler(httpd_req_t *req);
static esp_err_t api_rtc_set_handler(httpd_req_t *req);

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


// Company logo handler - Simple classic design
static esp_err_t logo_handler(httpd_req_t *req)
{
    // Simple classic logo with clean typography
    const char* logo_svg =
        "<svg width='200' height='60' xmlns='http://www.w3.org/2000/svg'>"
        "<defs>"
        "<linearGradient id='blueGrad' x1='0%' y1='0%' x2='100%' y2='0%'>"
        "<stop offset='0%' style='stop-color:#0066cc;stop-opacity:1' />"
        "<stop offset='100%' style='stop-color:#00aaff;stop-opacity:1' />"
        "</linearGradient>"
        "</defs>"
        "<rect width='200' height='60' fill='#ffffff' />"
        "<text x='10' y='35' font-family='Arial,sans-serif' font-size='26' font-weight='bold' fill='url(#blueGrad)'>FLUXGEN</text>"
        "<text x='10' y='50' font-family='Arial,sans-serif' font-size='9' fill='#666'>Building a Water Positive Future</text>"
        "</svg>";

    httpd_resp_set_type(req, "image/svg+xml");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    httpd_resp_send(req, logo_svg, strlen(logo_svg));
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
        "<h2 class='section-title'><i>📊</i>System Overview</h2>"

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
        "<p><strong>Sensors Configured:</strong> %d</p>"
        "<p><strong>RS485 Interface:</strong> GPIO16(RX), GPIO17(TX), GPIO4(RTS)</p>"
        "<p><strong>Modbus Protocol:</strong> RTU over RS485</p>"
        "<p><strong>Supported Baud Rates:</strong> 2400-230400 bps</p>"
        "<p><strong>Supported Data Types:</strong> UINT16, INT16, UINT32, INT32, FLOAT32 (All byte orders)</p>"
        "</div>"
        "</div>",
        g_system_config.sensor_count);
    httpd_resp_sendstr_chunk(req, chunk);

    // Network Mode Selection Section (main WiFi config section)
    snprintf(chunk, sizeof(chunk),
        "<div id='wifi' class='section'>"
        "<h2 class='section-title'><i>🌐</i>Network Configuration</h2>"
        "<div class='sensor-card'>"
        "<h3>Network Connectivity Mode</h3>"
        "<p style='color:#666;margin-bottom:15px'>Choose your network connectivity method</p>"
        "<form id='network_mode_form' onsubmit='return saveNetworkMode(event)'>"
        "<div style='margin:15px 0'>"
        "<label style='display:inline-block;margin-right:30px;cursor:pointer;padding:10px 15px;border:2px solid #ddd;border-radius:6px;background:#f8f9fa'>"
        "<input type='radio' name='network_mode' value='0' id='mode_wifi' %s onchange='toggleNetworkMode()' style='margin-right:8px'>"
        "<span style='font-weight:600'>WiFi</span>"
        "</label>"
        "<label style='display:inline-block;cursor:pointer;padding:10px 15px;border:2px solid #ddd;border-radius:6px;background:#f8f9fa'>"
        "<input type='radio' name='network_mode' value='1' id='mode_sim' %s onchange='toggleNetworkMode()' style='margin-right:8px'>"
        "<span style='font-weight:600'>SIM Module (4G)</span>"
        "</label>"
        "</div>"
        "<div id='network_mode_result' style='display:none;padding:12px;margin:15px 0;border-radius:5px'></div>"
        "<div style='background:#d4edda;padding:12px;margin:15px 0;border-radius:5px;border:1px solid #c3e6cb'>"
        "<button type='submit' style='background:#28a745;color:white;padding:10px 15px;border:none;border-radius:4px;font-weight:bold'>Save Network Mode</button>"
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
        "<div class='sensor-card'>"
        "<h3>WiFi Network Configuration</h3>"
        "<button type='button' onclick='scanWiFi()' class='scan-button' style='margin-bottom:var(--space-md)'>📡 Scan WiFi Networks</button>"
        "<div id='scan-status' style='color:#666;font-size:13px;margin-bottom:var(--space-sm)'></div>"
        "<div id='networks' style='display:none;border:1px solid #ddd;border-radius:8px;margin-bottom:var(--space-md);background:#f8f9fa;max-height:200px;overflow-y:auto'></div>"
        "<div class='wifi-grid'>"
        "<label>Network:</label>"
        "<input type='text' id='wifi_ssid' name='wifi_ssid' value='%s' required>"
        "<label>Password:</label>"
        "<div class='wifi-input-group'>"
        "<input type='password' id='wifi_password' name='wifi_password' value='%s' style='width:100%%;padding-right:60px'>"
        "<span onclick='togglePassword()' style='position:absolute;right:12px;cursor:pointer;color:#007bff;font-size:12px;font-weight:600;user-select:none;padding:4px 8px;background:#f8f9fa;border-radius:4px'>SHOW</span>"
        "</div>"
        "</div>"
        "<div style='background:#e8f4f8;padding:var(--space-sm);border-radius:var(--radius-sm);margin-top:var(--space-md);border-left:4px solid #17a2b8'>"
        "<small style='color:#0c5460'><strong>💡 Tip:</strong> Click on any scanned network to auto-fill the SSID field.</small>"
        "</div>"
        "<div style='background:#d4edda;padding:var(--space-md);margin-top:var(--space-md);border-radius:var(--radius-sm);border:1px solid #c3e6cb'>"
        "<button type='submit' style='background:#28a745;color:white;padding:10px 15px;border:none;border-radius:4px;font-weight:bold'>Save WiFi Settings</button>"
        "<p style='color:#155724;font-size:11px;margin:8px 0 0 0'>This saves WiFi settings only. Azure and sensors are configured separately.</p>"
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
        "<div class='sensor-card' style='padding:var(--space-lg)'>"
        "<h3 style='margin-top:0;margin-bottom:12px;color:var(--color-primary)'>Cellular Network Settings</h3>"
        "<p style='color:#666;margin-bottom:15px;font-size:14px'>Configure 4G cellular connectivity</p>"
        "<div style='display:grid;grid-template-columns:200px 1fr;gap:8px;align-items:start;margin-bottom:8px'>"
        "<label style='font-weight:600;padding-top:8px'>APN (Access Point Name):</label>"
        "<div>"
        "<input type='text' id='sim_apn' name='sim_apn' value='%s' placeholder='airteliot' maxlength='63' style='width:100%%;max-width:400px;padding:8px;border:1px solid #ddd;border-radius:4px'>"
        "<small style='color:#666;display:block;margin-top:2px'>Default: airteliot (Airtel) | Use jionet for Jio SIM</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:8px'>APN Username (optional):</label>"
        "<div>"
        "<input type='text' id='sim_apn_user' name='sim_apn_user' value='%s' placeholder='username' maxlength='63' style='width:100%%;max-width:400px;padding:8px;border:1px solid #ddd;border-radius:4px'>"
        "<small style='color:#666;display:block;margin-top:2px'>Leave blank if not required by carrier</small>"
        "</div>"
        "<label style='font-weight:600;padding-top:8px'>APN Password (optional):</label>"
        "<div>"
        "<input type='password' id='sim_apn_pass' name='sim_apn_pass' value='%s' maxlength='63' style='width:100%%;max-width:400px;padding:8px;border:1px solid #ddd;border-radius:4px'>"
        "<small style='color:#666;display:block;margin-top:2px'>Leave blank if not required by carrier</small>"
        "</div>"
        "</div>"
        "</div>"
        "<div class='sensor-card'>"
        "<h3>Hardware Configuration</h3>"
        "<label>UART Port:</label>"
        "<select id='sim_uart' name='sim_uart' style='padding:8px;border:1px solid #ddd;border-radius:6px;margin-bottom:8px'>"
        "<option value='1' %s>UART1</option>"
        "<option value='2' %s>UART2</option>"
        "</select><br>"
        "<div style='display:grid;grid-template-columns:1fr 1fr;gap:15px;margin-bottom:8px'>"
        "<div>"
        "<label>TX Pin:</label>"
        "<input type='number' id='sim_tx_pin' name='sim_tx_pin' value='%d' min='0' max='39' style='width:100%%;padding:8px'><br>"
        "<small style='color:#666'>GPIO for UART TX</small>"
        "</div>"
        "<div>"
        "<label>RX Pin:</label>"
        "<input type='number' id='sim_rx_pin' name='sim_rx_pin' value='%d' min='0' max='39' style='width:100%%;padding:8px'><br>"
        "<small style='color:#666'>GPIO for UART RX</small>"
        "</div>"
        "</div>"
        "<div style='display:grid;grid-template-columns:1fr 1fr;gap:15px;margin-bottom:8px'>"
        "<div>"
        "<label>Power Pin:</label>"
        "<input type='number' id='sim_pwr_pin' name='sim_pwr_pin' value='%d' min='0' max='39' style='width:100%%;padding:8px'><br>"
        "<small style='color:#666'>GPIO for module power</small>"
        "</div>"
        "<div>"
        "<label>Reset Pin:</label>"
        "<input type='number' id='sim_reset_pin' name='sim_reset_pin' value='%d' min='-1' max='39' style='width:100%%;padding:8px'><br>"
        "<small style='color:#666'>-1 to disable</small>"
        "</div>"
        "</div>"
        "<label>Baud Rate:</label>"
        "<select id='sim_baud' name='sim_baud' style='padding:8px;border:1px solid #ddd;border-radius:6px;margin-bottom:8px'>"
        "<option value='9600' %s>9600</option>"
        "<option value='19200' %s>19200</option>"
        "<option value='38400' %s>38400</option>"
        "<option value='57600' %s>57600</option>"
        "<option value='115200' %s>115200</option>"
        "</select><br>"
        "<button type='button' onclick='testSIMConnection()' style='background:#17a2b8;color:white;padding:10px 15px;border:none;border-radius:4px;font-weight:bold;margin-top:8px'>Test SIM Connection</button>"
        "<div id='sim_test_result' style='margin-top:10px;padding:10px;border-radius:4px;display:none'></div>"
        "<div style='background:#d4edda;padding:var(--space-md);margin-top:var(--space-md);border-radius:var(--radius-sm);border:1px solid #c3e6cb'>"
        "<button type='submit' style='background:#28a745;color:white;padding:10px 15px;border:none;border-radius:4px;font-weight:bold'>Save SIM Configuration</button>"
        "<p style='color:#155724;font-size:11px;margin:8px 0 0 0'>This saves SIM module settings.</p>"
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
        "<div class='sensor-card' style='text-align:left;padding:var(--space-lg)'>"
        "<h3 style='text-align:center;margin-top:0;margin-bottom:15px;color:var(--color-primary)'>Offline Message Caching</h3>"
        "<p style='color:#666;margin-bottom:15px;text-align:center;font-size:14px'>Enable offline message caching during network outages</p>"
        "<label style='display:inline-block;cursor:pointer;padding:10px;background:#f8f9fa;border-radius:6px;text-align:left;margin-bottom:10px'>"
        "<input type='checkbox' id='sd_enabled' name='sd_enabled' value='1' %s onchange='toggleSDOptions()' style='margin-right:8px;vertical-align:middle'>"
        "<span style='font-weight:600;vertical-align:middle'>Enable SD Card</span>"
        "</label>"
        "<div id='sd_options' style='display:%s;text-align:left;margin-top:10px'>"
        "<label style='display:inline-block;cursor:pointer;padding:10px;background:#e8f4f8;border-radius:6px;text-align:left'>"
        "<input type='checkbox' id='sd_cache_on_failure' name='sd_cache_on_failure' value='1' %s style='margin-right:8px;vertical-align:middle'>"
        "<span style='vertical-align:middle'>Cache Messages When Network Unavailable</span>"
        "</label>"
        "</div>"
        "</div>"
        "<div id='sd_hw_options' style='display:%s'>"
        "<div class='sensor-card'>"
        "<h3>SPI Pin Configuration</h3>"
        "<div style='display:grid;grid-template-columns:1fr 1fr;gap:15px;margin-bottom:8px'>"
        "<div>"
        "<label>MOSI Pin:</label>"
        "<input type='number' id='sd_mosi' name='sd_mosi' value='%d' min='0' max='39' style='width:100%%;padding:8px'><br>"
        "<small style='color:#666'>SPI Master Out Slave In</small>"
        "</div>"
        "<div>"
        "<label>MISO Pin:</label>"
        "<input type='number' id='sd_miso' name='sd_miso' value='%d' min='0' max='39' style='width:100%%;padding:8px'><br>"
        "<small style='color:#666'>SPI Master In Slave Out</small>"
        "</div>"
        "</div>"
        "<div style='display:grid;grid-template-columns:1fr 1fr;gap:15px;margin-bottom:8px'>"
        "<div>"
        "<label>CLK Pin:</label>"
        "<input type='number' id='sd_clk' name='sd_clk' value='%d' min='0' max='39' style='width:100%%;padding:8px'><br>"
        "<small style='color:#666'>SPI Clock</small>"
        "</div>"
        "<div>"
        "<label>CS Pin:</label>"
        "<input type='number' id='sd_cs' name='sd_cs' value='%d' min='0' max='39' style='width:100%%;padding:8px'><br>"
        "<small style='color:#666'>Chip Select</small>"
        "</div>"
        "</div>"
        "<label>SPI Host:</label>"
        "<select id='sd_spi_host' name='sd_spi_host' style='padding:8px;border:1px solid #ddd;border-radius:6px;margin-bottom:8px'>"
        "<option value='1' %s>HSPI (SPI2)</option>"
        "<option value='2' %s>VSPI (SPI3)</option>"
        "</select><br>"
        "<button type='button' onclick='checkSDStatus()' style='background:#17a2b8;color:white;padding:10px 15px;border:none;border-radius:4px;font-weight:bold;margin-top:8px'>Check SD Card Status</button>"
        "<div id='sd_status_result' style='margin-top:8px;padding:10px;border-radius:4px;display:none'></div>"
        "<button type='button' onclick='replayCachedMessages()' style='background:#ffc107;color:#333;padding:10px 15px;border:none;border-radius:4px;font-weight:bold;margin:8px 10px 8px 0'>Replay Cached Messages</button>"
        "<button type='button' onclick='clearCachedMessages()' style='background:#dc3545;color:white;padding:10px 15px;border:none;border-radius:4px;font-weight:bold;margin:8px 0'>Clear All Cached Messages</button>"
        "<div style='background:#d4edda;padding:12px;margin:15px 0;border-radius:5px;border:1px solid #c3e6cb'>"
        "<button type='submit' style='background:#28a745;color:white;padding:10px 15px;border:none;border-radius:4px;font-weight:bold'>Save SD Card Configuration</button>"
        "<div id='sd_save_result' style='margin-top:10px;padding:10px;border-radius:4px;display:none;border:1px solid'></div>"
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
        "<div class='sensor-card' style='text-align:left;padding:var(--space-lg)'>"
        "<h3 style='text-align:center;margin-top:0;margin-bottom:15px;color:var(--color-primary)'>Accurate Timekeeping</h3>"
        "<p style='color:#666;margin-bottom:15px;text-align:center;font-size:14px'>Maintain accurate time even during network outages</p>"
        "<label style='display:inline-block;cursor:pointer;padding:10px;background:#f8f9fa;border-radius:6px;text-align:left;margin-bottom:10px'>"
        "<input type='checkbox' id='rtc_enabled' name='rtc_enabled' value='1' %s onchange='toggleRTCOptions()' style='margin-right:8px;vertical-align:middle'>"
        "<span style='font-weight:600;vertical-align:middle'>Enable RTC</span>"
        "</label>"
        "<div id='rtc_options' style='display:%s;text-align:left;margin-top:10px'>"
        "<label style='display:inline-block;cursor:pointer;padding:10px;background:#e8f4f8;border-radius:6px;margin-bottom:10px;text-align:left;display:block'>"
        "<input type='checkbox' id='rtc_sync_on_boot' name='rtc_sync_on_boot' value='1' %s style='margin-right:8px;vertical-align:middle'>"
        "<span style='vertical-align:middle'>Sync System Time from RTC on Boot</span>"
        "</label>"
        "<label style='display:inline-block;cursor:pointer;padding:10px;background:#e8f4f8;border-radius:6px;text-align:left;display:block'>"
        "<input type='checkbox' id='rtc_update_from_ntp' name='rtc_update_from_ntp' value='1' %s style='margin-right:8px;vertical-align:middle'>"
        "<span style='vertical-align:middle'>Update RTC from NTP When Online</span>"
        "</label>"
        "</div>"
        "</div>"
        "<div id='rtc_hw_options' style='display:%s'>"
        "<div class='sensor-card'>"
        "<h3>I2C Pin Configuration</h3>"
        "<div style='display:grid;grid-template-columns:1fr 1fr;gap:15px;margin-bottom:8px'>"
        "<div>"
        "<label>SDA Pin:</label>"
        "<input type='number' id='rtc_sda' name='rtc_sda' value='%d' min='0' max='39' style='width:100%%;padding:8px'><br>"
        "<small style='color:#666'>I2C Data Line</small>"
        "</div>"
        "<div>"
        "<label>SCL Pin:</label>"
        "<input type='number' id='rtc_scl' name='rtc_scl' value='%d' min='0' max='39' style='width:100%%;padding:8px'><br>"
        "<small style='color:#666'>I2C Clock Line</small>"
        "</div>"
        "</div>"
        "<label>I2C Port:</label>"
        "<select id='rtc_i2c_num' name='rtc_i2c_num' style='padding:8px;border:1px solid #ddd;border-radius:6px;margin-bottom:8px'>"
        "<option value='0' %s>I2C_NUM_0</option>"
        "<option value='1' %s>I2C_NUM_1</option>"
        "</select><br>"
        "<button type='button' onclick='getRTCTime()' style='background:#17a2b8;color:white;padding:10px 15px;border:none;border-radius:4px;font-weight:bold;margin-top:8px'>Get RTC Time</button>"
        "<div id='rtc_time_result' style='margin-top:8px;padding:10px;border-radius:4px;display:none'></div>"
        "<button type='button' onclick='syncRTCFromNTP()' style='background:#ffc107;color:#333;padding:10px 15px;border:none;border-radius:4px;font-weight:bold;margin:8px 10px 8px 0'>Sync RTC from NTP Now</button>"
        "<button type='button' onclick='syncSystemFromRTC()' style='background:#6c757d;color:white;padding:10px 15px;border:none;border-radius:4px;font-weight:bold;margin:8px 0'>Sync System Time from RTC</button>"
        "<div style='background:#d4edda;padding:12px;margin:15px 0;border-radius:5px;border:1px solid #c3e6cb'>"
        "<button type='submit' style='background:#28a745;color:white;padding:10px 15px;border:none;border-radius:4px;font-weight:bold'>Save RTC Configuration</button>"
        "<div id='rtc_save_result' style='margin-top:10px;padding:10px;border-radius:4px;display:none;border:1px solid'></div>"
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

    // Configuration Trigger Section
    snprintf(chunk, sizeof(chunk),
        "<div class='sensor-card'>"
        "<h3>Configuration Trigger</h3>"
        "<form id='system-control-form' onsubmit='return saveSystemConfig()'>"
        "<label>Trigger GPIO Pin:</label>"
        "<input type='number' id='trigger_gpio_pin' name='trigger_gpio_pin' value='%d' min='0' max='39' style='width:100px'><br>"
        "<small style='color:#666'>GPIO pin for configuration mode trigger (0-39, pull LOW to enter config mode)</small><br>"
        "<div style='background:#d4edda;padding:12px;margin:15px 0;border-radius:5px;border:1px solid #c3e6cb'>"
        "<button type='submit' style='background:#28a745;color:white;padding:10px 15px;border:none;border-radius:4px;font-weight:bold'>Save System Settings</button>"
        "</div>"
        "<div id='system-control-result' style='margin-top:10px;padding:10px;border-radius:4px;display:none'></div>"
        "</form>"
        "</div>",
        g_system_config.trigger_gpio_pin > 0 ? g_system_config.trigger_gpio_pin : 34);
    httpd_resp_sendstr_chunk(req, chunk);
    
    // Modem Control Section
    snprintf(chunk, sizeof(chunk),
        "<div class='sensor-card'>"
        "<h3>Modem Control</h3>"
        "<form id='modem-form' onsubmit='return saveModemConfig()'>"
        "<label>"
        "<input type='checkbox' id='modem_reset_enabled' name='modem_reset_enabled' value='1' %s> Enable Modem Reset on MQTT Disconnect"
        "</label><br>"
        "<small style='color:#666'>When enabled, the specified GPIO will power cycle the modem (2 seconds) on MQTT disconnection</small><br>"
        "<label>GPIO Pin for Modem Reset:</label>"
        "<input type='number' id='modem_reset_gpio_pin' name='modem_reset_gpio_pin' value='%d' min='2' max='39' style='width:100px'><br>"
        "<small style='color:#666'>GPIO pin to control modem power (2-39, avoid 0,1,6-11 which are reserved)</small><br>"
        "<label>Modem Boot Delay (seconds):</label>"
        "<input type='number' id='modem_boot_delay' name='modem_boot_delay' value='%d' min='5' max='60' style='width:100px'><br>"
        "<small style='color:#666'>Time to wait for modem to boot up after reset before reconnecting WiFi (5-60 seconds)</small><br>"
        "<div style='background:#d4edda;padding:12px;margin:15px 0;border-radius:5px;border:1px solid #c3e6cb'>"
        "<button type='submit' style='background:#28a745;color:white;padding:10px 15px;border:none;border-radius:4px;font-weight:bold'>Save Modem Settings</button>"
        "</div>"
        "<div id='modem-result' style='margin-top:10px;padding:10px;border-radius:4px;display:none'></div>"
        "</form>"
        "</div>", 
        g_system_config.modem_reset_enabled ? "checked" : "",
        g_system_config.modem_reset_gpio_pin > 0 ? g_system_config.modem_reset_gpio_pin : 2,
        g_system_config.modem_boot_delay > 0 ? g_system_config.modem_boot_delay : 15);
    httpd_resp_sendstr_chunk(req, chunk);
    
    snprintf(chunk, sizeof(chunk),
        "<div style='background:#d1ecf1;padding:12px;margin:15px 0;border-radius:5px;border:1px solid #bee5eb'>"
        "<button type='button' onclick='rebootSystem()' style='background:#dc3545;color:white;padding:10px 15px;border:none;border-radius:4px;font-weight:bold'>Reboot to Normal Mode</button>"
        "<p style='color:#0c5460;font-size:11px;margin:8px 0 0 0'>Exit configuration mode and restart to normal operation mode.</p>"
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
        "<div class='sensor-card'>"
        "<h3>Connection Settings</h3>"
        "<label>IoT Hub FQDN:</label><input type='text' name='azure_hub_fqdn' value='%s' readonly style='background:#f8f9fa'><br>"
        "<small style='color:#666'>This is configured in the firmware (read-only)</small><br>"
        "<label>Device ID:</label><input type='text' name='azure_device_id' value='%s' style='width:100%%;max-width:400px;padding:8px' placeholder='Enter your Azure device ID' required><br>"
        "<small style='color:#666'>Your Azure IoT Hub device identifier</small><br>",
        IOT_CONFIG_IOTHUB_FQDN, g_system_config.azure_device_id);
    httpd_resp_sendstr_chunk(req, chunk);
    
    // Azure section - Part 2
    snprintf(chunk, sizeof(chunk),
        "<label>Device Key *:</label>"
        "<div style='position:relative;display:inline-block;width:100%%;max-width:400px'>"
        "<input type='password' id='azure_device_key' name='azure_device_key' value='%s' style='width:calc(100%% - 50px);padding:8px' placeholder='Enter your Azure device primary key' required>"
        "<span onclick='toggleAzurePassword()' style='position:absolute;right:5px;top:8px;cursor:pointer;background:#f8f9fa;padding:2px 5px;border-radius:3px;font-size:12px'>SHOW</span>"
        "</div><br>"
        "<small style='color:#666'>Required: Primary key from Azure IoT Hub device registration</small><br>"
        "</div>",
        g_system_config.azure_device_key);
    httpd_resp_sendstr_chunk(req, chunk);
    
    // Azure section - Part 3
    snprintf(chunk, sizeof(chunk),
        "<div class='sensor-card'>"
        "<h3>Telemetry Settings</h3>"
        "<label>Send Interval (seconds):</label><input type='number' name='telemetry_interval' value='%d' min='30' max='3600' style='width:100px'><br>"
        "<small style='color:#666'>How often to send sensor data to Azure (30-3600 seconds)</small><br>"
        "<div style='background:#d4edda;padding:12px;margin:15px 0;border-radius:5px;border:1px solid #c3e6cb'>"
        "<button type='button' onclick='saveAzureConfig()' style='background:#28a745;color:white;padding:12px 20px;border:none;border-radius:4px;font-weight:bold'>Save Azure Configuration</button>"
        "<p style='color:#155724;font-size:11px;margin:8px 0 0 0'>This saves Azure IoT Hub settings only.</p>"
        "</div>"
        "</div>"
        "</form>"
        "</div>",
        g_system_config.telemetry_interval);
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
        "<button type='button' onclick='showSensorSubMenu(\"regular\")' id='btn-regular-sensors' style='background:#007bff;color:white;padding:10px 15px;border:none;border-radius:4px;cursor:pointer;font-weight:bold'>Regular Sensors (%d)</button>"
        "<button type='button' onclick='showSensorSubMenu(\"water_quality\")' id='btn-water-quality-sensors' style='background:#17a2b8;color:white;padding:10px 15px;border:none;border-radius:4px;cursor:pointer;font-weight:bold'>Water Quality Sensors (%d)</button>"
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
                    "<button type='button' onclick='editSensor(%d)' style='background:#17a2b8;color:white;margin:2px;padding:6px 12px'>Edit</button> "
                    "<button type='button' onclick='testSensor(%d)' style='background:#007bff;color:white;margin:2px;padding:6px 12px'>Test RS485</button> "
                    "<button type='button' onclick='deleteSensor(%d)' style='background:#dc3545;color:white;margin:2px;padding:6px 12px'>Delete</button>"
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
                    "<button type='button' onclick='editSensor(%d)' style='background:#17a2b8;color:white;margin:2px;padding:6px 12px'>Edit</button> "
                    "<button type='button' onclick='testSensor(%d)' style='background:#007bff;color:white;margin:2px;padding:6px 12px'>Test RS485</button> "
                    "<button type='button' onclick='deleteSensor(%d)' style='background:#dc3545;color:white;margin:2px;padding:6px 12px'>Delete</button>"
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
                    "<button type='button' onclick='editSensor(%d)' style='background:#17a2b8;color:white;margin:2px;padding:6px 12px'>Edit</button> "
                    "<button type='button' onclick='testSensor(%d)' style='background:#007bff;color:white;margin:2px;padding:6px 12px'>Test RS485</button> "
                    "<button type='button' onclick='deleteSensor(%d)' style='background:#dc3545;color:white;margin:2px;padding:6px 12px'>Delete</button>"
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
                    "<button type='button' onclick='editSensor(%d)' style='background:#17a2b8;color:white;margin:2px;padding:6px 12px'>Edit</button> "
                    "<button type='button' onclick='testSensor(%d)' style='background:#007bff;color:white;margin:2px;padding:6px 12px'>Test RS485</button> "
                    "<button type='button' onclick='deleteSensor(%d)' style='background:#dc3545;color:white;margin:2px;padding:6px 12px'>Delete</button>"
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
                    "<button type='button' onclick='editSensor(%d)' style='background:#17a2b8;color:white;margin:2px;padding:6px 12px'>Edit</button> "
                    "<button type='button' onclick='testSensor(%d)' style='background:#007bff;color:white;margin:2px;padding:6px 12px'>Test RS485</button> "
                    "<button type='button' onclick='deleteSensor(%d)' style='background:#dc3545;color:white;margin:2px;padding:6px 12px'>Delete</button>"
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
        "<button type='button' onclick='addRegularSensor()' style='background:#28a745;color:white;padding:12px 20px;margin:5px;border:none;border-radius:5px;font-size:16px;cursor:pointer;box-shadow:0 2px 4px rgba(0,0,0,0.2)' onmouseover='this.style.background=\"#218838\"' onmouseout='this.style.background=\"#28a745\"'>Add New Regular Sensor</button>"
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
        "<button type='button' onclick='addWaterQualitySensor()' style='background:#17a2b8;color:white;padding:12px 20px;margin:5px;border:none;border-radius:5px;font-size:16px;cursor:pointer;box-shadow:0 2px 4px rgba(0,0,0,0.2)' onmouseover='this.style.background=\"#138496\"' onmouseout='this.style.background=\"#17a2b8\"'>Add Water Quality Sensor</button>"
        "<p style='color:#666;font-size:12px;margin:10px 0 5px 0'>Create individual water quality sensors with unique Unit IDs and custom Modbus configurations</p>"
        "</div>");
    
    httpd_resp_sendstr_chunk(req, "</div>");
    
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
    httpd_resp_sendstr_chunk(req, 
        "</div>"
        "</form>");
    
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
        "  h += '<div style=\"margin-top:15px;padding:10px;background:#f8f9fa;border-radius:4px\">';"
        "  h += '<button type=\"button\" onclick=\"testNewSensorRS485(' + sensorCount + ')\" style=\"background:#17a2b8;color:white;margin:5px;padding:10px 15px;border:none;border-radius:4px;font-weight:bold\">Test RS485</button>';"
        "  h += '<button type=\"button\" onclick=\"saveSingleSensor(' + sensorCount + ')\" style=\"background:#28a745;color:white;margin:5px;padding:10px 15px;border:none;border-radius:4px;font-weight:bold\">Save This Sensor</button>';"
        "  h += '<button type=\"button\" onclick=\"removeSensorForm(' + sensorCount + ')\" style=\"background:#dc3545;color:white;margin:5px;padding:10px 15px;border:none;border-radius:4px\">Cancel</button>';"
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
        "  var h = '<div class=\"sensor-card\" id=\"sensor-card-' + sensorCount + '\" style=\"border:2px dashed #007bff;padding:15px;margin:10px 0;background:#f8f9fa\">';"
        "  h += '<h4 style=\"color:#007bff;margin-top:0\">New Regular Sensor ' + (sensorCount+1) + '</h4>';"
        "  h += '<div class=\"sensor-form-grid\">';"
        "  h += '<label><strong>Sensor Type *:</strong></label><select name=\"sensor_' + sensorCount + '_sensor_type\" onchange=\"updateSensorForm(' + sensorCount + ')\" class=\"input-medium required-field\" required>';"
        "  h += '<option value=\"\">Select Regular Sensor Type</option>';"
        "  h += '<option value=\"Flow-Meter\">Flow-Meter</option>';"
        "  h += '<option value=\"Clampon\">Clampon</option>';"
        "  h += '<option value=\"Dailian\">Dailian Ultrasonic</option>';"
        "  h += '<option value=\"Piezometer\">Piezometer (Water Level)</option>';"
        "  h += '<option value=\"Level\">Level</option>';"
        "  h += '<option value=\"Radar Level\">Radar Level</option>';"
        "  h += '<option value=\"RAINGAUGE\">Rain Gauge</option>';"
        "  h += '<option value=\"BOREWELL\">Borewell</option>';"
        "  h += '<option value=\"ENERGY\">Energy Meter</option>';"
        "  h += '<option value=\"ZEST\">ZEST</option>';"
        "  h += '</select></div>';"
        "  h += '<div id=\"sensor-form-' + sensorCount + '\" style=\"display:none\">';"
        "  h += '</div>';"
        "  h += '<div class=\"sensor-actions\">';"
        "  h += '<button type=\"button\" onclick=\"testNewSensorRS485(' + sensorCount + ')\" style=\"background:#17a2b8;color:white;border:none;border-radius:4px;font-weight:bold\">Test RS485</button>';"
        "  h += '<button type=\"button\" onclick=\"saveSingleSensor(' + sensorCount + ')\" style=\"background:#28a745;color:white;border:none;border-radius:4px;font-weight:bold\">Save This Sensor</button>';"
        "  h += '<button type=\"button\" onclick=\"removeSensorForm(' + sensorCount + ')\" style=\"background:#dc3545;color:white;border:none;border-radius:4px\">Cancel</button>';"
        "  h += '<div id=\"test-result-new-' + sensorCount + '\" style=\"margin-top:10px;display:none\"></div>';"
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
        "  var h = '<div class=\"sensor-card\" id=\"sensor-card-' + sensorCount + '\" style=\"border:2px dashed #17a2b8;padding:15px;margin:10px 0;background:#f8f9fa\">';"
        "  h += '<h4 style=\"color:#17a2b8;margin-top:0\">New Water Quality Sensor ' + (sensorCount+1) + '</h4>';"
        "  h += '<input type=\"hidden\" name=\"sensor_' + sensorCount + '_sensor_type\" value=\"QUALITY\">';"
        "  h += '<p style=\"color:#17a2b8;font-weight:bold;margin:10px 0\">[TEST] Water Quality Sensor - Automatically set to QUALITY type</p>';"
        "  h += '<div id=\"sensor-form-' + sensorCount + '\" style=\"display:block\">';"
        "  h += '<div class=\"sensor-form-grid\">';"
        "  h += '<label><strong>Sensor Name *:</strong></label><input type=\"text\" name=\"sensor_' + sensorCount + '_name\" placeholder=\"e.g., Tank 1 Water Quality\" class=\"input-large required-field\" required>';"
        "  h += '<label><strong>Unit ID *:</strong></label><input type=\"text\" name=\"sensor_' + sensorCount + '_unit_id\" placeholder=\"e.g., TFG2235Q\" class=\"input-large required-field\" required>';"
        "  h += '<label><strong>Description:</strong></label><input type=\"text\" name=\"sensor_' + sensorCount + '_description\" placeholder=\"e.g., Main tank water quality monitoring\" class=\"input-large optional-field\">';"
        "  h += '<label><strong>Baud Rate:</strong></label><select name=\"sensor_' + sensorCount + '_baud_rate\" class=\"input-medium optional-field\">';"
        "  h += '<option value=\"9600\">9600</option><option value=\"19200\">19200</option><option value=\"38400\">38400</option><option value=\"115200\">115200</option>';"
        "  h += '</select>';"
        "  h += '<label><strong>Parity:</strong></label><select name=\"sensor_' + sensorCount + '_parity\" class=\"input-medium optional-field\">';"
        "  h += '<option value=\"none\">None</option><option value=\"even\">Even</option><option value=\"odd\">Odd</option>';"
        "  h += '</select></div>';"
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
        "  h += '<div class=\"sensor-actions\">';"
        "  h += '<button type=\"button\" onclick=\"testNewSensorRS485(' + sensorCount + ')\" style=\"background:#17a2b8;color:white;border:none;border-radius:4px;font-weight:bold\">Test RS485</button>';"
        "  h += '<button type=\"button\" onclick=\"saveSingleSensor(' + sensorCount + ')\" style=\"background:#28a745;color:white;border:none;border-radius:4px;font-weight:bold\">Save Water Quality Sensor</button>';"
        "  h += '<button type=\"button\" onclick=\"removeSensorForm(' + sensorCount + ')\" style=\"background:#dc3545;color:white;border:none;border-radius:4px\">Cancel</button>';"
        "  h += '<div id=\"test-result-new-' + sensorCount + '\" style=\"margin-top:10px;display:none\"></div>';"
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
        "  {key: 'Temp', name: 'Temp', units: '°C', description: 'Water temperature sensor'},"
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
        "formHtml += '<div class=\"sensor-form-grid\">';"
        "formHtml += '<label><strong>Name *:</strong></label><input type=\"text\" name=\"sensor_' + sensorId + '_name\" placeholder=\"e.g., anil\" class=\"input-medium required-field\" required>';"
        "formHtml += '<label><strong>Unit ID *:</strong></label><input type=\"text\" name=\"sensor_' + sensorId + '_unit_id\" placeholder=\"e.g., fg1234\" class=\"input-medium required-field\" required>';"
        "formHtml += '<label>Slave ID:</label><input type=\"number\" name=\"sensor_' + sensorId + '_slave_id\" value=\"1\" min=\"1\" max=\"247\" class=\"input-tiny optional-field\">';"
        "formHtml += '<label>Register Address:</label><input type=\"number\" name=\"sensor_' + sensorId + '_register_address\" value=\"0\" min=\"0\" max=\"65535\" class=\"input-small optional-field\">';"
        "formHtml += '<label>Quantity (registers):</label><input type=\"number\" name=\"sensor_' + sensorId + '_quantity\" value=\"1\" min=\"1\" max=\"125\" class=\"input-tiny optional-field\">';"
        "formHtml += '<label>Register Type:</label><select name=\"sensor_' + sensorId + '_register_type\" class=\"input-medium optional-field\">';"
        "formHtml += '<option value=\"HOLDING\">Holding Registers (03) - Read/Write</option>';"
        "formHtml += '<option value=\"INPUT\">Input Registers (04) - Read Only</option>';"
        "formHtml += '</select>';"
        "formHtml += '</div>';"
        "if (sensorType !== 'ZEST' && sensorType !== 'Clampon' && sensorType !== 'Dailian' && sensorType !== 'Piezometer') {"
        "formHtml += '<div class=\"sensor-form-grid\">';"
        "formHtml += '<label>Data Type:</label><select name=\"sensor_' + sensorId + '_data_type\" class=\"input-large optional-field\" onchange=\"showDataTypeInfo(this, ' + sensorId + ')\">';"
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
        "formHtml += '</div>';"
        "formHtml += '<div id=\"datatype-info-' + sensorId + '\" style=\"background:#e3f2fd;padding:10px;margin:10px 0;border-radius:4px;font-size:11px;display:none\"></div>';"
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
        "formHtml += '<input type=\"hidden\" name=\"sensor_' + sensorId + '_data_type\" value=\"FLOAT32_3412\">';"
        "formHtml += '<p style=\"color:#28a745;font-size:12px;margin:10px 0\"><strong>Data Format:</strong> Fixed - FLOAT32_3412 (CDAB byte order)</p>';"
        "formHtml += '<p style=\"color:#007bff;font-size:11px;margin:5px 0\"><em>Piezometer defaults: Register 10, Quantity 2 - reads water level in mwc (meters water column)</em></p>';"
        "}"
        "formHtml += '<div class=\"sensor-form-grid\">';"
        "formHtml += '<label>Baud Rate:</label><select name=\"sensor_' + sensorId + '_baud_rate\" class=\"input-small optional-field\">';"
        "formHtml += '<option value=\"2400\">2400 bps</option><option value=\"4800\">4800 bps</option><option value=\"9600\" selected>9600 bps</option><option value=\"19200\">19200 bps</option><option value=\"38400\">38400 bps</option><option value=\"57600\">57600 bps</option><option value=\"115200\">115200 bps</option></select>';"
        "formHtml += '<label>Parity:</label><select name=\"sensor_' + sensorId + '_parity\" class=\"input-small optional-field\">';"
        "formHtml += '<option value=\"none\" selected>None</option><option value=\"even\">Even</option><option value=\"odd\">Odd</option></select>';"
        "formHtml += '</div>';"
        "if (sensorType === 'Flow-Meter') {"
        "formHtml += '<label>Scale Factor:</label><input type=\"number\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\" step=\"any\" style=\"width:100px\" title=\"Multiplier applied to raw sensor value\"> <span style=\"color:#666;font-size:11px\">(e.g., 0.1 for /10, 1.0 for no scaling)</span><br>';"
        "} else if (sensorType === 'Level') {"
        "formHtml += '<label>Sensor Height:</label><input type=\"number\" name=\"sensor_' + sensorId + '_sensor_height\" value=\"0\" step=\"any\" style=\"width:100px\" title=\"Physical height of the sensor from bottom\"> <span style=\"color:#666;font-size:11px\">(meters or your unit)</span><br>';"
        "formHtml += '<label>Maximum Water Level:</label><input type=\"number\" name=\"sensor_' + sensorId + '_max_water_level\" value=\"0\" step=\"any\" style=\"width:100px\" title=\"Maximum water level for calculation\"> <span style=\"color:#666;font-size:11px\">(meters or your unit)</span><br>';"
        "formHtml += '<input type=\"hidden\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\">';"
        "} else if (sensorType === 'Radar Level') {"
        "formHtml += '<label><strong>Maximum Water Level *:</strong></label><input type=\"number\" name=\"sensor_' + sensorId + '_max_water_level\" value=\"0\" step=\"any\" style=\"width:100px;border:2px solid #dc3545\" title=\"Maximum water level for percentage calculation\" required> <span style=\"color:#666;font-size:11px\">(meters or your unit)</span><br>';"
        "formHtml += '<input type=\"hidden\" name=\"sensor_' + sensorId + '_sensor_height\" value=\"0\">';"
        "formHtml += '<input type=\"hidden\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\">';"
        "} else if (sensorType === 'ENERGY') {"
        "formHtml += '<label><strong>Meter Type *:</strong></label><input type=\"text\" name=\"sensor_' + sensorId + '_meter_type\" placeholder=\"e.g., Power Meter, Electric Meter\" style=\"width:250px;border:2px solid #dc3545\" required title=\"Type of energy meter (will be used as meter identifier)\"> <span style=\"color:#666;font-size:11px\">(Used as meter identifier in JSON)</span><br>';"
        "formHtml += '<input type=\"hidden\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\">';"
        "} else if (sensorType === 'RAINGAUGE') {"
        "formHtml += '<label>Scale Factor:</label><input type=\"number\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\" step=\"any\" style=\"width:100px\" title=\"Rain gauge scaling factor\"> <span style=\"color:#666;font-size:11px\">(e.g., 0.1 for mm/tips, 1.0 for direct reading)</span><br>';"
        "} else if (sensorType === 'BOREWELL') {"
        "formHtml += '<label>Scale Factor:</label><input type=\"number\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\" step=\"any\" style=\"width:100px\" title=\"Borewell sensor scaling factor\"> <span style=\"color:#666;font-size:11px\">(Scaling multiplier for borewell measurements)</span><br>';"
        "} else if (sensorType === 'ZEST') {"
        "formHtml += '<label>Scale Factor:</label><input type=\"number\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\" step=\"any\" style=\"width:100px\" title=\"ZEST sensor scaling factor\"> <span style=\"color:#666;font-size:11px\">(Applied to both INT32 values)</span><br>';"
        "formHtml += '<p style=\"color:#007bff;font-size:11px;margin:5px 0\"><em>ZEST defaults: Register 4121, Quantity 4 - First 2 as INT32_BE, next 2 as INT32_LE_SWAP, then sums them</em></p>';"
        "formHtml += '<p style=\"color:#28a745;font-size:10px;margin:5px 0\"><strong>Fixed format - no data type selection needed</strong></p>';"
        "} else if (sensorType === 'Clampon') {"
        "formHtml += '<label>Scale Factor:</label><input type=\"number\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\" step=\"any\" style=\"width:100px\" title=\"Clampon sensor scaling factor\"> <span style=\"color:#666;font-size:11px\">(Multiplier for raw value)</span><br>';"
        "formHtml += '<p style=\"color:#28a745;font-size:10px;margin:5px 0\"><strong>Fixed format - UINT32_3412 (CDAB) - no data type selection needed</strong></p>';"
        "} else if (sensorType === 'Dailian') {"
        "formHtml += '<label>Scale Factor:</label><input type=\"number\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\" step=\"any\" style=\"width:100px\" title=\"Dailian Ultrasonic scaling factor\"> <span style=\"color:#666;font-size:11px\">(Value has 3 implicit decimals: 863 = 0.863)</span><br>';"
        "formHtml += '<p style=\"color:#28a745;font-size:10px;margin:5px 0\"><strong>Fixed format - UINT32_3412 (CDAB) - no data type selection needed</strong></p>';"
        "} else if (sensorType === 'Piezometer') {"
        "formHtml += '<label>Scale Factor:</label><input type=\"number\" name=\"sensor_' + sensorId + '_scale_factor\" value=\"1.0\" step=\"any\" style=\"width:100px\" title=\"Piezometer scaling factor\"> <span style=\"color:#666;font-size:11px\">(Multiplier for mwc reading)</span><br>';"
        "formHtml += '<p style=\"color:#28a745;font-size:10px;margin:5px 0\"><strong>Fixed format - FLOAT32_3412 (CDAB) - no data type selection needed</strong></p>';"
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
        "if (quantityInput) quantityInput.value = '2';"
        "if (regAddrInput) regAddrInput.value = '10';"
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
        "if (scanInProgress) {"
        "statusDiv.innerHTML='<span style=\"color:#ffc107\">Scan already in progress. Please wait...</span>';"
        "return;"
        "}"
        "scanInProgress = true;"
        "scanBtn.style.opacity='0.6';"
        "scanBtn.style.pointerEvents='none';"
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
        "item.innerHTML='<div style=\"display:flex;justify-content:space-between;align-items:center\"><div><div style=\"display:flex;align-items:center;gap:8px\"><strong style=\"color:#2c3e50\">'+n.ssid+'</strong>'+n.security_icon+'</div><small style=\"color:#666;margin-top:2px;display:block\">Channel '+n.channel+' • '+n.signal_strength+'</small></div><div style=\"text-align:right\"><div style=\"color:'+signalColor+';font-weight:bold;font-size:13px\">'+n.rssi+'dBm</div><small style=\"color:#888\">'+n.signal_icon+'</small></div></div>';"
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
        "scanBtn.style.opacity='1';"
        "scanBtn.style.pointerEvents='auto';"
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
        "editForm+='<button type=\"button\" onclick=\"saveSensorEdit('+sensorId+')\" style=\"background:#28a745;color:white;padding:10px 15px;border:none;border-radius:4px;font-weight:bold\">Save Changes</button> ';"
        "editForm+='<button type=\"button\" onclick=\"testEditSensorRS485('+sensorId+')\" style=\"background:#007bff;color:white;padding:10px 15px;border:none;border-radius:4px;font-weight:bold\">Test RS485</button> ';"
        "editForm+='<button type=\"button\" onclick=\"cancelSensorEdit('+sensorId+')\" style=\"background:#6c757d;color:white;padding:10px 15px;border:none;border-radius:4px;font-weight:bold\">Cancel</button>';"
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
        "if(sensorType==='Piezometer' && !dataType) dataType='FLOAT32_3412';"
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
        "const btnRegular = document.getElementById('btn-regular-sensors');"
        "const btnWaterQuality = document.getElementById('btn-water-quality-sensors');"
        "if (menuType === 'regular') {"
        "regularSensors.style.display = 'block';"
        "waterQualitySensors.style.display = 'none';"
        "btnRegular.style.background = '#007bff';"
        "btnWaterQuality.style.background = '#6c757d';"
        "} else if (menuType === 'water_quality') {"
        "regularSensors.style.display = 'none';"
        "waterQualitySensors.style.display = 'block';"
        "btnRegular.style.background = '#6c757d';"
        "btnWaterQuality.style.background = '#17a2b8';"
        "}"
        "}"
        "console.log('Script loaded successfully. addSensor function defined:', typeof addSensor);"
        "</script>");
    
    // Close sensors section
    httpd_resp_sendstr_chunk(req,
        "</div>" // Close sensors section
        "<div id='write_ops' class='section'>"
        "<h2 class='section-title'><i>✏️</i>Write Operations</h2>"
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
    
    // Monitoring section
    snprintf(chunk, sizeof(chunk),
        "<div id='monitoring' class='section'>"
        "<h2 class='section-title'><i>🖥️</i>System Monitor</h2>"
        "<div class='sensor-card'>"
        "<h3>RS485 Configuration</h3>"
        "<p><strong>RX Pin:</strong> GPIO16</p>"
        "<p><strong>TX Pin:</strong> GPIO17</p>"
        "<p><strong>RTS Pin:</strong> GPIO4</p>"
        "<p><strong>Baud Rate:</strong> 9600</p>"
        "<p><strong>Parity:</strong> None</p>"
        "</div>"
        "<div class='sensor-card'>"
        "<h3>System Status</h3>"
        "<p><strong>Firmware:</strong> v1.1.0-final</p>"
        "<p><strong>Free Heap:</strong> <span id='heap'>Loading...</span></p>"
        "<p><strong>WiFi RSSI:</strong> <span id='rssi'>Loading...</span></p>"
        "</div>"
        "</div>");
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
            ESP_LOGI(TAG, "Azure device key updated");
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
            snprintf(temp_buf, sizeof(temp_buf), "<p style='margin:5px 0;color:#0277bd'>• Network: %s</p>", ap_info.ssid);
            httpd_resp_sendstr_chunk(req, temp_buf);
            snprintf(temp_buf, sizeof(temp_buf), "<p style='margin:5px 0;color:#0277bd'>• Signal: %d dBm</p>", ap_info.rssi);
            httpd_resp_sendstr_chunk(req, temp_buf);
            httpd_resp_sendstr_chunk(req, "<p style='margin:8px 0;color:#0277bd;font-size:14px'>You can now access this interface via your main WiFi network.</p>");
        } else {
            // Connection in progress or failed
            httpd_resp_sendstr_chunk(req, "<p style='margin:8px 0;color:#f57c00'>⏳ Connecting to WiFi network...</p>");
            snprintf(temp_buf, sizeof(temp_buf), "<p style='margin:5px 0;color:#0277bd'>• Target: %s</p>", g_system_config.wifi_ssid);
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
        char *format_table = (char*)malloc(6000);  // Larger buffer for comprehensive formats
        if (format_table == NULL) {
            ESP_LOGE(TAG, "Failed to allocate format_table buffer");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

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
            snprintf(format_table, 6000,
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
                // Registers [0,1]: INT32 Big Endian (ABCD)
                // Registers [2,3]: INT32 Little Endian Byte Swap (DCBA)

                // First 2 registers as INT32 Big Endian
                uint32_t int32_be = ((uint32_t)registers[0] << 16) | registers[1];
                int32_t signed_value1 = (int32_t)int32_be;
                double value1 = (double)signed_value1 * sensor->scale_factor;

                // Next 2 registers as INT32 Little Endian Byte Swap
                uint32_t int32_le_swap = ((uint32_t)registers[3] << 16) | registers[2];
                int32_t signed_value2 = (int32_t)int32_le_swap;
                double value2 = (double)signed_value2 * sensor->scale_factor;

                display_value = value1 + value2;
                snprintf(value_desc, sizeof(value_desc), "ZEST: INT32_BE(%ld) + INT32_LE_SWAP(%ld) = %.6f",
                         (long)signed_value1, (long)signed_value2, display_value);
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
                strcat(format_table, temp_str);
                
                // Calculate individual components for display (same logic as above)
                uint32_t uint32_be = ((uint32_t)registers[0] << 16) | registers[1];
                double value1 = (double)uint32_be * sensor->scale_factor;
                
                uint32_t uint32_le_swap = ((uint32_t)registers[3] << 16) | registers[2];
                double value2 = (double)uint32_le_swap * 0.001 * sensor->scale_factor;
                
                // Calculate correct ZEST total sum
                double zest_total = value1 + value2;
                
                char zest_breakdown[700];
                snprintf(zest_breakdown, sizeof(zest_breakdown),
                         "<br><div class='scada-breakdown'>"
                         "<b>ZEST Calculation Breakdown:</b><br>"
                         "• Registers [0-1] as UINT32_BE: 0x%04X%04X = %lu × %.3f = <b>%.6f</b><br>"
                         "• Registers [2-3] as UINT32_LE_SWAP: 0x%04X%04X = %lu × 0.001 × %.3f = <b>%.6f</b><br>"
                         "• <b>Total Sum = %.6f</b> ✅"
                         "</div>",
                         registers[0], registers[1], (unsigned long)uint32_be, sensor->scale_factor, value1,
                         registers[2], registers[3], (unsigned long)uint32_le_swap, sensor->scale_factor, value2,
                         zest_total);
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
            snprintf(format_table, 6000,
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
    g_system_config.config_complete = true;
    config_save_to_nvs(&g_system_config);
    
    const char* response = "{\"status\":\"success\",\"message\":\"Switching to operation mode\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    // Restart in 2 seconds
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    
    return ESP_OK;
}

// Reboot system handler
static esp_err_t reboot_handler(httpd_req_t *req)
{
    const char* response = "{\"status\":\"success\",\"message\":\"System rebooting...\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
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
        char *format_table = (char*)malloc(6000);  // Larger buffer for comprehensive formats
        if (format_table == NULL) {
            ESP_LOGE(TAG, "Failed to allocate format_table buffer");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        // Build the comprehensive data format table
        snprintf(format_table, 6000,
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
        char *data_output = malloc(1200);
        char *temp = malloc(300);
        if (!data_output || !temp) {
            ESP_LOGE(TAG, "Failed to allocate data buffers");
            if (data_output) free(data_output);
            if (temp) free(temp);
            free(response);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        memset(data_output, 0, 1200);
        memset(temp, 0, 300);
        
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
        
        snprintf(temp, 300, "<strong>Primary Value:</strong> %.6f (%s) | <strong>Scaled Value:</strong> %.6f (%s)<br>", 
                 primary_value, primary_desc, scaled_value, scale_desc);
        strcat(data_output, temp);
        
        // Add Level Filled display for Level and Radar Level sensors
        if (strcmp(sensor_type, "Level") == 0 && max_water_level > 0) {
            snprintf(temp, 300, "<strong>Level Filled:</strong> %.2f%% (Calculated using formula: (%.2f - %.6f) / %.2f * 100)<br>", 
                     scaled_value, sensor_height, primary_value, max_water_level);
            strcat(data_output, temp);
        } else if (strcmp(sensor_type, "Radar Level") == 0 && max_water_level > 0) {
            snprintf(temp, 300, "<strong>Level Filled:</strong> %.2f%% (Calculated using formula: %.6f / %.2f * 100)<br>", 
                     scaled_value, primary_value, max_water_level);
            strcat(data_output, temp);
        }
        
        // Raw registers
        strcat(data_output, "<strong>Raw Registers:</strong> [");
        for (int i = 0; i < reg_count; i++) {
            snprintf(temp, 300, "%s%d", i > 0 ? ", " : "", registers[i]);
            strcat(data_output, temp);
        }
        strcat(data_output, "]<br>");
        
        // Hex string
        strcat(data_output, "<strong>HexString:</strong> ");
        for (int i = 0; i < reg_count; i++) {
            snprintf(temp, 300, "%04X", registers[i]);
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
            snprintf(temp, 300, 
                     "<tr><td><strong>INT8 (High):</strong> %d</td><td><strong>INT8 (Low):</strong> %d</td><td><strong>UINT8 (High):</strong> %u</td><td><strong>UINT8 (Low):</strong> %u</td></tr>",
                     (int8_t)high_byte, (int8_t)low_byte, high_byte, low_byte);
            strcat(data_output, temp);
            vTaskDelay(pdMS_TO_TICKS(1)); // Yield after section
        }
        
        // 16-bit interpretations (1 register)
        if (reg_count >= 1) {
            uint16_t reg0 = registers[0];
            uint16_t reg0_le = ((reg0 & 0xFF) << 8) | ((reg0 >> 8) & 0xFF);
            snprintf(temp, 300, 
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
            
            snprintf(temp, 300, 
                     "<tr><td><strong>FLOAT32_1234:</strong> %.3e</td><td><strong>FLOAT32_4321:</strong> %.3e</td><td><strong>FLOAT32_2143:</strong> %.3e</td><td><strong>FLOAT32_3412:</strong> %.3e</td></tr>",
                     conv_1234.f, conv_4321.f, conv_2143.f, conv_3412.f);
            strcat(data_output, temp);
            
            // 32-bit Integer interpretations
            snprintf(temp, 300, 
                     "<tr style='background:#f8f8f8'><td><strong>INT32_1234:</strong> %ld</td><td><strong>INT32_3412:</strong> %ld</td><td><strong>INT32_2143:</strong> %ld</td><td><strong>INT32_4321:</strong> %ld</td></tr>",
                     (int32_t)val_1234, (int32_t)val_4321, (int32_t)val_2143, (int32_t)val_3412);
            strcat(data_output, temp);
            
            // 32-bit Unsigned Integer interpretations
            snprintf(temp, 300, 
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
            
            snprintf(temp, 300, 
                     "<tr><td><strong>FLOAT64_12345678:</strong> %.3e</td><td><strong>FLOAT64_87654321:</strong> %.3e</td><td><strong>ASCII (4 chars):</strong> %c%c%c%c</td><td><strong>HEX:</strong> 0x%04X%04X%04X%04X</td></tr>",
                     conv64_12345678.d, conv64_87654321.d,
                     (char)(registers[0] >> 8), (char)(registers[0] & 0xFF), (char)(registers[1] >> 8), (char)(registers[1] & 0xFF),
                     registers[0], registers[1], registers[2], registers[3]);
            strcat(data_output, temp);
        } else if (reg_count >= 2) {
            // ASCII and HEX for 2 registers
            snprintf(temp, 300, 
                     "<tr style='background:#f0f0f0'><td><strong>ASCII (2 chars):</strong> %c%c</td><td><strong>HEX:</strong> 0x%04X%04X</td><td><strong>BOOL (R0):</strong> %s</td><td><strong>BOOL (R1):</strong> %s</td></tr>",
                     (char)(registers[0] >> 8), (char)(registers[0] & 0xFF),
                     registers[0], registers[1],
                     registers[0] ? "True" : "False", registers[1] ? "True" : "False");
            strcat(data_output, temp);
        } else if (reg_count >= 1) {
            // Single register special formats
            snprintf(temp, 300, 
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
        char *escaped_output = malloc(2000);
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
        for (int i = 0; data_output[i] && j < 1980; i++) {
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
            strcpy(unit, "°C");
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

        ESP_LOGI(TAG, "Web server started on port 80");
        ESP_LOGI(TAG, "[NET] All URI handlers registered successfully (including new SIM/SD/RTC endpoints)");
        ESP_LOGI(TAG, "[CONFIG] Available endpoints: /, /save_config, /save_azure_config, /save_network_mode, /save_sim_config, /save_sd_config, /save_rtc_config, /test_sensor, /test_rs485, /start_operation, /scan_wifi, /live_data, /edit_sensor, /save_single_sensor, /delete_sensor, /api/system_status, /api/sim_test, /api/sd_status, /api/sd_clear, /api/sd_replay, /api/rtc_time, /api/rtc_sync, /api/rtc_set, /write_single_register, /write_multiple_registers, /reboot, /watchdog_control, /gpio_trigger, /logo, /favicon.ico");
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
    
    // Check if configuration is complete
    if (g_system_config.config_complete) {
        g_config_state = CONFIG_STATE_OPERATION;
        ESP_LOGI(TAG, "Configuration complete, starting in operation mode");
    } else {
        g_config_state = CONFIG_STATE_SETUP;
        ESP_LOGI(TAG, "No configuration found, starting in setup mode");
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
    ESP_LOGI(TAG, "Cleaning up SIM test - disconnecting PPP...");
    a7670c_ppp_disconnect();
    vTaskDelay(pdMS_TO_TICKS(2000));  // Wait for disconnect to complete

    ESP_LOGI(TAG, "Deinitializing modem...");
    a7670c_ppp_deinit();
    vTaskDelay(pdMS_TO_TICKS(1000));  // Wait for cleanup to complete

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
    esp_netif_create_default_wifi_sta();

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
            ESP_LOGI(TAG, "💡 Use GPIO trigger to start web config AP mode");
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

    // SD Card defaults (NEW)
    g_system_config.sd_config.enabled = true;   // ENABLED by default for production use
    g_system_config.sd_config.cache_on_failure = true;  // Auto-cache when network fails
    g_system_config.sd_config.mosi_pin = GPIO_NUM_13;
    g_system_config.sd_config.miso_pin = GPIO_NUM_12;
    g_system_config.sd_config.clk_pin = GPIO_NUM_14;
    g_system_config.sd_config.cs_pin = GPIO_NUM_5;
    g_system_config.sd_config.spi_host = SPI2_HOST;
    g_system_config.sd_config.max_message_size = 1024;
    g_system_config.sd_config.min_free_space_mb = 10;

    // RTC (DS3231) defaults (NEW)
    g_system_config.rtc_config.enabled = false;
    g_system_config.rtc_config.sda_pin = GPIO_NUM_21;
    g_system_config.rtc_config.scl_pin = GPIO_NUM_22;
    g_system_config.rtc_config.i2c_num = I2C_NUM_0;
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