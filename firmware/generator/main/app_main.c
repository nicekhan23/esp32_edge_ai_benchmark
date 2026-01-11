#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "waveform_tables.h"
#include "dac_output.h"
#include "uart_labels.h"

// Configuration
#define WAVEFORM_SWITCH_PERIOD_MS  5000    // Base switch period
#define HEARTBEAT_PERIOD_MS        1000    // Send heartbeat every second
#define INITIAL_WAVEFORM           WAVEFORM_SINE

static const char *TAG = "app_main";

// Heartbeat task
void heartbeat_task(void *pvParameter) {
    while (1) {
        uart_send_heartbeat();
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS));
    }
}

// Waveform management task
void waveform_manager_task(void *pvParameter) {
    waveform_type_t current_waveform = INITIAL_WAVEFORM;
    
    // Initialize random seed
    srand(esp_timer_get_time());
    
    // Set initial waveform
    dac_output_set_waveform(current_waveform);
    uart_send_label(current_waveform);
    
    ESP_LOGI(TAG, "Starting waveform generation");
    
    while (1) {
        // Add random jitter (±10%)
        uint32_t jitter_ms = (rand() % 1000) - 500;  // -500 to +500 ms
        uint32_t delay_ms = WAVEFORM_SWITCH_PERIOD_MS + jitter_ms;
        
        if (delay_ms < 1000) delay_ms = 1000;  // Minimum 1 second
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        
        // Cycle to next waveform
        current_waveform = (current_waveform + 1) % WAVEFORM_COUNT;
        dac_output_set_waveform(current_waveform);
        
        // Send label with retries
        if (!uart_send_label_with_ack(current_waveform)) {
            ESP_LOGW(TAG, "Failed to get ACK for waveform %d, continuing anyway", current_waveform);
        }
        
        ESP_LOGI(TAG, "Switched to waveform: %d after %d ms (jitter: %d ms)", 
                 current_waveform, delay_ms, jitter_ms);
    }
}

void app_main(void) {
    // Initialize logging
    esp_log_level_set("*", ESP_LOG_INFO);
    
    ESP_LOGI(TAG, "Starting enhanced waveform generator application");
    
    // Initialize waveform tables
    waveform_tables_init();
    
    // Initialize hardware modules
    dac_output_init();
    uart_labels_init();
    
    // Start DAC output
    dac_output_start();
    
    // Create waveform management task
    xTaskCreate(
        waveform_manager_task,  // Task function
        "wave_mgr",             // Task name
        4096,                   // Stack size
        NULL,                   // Parameters
        5,                      // Priority
        NULL                    // Task handle
    );
    
    // Create heartbeat task
    xTaskCreate(
        heartbeat_task,         // Task function
        "heartbeat",            // Task name
        2048,                   // Stack size
        NULL,                   // Parameters
        3,                      // Priority (lower than waveform task)
        NULL                    // Task handle
    );
    
    // Main task just monitors
    int counter = 0;
    while (1) {
        ESP_LOGI(TAG, "System running... (loop %d)", ++counter);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}