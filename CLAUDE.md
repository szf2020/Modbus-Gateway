# CLAUDE.md - ESP32 Modbus IoT Gateway Project Context

## Project Overview
**ESP32 Modbus IoT Gateway v1.0.0** - A production-ready industrial IoT gateway for RS485 Modbus communication with Azure IoT Hub integration.

## ⚠️ CRITICAL WARNINGS - MUST READ

### 1. **web_config.c File (609KB) - EXTREME CAUTION REQUIRED**
- **Size**: 609KB - One of the largest source files
- **Content**: Mixed HTML/JavaScript/CSS embedded in C strings
- **Risk Level**: HIGH - Very prone to corruption during edits
- **Common Issues**:
  - Sections can merge during copy/paste operations
  - HTML strings can break C syntax if quotes aren't escaped
  - Large chunks of legitimate code can be embedded in corrupted sections
  - Missing `httpd_resp_sendstr_chunk()` calls cause web pages to not load

### 2. **Known Vulnerable Code Sections**
```
web_config.c lines 1260-1400: Sensor display logic - easily corrupted
web_config.c lines 3801-3833: WiFi configuration section
web_config.c lines 3835-3854: SIM configuration section
web_config.c lines 3856-3920: Modbus Explorer section
```

### 3. **WiFi Double Initialization Issue**
- **Problem**: `esp_netif_create_default_wifi_sta()` called twice causes crash
- **Location**: `web_config_start_ap_mode()` function
- **Solution**: Always check if interface exists before creating:
```c
esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
if (sta_netif == NULL) {
    sta_netif = esp_netif_create_default_wifi_sta();
}
```

## 🛠️ LESSONS LEARNED FROM DEBUGGING SESSIONS

### Issue #1: Heap Corruption with ZEST Sensors
**Problem**: ESP32 crashes after successful Modbus read from ZEST sensor
**Root Cause**: Missing ZEST_FIXED data type handler in test_rs485_handler
**Solution**: Added ZEST_FIXED case and increased buffer sizes
**Prevention**: Always check all data type cases are handled

### Issue #2: Web Config Sections Missing
**Problem**: WiFi and SIM configuration sections disappeared from web interface
**Root Cause**: When fixing corrupted code, legitimate sections were embedded in the corrupted block and got deleted
**Solution**: Re-added WiFi/SIM sections properly
**Prevention**:
- Before removing corrupted code, identify ALL content within it
- Check that every navigation button has a corresponding section
- Verify JavaScript references (`getElementById`) have matching HTML elements

### Issue #3: JavaScript Errors in Web Interface
**Problem**: `Cannot read properties of null` errors
**Root Cause**: JavaScript looking for elements that don't exist
**Common Missing Elements**:
- `mode_wifi` and `mode_sim` radio buttons
- `wifi` and `sim` div sections
- Various form input fields
**Solution**: Ensure all referenced IDs exist in HTML

### Issue #4: GPIO Conflicts
**Problem**: False positive GPIO conflict errors during compilation
**Root Cause**: Overly aggressive conflict detection macros
**Solution**: Comment out false conflict checks, document actual pin usage

### Issue #5: MQTT Reconnection Crashes
**Problem**: System restarts after 5 failed MQTT attempts
**Root Cause**: `SYSTEM_RESTART_ON_CRITICAL_ERROR` was true with low retry limit
**Solution**:
- Set `MAX_MQTT_RECONNECT_ATTEMPTS` to 10
- Set `SYSTEM_RESTART_ON_CRITICAL_ERROR` to false
- Added exponential backoff for reconnection

## 📋 BEFORE EDITING CHECKLIST

### Before Editing web_config.c:
- [ ] Make a backup copy of the file
- [ ] Note the line numbers you're editing
- [ ] Check what sections come before/after your edit
- [ ] Verify all `snprintf()` calls have matching `httpd_resp_sendstr_chunk()`
- [ ] Ensure HTML strings have escaped quotes (`\"` not `"`)
- [ ] Test one section at a time

