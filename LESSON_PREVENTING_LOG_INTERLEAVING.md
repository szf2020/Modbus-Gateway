# LESSON: Preventing Log Interleaving in Multi-Core/Multi-Thread Systems
## How to Keep Your Beautiful Log Headers Intact

### The Problem Demonstrated

**What Happened:**
```
I (25174) AZURE_IOT: ╔══════════════════════════════════════════════════════════╗
I (25174) MODBUS: [WAIT] Waiting for response (timeout: 1000 ms)...
I (25194) AZURE_IOT: ║           ☁️  MQTT CLIENT TASK STARTED ☁️                 ║
I (25204) AZURE_IOT: ╚══════════════════════════════════════════════════════════╝
```

The beautiful box-drawing characters were split by a MODBUS log message from another task running on a different core!

### Root Cause Analysis

In multi-core systems like ESP32:
- **Core 0** runs the Modbus task
- **Core 1** runs MQTT and Telemetry tasks
- Both cores can call `ESP_LOGI()` simultaneously
- The logging system outputs messages as they arrive
- Result: Interleaved/mixed log messages

### Why This Is a Critical Issue

1. **Breaks Visual Structure** - Styled headers become unreadable
2. **Confuses Debugging** - Hard to follow log flow
3. **Professional Impact** - Looks unprofessional in production logs
4. **Parser Problems** - Automated log parsers may fail
5. **Customer Perception** - Appears as poor quality software

## The Solution: Thread Synchronization

### Method 1: Mutex Protection (Implemented)

```c
// Global mutex for log synchronization
static SemaphoreHandle_t startup_log_mutex = NULL;

// In app_main, before creating tasks:
startup_log_mutex = xSemaphoreCreateMutex();

// In each task startup:
static void modbus_task(void *pvParameters)
{
    // Wait to let system stabilize
    vTaskDelay(pdMS_TO_TICKS(100));

    // Lock mutex before logging
    if (startup_log_mutex != NULL) {
        xSemaphoreTake(startup_log_mutex, portMAX_DELAY);
    }

    // Log the beautiful header
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║         🔌 MODBUS READER TASK STARTED 🔌                 ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════╝");

    // Release mutex
    if (startup_log_mutex != NULL) {
        xSemaphoreGive(startup_log_mutex);
    }
}
```

### Method 2: Staggered Startup Delays

```c
// Each task waits a different amount
modbus_task:    vTaskDelay(pdMS_TO_TICKS(100));  // Starts first
mqtt_task:      vTaskDelay(pdMS_TO_TICKS(200));  // Starts second
telemetry_task: vTaskDelay(pdMS_TO_TICKS(300));  // Starts third
```

### Method 3: Combined Approach (Best)

Use both mutex AND staggered delays:
- Mutex ensures atomic logging
- Delays reduce contention probability

## Common Mistakes to Avoid

### Mistake 1: No Synchronization
```c
// WRONG - Tasks start logging immediately
static void task(void *pvParameters) {
    ESP_LOGI(TAG, "╔════════╗");  // Can be interrupted!
    ESP_LOGI(TAG, "║ HEADER ║");  // By another task!
    ESP_LOGI(TAG, "╚════════╝");  // Broken box!
}
```

### Mistake 2: Forgetting Multi-Core Nature
```c
// WRONG - Assuming sequential execution
xTaskCreate(task1, ...);  // Core 0
xTaskCreate(task2, ...);  // Core 1 - Runs SIMULTANEOUSLY!
```

### Mistake 3: Too Many Separate Log Calls
```c
// WRONG - Each line can be interrupted
ESP_LOGI(TAG, "Line 1");
ESP_LOGI(TAG, "Line 2");  // Another task can log here!
ESP_LOGI(TAG, "Line 3");
```

### Mistake 4: Not Considering Timing
```c
// WRONG - All tasks log at same time
void task1() { ESP_LOGI(...); }  // T=0ms
void task2() { ESP_LOGI(...); }  // T=0ms - COLLISION!
void task3() { ESP_LOGI(...); }  // T=0ms - COLLISION!
```

## Best Practices

### 1. Use Atomic Logging Where Possible
```c
// BETTER - Single log call if possible
char buffer[512];
snprintf(buffer, sizeof(buffer),
    "╔════════╗\n"
    "║ HEADER ║\n"
    "╚════════╝");
ESP_LOGI(TAG, "%s", buffer);
```

### 2. Create a Log Manager
```c
typedef struct {
    SemaphoreHandle_t mutex;
    bool initialized;
} log_manager_t;

void log_box_header(const char *title) {
    xSemaphoreTake(log_mgr.mutex, portMAX_DELAY);
    ESP_LOGI(TAG, "╔══════════════╗");
    ESP_LOGI(TAG, "║ %-12s ║", title);
    ESP_LOGI(TAG, "╚══════════════╝");
    xSemaphoreGive(log_mgr.mutex);
}
```

### 3. Use Task Priorities for Order
```c
// Higher priority tasks log first
xTaskCreate(critical_task, ..., PRIORITY_HIGH);
xTaskCreate(normal_task, ..., PRIORITY_NORMAL);
xTaskCreate(background_task, ..., PRIORITY_LOW);
```

