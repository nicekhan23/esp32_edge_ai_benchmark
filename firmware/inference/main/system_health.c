#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "system_monitor.h"
#include <string.h>

static const char *TAG = "SYSTEM_HEALTH";

// Add this global to track UART connection status
static bool s_uart_connected = false;
static uint32_t s_last_uart_activity = 0;

void health_init(system_health_t *health) {
    if (!health) return;
    
    memset(health, 0, sizeof(system_health_t));
    health->state = SYSTEM_STATE_NORMAL;
    health->uart_connected = false;
    health->health_counter = 0;
    health->recent_accuracy = 1.0f;
    health->inference_time_avg = 0;
    
    #ifdef CONFIG_DETAILED_LOGGING
    ESP_LOGI(TAG, "System health monitoring initialized");
    #endif
}

void update_system_health(system_health_t *health, 
                          QueueHandle_t samples_queue, 
                          QueueHandle_t labels_queue) {
    if (!health) return;
    
    // Check task status
    TaskStatus_t tasks[5];  // Reduced from 10
    uint32_t task_count = uxTaskGetSystemState(tasks, 5, NULL);
    health->task_count = (task_count > 63) ? 63 : task_count;
    
    // Check queue utilization
    if (samples_queue) {
        UBaseType_t queue_messages = uxQueueMessagesWaiting(samples_queue);
        UBaseType_t queue_spaces = uxQueueSpacesAvailable(samples_queue);
        UBaseType_t total = queue_messages + queue_spaces;
        
        if (total > 0) {
            uint32_t utilization = (queue_messages * 100) / total;
            health->queue_utilization = (utilization > 127) ? 127 : utilization;
        } else {
            health->queue_utilization = 0;
        }
    }
    
    // Check memory
    health->free_heap = esp_get_free_heap_size();
    health->min_free_heap = esp_get_minimum_free_heap_size();
    
    // Update UART connection status
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    health->uart_connected = ((current_time - s_last_uart_activity) < 5000);
    
    // Update health counter (wrap at 65535)
    health->health_counter = (health->health_counter + 1) & 0xFFFF;
}

// Call this from UART receive callback
void health_update_uart_activity(void) {
    s_last_uart_activity = xTaskGetTickCount() * portTICK_PERIOD_MS;
}

system_state_t check_system_state(system_health_t *health) {
    if (!health) return SYSTEM_STATE_FAILED;
    
    // Check multiple metrics
    if (health->free_heap < 10240) {  // Less than 10KB free
        health->state = SYSTEM_STATE_CRITICAL;
        #ifdef CONFIG_DETAILED_LOGGING
        ESP_LOGW(TAG, "Critical: Low heap memory (%d bytes)", health->free_heap);
        #endif
    } else if (!health->uart_connected || health->recent_accuracy < 0.5f) {
        health->state = SYSTEM_STATE_DEGRADED;
        #ifdef CONFIG_DETAILED_LOGGING
        ESP_LOGW(TAG, "Degraded: UART=%s, Accuracy=%.2f", 
                 health->uart_connected ? "connected" : "disconnected",
                 health->recent_accuracy);
        #endif
    } else if (health->queue_utilization > 90) {
        health->state = SYSTEM_STATE_DEGRADED;
        #ifdef CONFIG_DETAILED_LOGGING
        ESP_LOGW(TAG, "Degraded: High queue utilization (%d%%)", health->queue_utilization);
        #endif
    } else if (health->inference_time_avg > 100000) {  // >100ms
        health->state = SYSTEM_STATE_DEGRADED;
        #ifdef CONFIG_DETAILED_LOGGING
        ESP_LOGW(TAG, "Degraded: Slow inference (%d us)", health->inference_time_avg);
        #endif
    } else {
        health->state = SYSTEM_STATE_NORMAL;
    }
    
    return health->state;
}

void log_system_health(system_health_t *health) {
    if (!health) return;
    
    #ifdef CONFIG_DETAILED_LOGGING
    ESP_LOGI(TAG, "=== System Health ===");
    ESP_LOGI(TAG, "State: %s", 
             health->state == SYSTEM_STATE_NORMAL ? "NORMAL" :
             health->state == SYSTEM_STATE_DEGRADED ? "DEGRADED" :
             health->state == SYSTEM_STATE_CRITICAL ? "CRITICAL" : "FAILED");
    ESP_LOGI(TAG, "Tasks: %d", health->task_count);
    ESP_LOGI(TAG, "Queue utilization: %d%%", health->queue_utilization);
    ESP_LOGI(TAG, "Free heap: %d bytes (min: %d)", health->free_heap, health->min_free_heap);
    ESP_LOGI(TAG, "UART: %s", health->uart_connected ? "connected" : "disconnected");
    ESP_LOGI(TAG, "Recent accuracy: %.2f", health->recent_accuracy);
    ESP_LOGI(TAG, "Avg inference time: %d us", health->inference_time_avg);
    ESP_LOGI(TAG, "Health counter: %u", health->health_counter);
    #endif
}