### Before Major Changes:
- [ ] Commit current working state to git
- [ ] Document what you're changing and why
- [ ] Test incrementally - don't make multiple large changes at once
- [ ] Check heap usage before and after changes

## 🏗️ PROJECT STRUCTURE

### Critical Files and Their Roles:
```
main/
├── main.c                 # Main application (handle with care - task initialization)
├── web_config.c          # ⚠️ 609KB WEB INTERFACE - EXTREMELY FRAGILE
├── web_assets.h          # HTML/CSS/JS assets - navigation menu here
├── modbus.c              # RS485 Modbus implementation
├── sensor_manager.c      # Sensor data handling
├── gpio_map.h            # GPIO pin definitions and conflict detection
├── iot_configs.h         # Configuration constants (MQTT retry, etc.)
└── a7670c_ppp.c         # SIM module PPP implementation
```

### Memory Constraints:
- **Heap Size**: ~150KB available
- **Task Stack Sizes**:
  - modbus_task: 8192 bytes (safe)
  - mqtt_task: 8192 bytes (safe)
  - telemetry_task: 8192 bytes (safe)
  - modem_reset_task: 4096 bytes (increased from 2048)

## 🔧 COMMON FIXES

### Web Page Not Loading:
1. Check browser console for JavaScript errors
2. Verify all `getElementById()` calls have matching HTML elements
3. Ensure all sections end with `httpd_resp_sendstr_chunk(req, NULL);`
4. Check that `config_page_handler` returns `ESP_OK`

### ESP32 Crash on Boot:
1. Check for double WiFi initialization
2. Verify task stack sizes are adequate
3. Look for buffer overflows in string operations
4. Check GPIO configurations for conflicts

### Modbus Communication Issues:
1. Verify baud rate (9600 default)
2. Check GPIO pins: RX=16, TX=17, RTS=18
3. Ensure proper RS485 termination
4. Verify slave ID and register addresses

## 🚀 BUILD AND DEPLOY

### Standard Build Process:
```bash
idf.py build
idf.py -p COM3 flash monitor  # Windows
idf.py -p /dev/ttyUSB0 flash monitor  # Linux
```

### Clean Build (when things go wrong):
```bash
idf.py fullclean
idf.py build
```

### Monitor Filters (reduce noise):
```bash
idf.py monitor -f esp32_exception_decoder
```

## 💡 TIPS FOR FUTURE DEVELOPMENT

1. **Extract web_config.c HTML**: Consider moving HTML to separate files and including at compile time
2. **Use Version Control**: Commit after each successful change
3. **Test Incrementally**: Don't make multiple large changes without testing
4. **Document Changes**: Update this file when you discover new issues
5. **Buffer Sizes**: When in doubt, increase buffer sizes (but check heap)
6. **Error Messages**: Make them descriptive - future you will thank present you

## 🐛 KNOWN BUGS TO FIX

1. Web config file too large (609KB) - should be split
2. Sensor LED logic implemented but may not blink correctly
3. Missing Azure IoT features (Device Twin, OTA updates, C2D commands)
4. Race condition in web server toggle (partially fixed with debouncing)

## 📝 NOTES FOR CLAUDE CODE

**When working on this project:**
- Always read this file first
- Be extra careful with web_config.c - it's fragile
- Check for null pointers before using `getElementById()`
- Verify GPIO pins don't conflict before adding new peripherals
- Test web interface in browser console for JavaScript errors
- Remember that sections can be lost when fixing corruption
- **DO NOT attempt to run build commands** - User will build manually

**File Edit Priority:**
1. Try to edit smaller files when possible
2. If editing web_config.c is necessary, work in small sections
3. Always verify the web interface still works after changes
4. Use grep/search before assuming something doesn't exist

---

**Last Updated**: November 30, 2024
**Last Known Working Commit**: e0ee803
**Critical Issue Fixed**: Memory optimization for WiFi AP + PPP + MQTT coexistence