### 4. Implement Log Buffering
```c
// Collect messages, then output atomically
typedef struct {
    char messages[10][256];
    int count;
} log_buffer_t;

void flush_log_buffer(log_buffer_t *buf) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    for (int i = 0; i < buf->count; i++) {
        ESP_LOGI(TAG, "%s", buf->messages[i]);
    }
    xSemaphoreGive(mutex);
}
```

## Testing for Interleaving

### Test Case 1: Stress Test
```c
// Create many tasks logging simultaneously
for (int i = 0; i < 10; i++) {
    xTaskCreate(logging_task, ..., NULL);
}
```

### Test Case 2: Continuous Logging
```c
void stress_task(void *param) {
    while (1) {
        ESP_LOGI(TAG, "Task %d logging", (int)param);
        vTaskDelay(pdMS_TO_TICKS(rand() % 100));
    }
}
```

## Debugging Interleaved Logs

### Signs of Interleaving:
1. Broken box-drawing characters
2. Mixed message prefixes (MODBUS in MQTT logs)
3. Incomplete lines
4. Out-of-order timestamps
5. Mixed formatting

### How to Debug:
```c
// Add core ID to every log
ESP_LOGI(TAG, "[Core:%d] Message", xPortGetCoreID());

// Add task name
ESP_LOGI(TAG, "[%s] Message", pcTaskGetName(NULL));

// Add timestamp
ESP_LOGI(TAG, "[%lld] Message", esp_timer_get_time());
```

## Performance Considerations

### Mutex Overhead
- Each mutex take/give adds ~10-50 microseconds
- Acceptable for startup logs
- Consider alternatives for high-frequency logging

### Alternative Solutions:
1. **Ring Buffer** - Tasks write to buffer, single task outputs
2. **Message Queue** - Send log messages to dedicated logger task
3. **Core Affinity** - Pin logging to single core
4. **Log Levels** - Reduce log volume in production

## Implementation Checklist

- [ ] Create log synchronization mutex
- [ ] Add mutex protection to multi-line logs
- [ ] Implement startup delays for tasks
- [ ] Test with multiple tasks logging
- [ ] Verify no deadlocks possible
- [ ] Document synchronization strategy
- [ ] Consider production log volume
- [ ] Add log manager if needed

## Code Template: Safe Multi-Line Logging

```c
// reusable_log_helpers.h
#ifndef LOG_HELPERS_H
#define LOG_HELPERS_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    SemaphoreHandle_t mutex;
    bool initialized;
} log_sync_t;

extern log_sync_t g_log_sync;

// Initialize log synchronization
void log_sync_init(void);

// Thread-safe styled header
void log_styled_header(const char *tag, const char *emoji,
                       const char *title);

// Thread-safe box
void log_box(const char *tag, const char *lines[], int count);

#endif // LOG_HELPERS_H
```

```c
// reusable_log_helpers.c
#include "log_helpers.h"
#include "esp_log.h"

log_sync_t g_log_sync = {0};

void log_sync_init(void) {
    if (!g_log_sync.initialized) {
        g_log_sync.mutex = xSemaphoreCreateMutex();
        g_log_sync.initialized = true;
    }
}

void log_styled_header(const char *tag, const char *emoji,
                       const char *title) {
    if (g_log_sync.mutex) {
        xSemaphoreTake(g_log_sync.mutex, portMAX_DELAY);
    }

    ESP_LOGI(tag, "╔══════════════════════════════════════════════════════════╗");
    ESP_LOGI(tag, "║         %s %s %s                                        ║",
             emoji, title, emoji);
    ESP_LOGI(tag, "╚══════════════════════════════════════════════════════════╝");

    if (g_log_sync.mutex) {
        xSemaphoreGive(g_log_sync.mutex);
    }
}

void log_box(const char *tag, const char *lines[], int count) {
    if (g_log_sync.mutex) {
        xSemaphoreTake(g_log_sync.mutex, portMAX_DELAY);
    }

    ESP_LOGI(tag, "┌────────────────────────────────────────┐");
    for (int i = 0; i < count; i++) {
        ESP_LOGI(tag, "│ %-38s │", lines[i]);
    }
    ESP_LOGI(tag, "└────────────────────────────────────────┘");

    if (g_log_sync.mutex) {
        xSemaphoreGive(g_log_sync.mutex);
    }
}
```

## Summary: The Golden Rules

### Rule 1: Never Assume Sequential Logging
Multi-core systems execute in parallel. Protect your logs!

### Rule 2: Beautiful Headers Need Protection
If you spent time making pretty boxes, protect them with mutexes!

### Rule 3: Test With Concurrent Tasks
Always test with multiple tasks logging simultaneously.

### Rule 4: Consider Production Load
What works in development might fail under production load.

### Rule 5: Document Your Strategy
Tell other developers how your log synchronization works.

## Conclusion

Log interleaving is a common problem in multi-threaded/multi-core systems. The solution requires understanding:

1. **Why it happens** - Parallel execution
2. **When it happens** - Simultaneous logging
3. **How to prevent it** - Synchronization
4. **How to test it** - Stress testing
5. **How to maintain it** - Documentation

By implementing proper synchronization, your beautiful Style 15 headers will remain intact and professional-looking, making debugging easier and improving the overall quality perception of your system.

Remember: **A few milliseconds of synchronization overhead is worth preventing hours of debugging corrupted logs!**