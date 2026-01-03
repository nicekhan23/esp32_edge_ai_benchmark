/**
 * @file main.c
 * @brief Main application entry point for ESP32 ML signal processing system
 * @details Coordinates signal acquisition, feature extraction, inference, and output.
 *          Implements the main processing loop with periodic statistics reporting.
 * 
 * @author Your Name
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Runs indefinitely; designed for embedded real-time operation
 * @note Uses FreeRTOS tasks and queues for modular data flow
 * 
 * @copyright (c) 2025 ESP32 Signal Processing Project
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "signal_acquisition.h"
#include "feature_extraction.h"
#include "inference.h"
#include "benchmark.h"
#include "output.h"

static const char *TAG = "MAIN";                        /**< Logging tag for main */

/**
 * @brief Application entry point (main FreeRTOS task)
 * 
 * Initializes all subsystems, starts acquisition, and runs the main processing loop.
 * Periodically prints performance and acquisition statistics.
 * 
 * @note Never returns under normal operation
 * @note Includes 1ms delay per loop iteration for watchdog
 * 
 * @throws Various ESP_ERROR_CHECK failures if initialization fails
 */
void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32 ML Signal Processing System ===");
    
    // Initialize output system with task-based architecture
    output_config_t output_config = {
        .mode = OUTPUT_MODE_HUMAN,
        .print_raw_data = false,     // Disable verbose output
        .print_features = false,     // Disable verbose output
        .print_inference = true,     // Keep only inference results
        .print_stats = true,         // Enable periodic stats
        .output_interval_ms = 5000   // Longer interval for stats
    };
    
    if (!output_init(&output_config)) {
        ESP_LOGE(TAG, "Output initialization failed");
        return;
    }
    
    // Initialize subsystems
    signal_acquisition_init();
    signal_acquisition_init_uart();
    inference_init();
    benchmark_init();
    
    ESP_LOGI(TAG, "All subsystems initialized");
    
    // Get window queue
    QueueHandle_t window_queue = signal_acquisition_get_window_queue();
    if (!window_queue) {
        ESP_LOGE(TAG, "Window queue not available");
        return;
    }
    
    // Start acquisition
    signal_acquisition_start();
    
    ESP_LOGI(TAG, "Entering main processing loop...\n");
    
    // Main processing loop
    window_buffer_t window;
    feature_vector_t features;
    
    uint32_t stats_counter = 0;
    const uint32_t STATS_INTERVAL = 500;  // Increased interval
    
        while (1) {
        // Wait for window (blocking)
        if (xQueueReceive(window_queue, &window, portMAX_DELAY)) {
            benchmark_start_window();
            
            // Extract features
            extract_features(&window, &features);
            
            // Run inference
            benchmark_start_inference();
            inference_result_t result = run_inference(&features);
            benchmark_end_inference();
            
            benchmark_end_window();
            
            // ========== ADD VALIDATION OUTPUT ==========
            // Every 50 windows, print detailed validation
            if (window.window_id % 50 == 0) {
                output_window_validation(&window, &features);
            }
            
            // ========== EXPORT ML TRAINING DATA ==========
            // Export every window for dataset collection
            output_ml_dataset_row(&window, &features, &result);
            
            // Original output logic (keep for monitoring)
            output_message_t msg;
            msg.timestamp_us = esp_timer_get_time();
            
            if (output_config.print_inference) {
                msg.type = OUTPUT_INFERENCE;
                msg.data.inference = result;
                output_queue_send(&msg);
            }
            
            // Periodic statistics (less frequent)
            stats_counter++;
            if (stats_counter >= STATS_INTERVAL) {
                // Get current metrics
                benchmark_metrics_t metrics = benchmark_get_metrics();
                acquisition_stats_t acq_stats = signal_acquisition_get_stats();
                inference_stats_t inf_stats = get_inference_stats();
                
                // Send stats to output queue
                if (output_config.print_stats) {
                    msg.type = OUTPUT_BENCHMARK_SUMMARY;
                    msg.data.benchmark = metrics;
                    output_queue_send(&msg);
                    
                    msg.type = OUTPUT_ACQUISITION_STATS;
                    msg.data.acq_stats = acq_stats;
                    output_queue_send(&msg);
                    
                    msg.type = OUTPUT_INFERENCE_STATS;
                    msg.data.inf_stats = inf_stats;
                    output_queue_send(&msg);
                }
                
                stats_counter = 0;
                
                // Optional: Log queue stats for debugging
                uint32_t queue_count, queue_size;
                output_queue_stats(&queue_count, &queue_size);
                if (queue_count > queue_size * 0.8) {
                    ESP_LOGW(TAG, "Output queue filling up: %u/%u", queue_count, queue_size);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }
    
    // Cleanup (unreachable in normal operation)
    signal_acquisition_stop();
    inference_cleanup();
    output_cleanup();
    ESP_LOGI(TAG, "System shutdown complete");
}