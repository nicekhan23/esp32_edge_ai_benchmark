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
    
    // Initialize output system
    output_config_t output_config = {
        .mode = OUTPUT_MODE_HUMAN,
        .print_raw_data = false,
        .print_features = true,
        .print_inference = true,
        .print_stats = false,
        .output_interval_ms = 1000
    };
    output_init(&output_config);
    
    // Initialize subsystems
    signal_acquisition_init();
    inference_init();
    benchmark_init();
    
    ESP_LOGI(TAG, "All subsystems initialized");
    
    // Get window queue
    QueueHandle_t window_queue = signal_acquisition_get_window_queue();
    
    // Start acquisition
    signal_acquisition_start();
    
    ESP_LOGI(TAG, "Entering main processing loop...\n");
    
    // Main processing loop
    window_buffer_t window;
    feature_vector_t features;
    
    uint32_t stats_counter = 0;
    const uint32_t STATS_INTERVAL = 100;  /**< Print stats every 100 windows */
    
    while (1) {
        // Wait for window
        if (xQueueReceive(window_queue, &window, portMAX_DELAY)) {
            benchmark_start_window();
            
            // Extract features
            extract_features(&window, &features);
            
            // Run inference
            benchmark_start_inference();
            inference_result_t result = run_inference(&features);
            benchmark_end_inference();
            
            // Output results
            if (output_config.print_raw_data) {
                output_raw_window(&window);
            }
            if (output_config.print_features) {
                output_features(&features);
            }
            if (output_config.print_inference) {
                output_inference_result(&result);
            }
            
            benchmark_end_window();
            
            // Periodic statistics
            stats_counter++;
            if (stats_counter >= STATS_INTERVAL) {
                benchmark_metrics_t metrics = benchmark_get_metrics();
                acquisition_stats_t acq_stats = signal_acquisition_get_stats();
                inference_stats_t inf_stats = get_inference_stats();
                
                output_benchmark_summary(&metrics);
                output_acquisition_stats(&acq_stats);
                output_inference_stats(&inf_stats);
                
                stats_counter = 0;
            }
            
            // Small delay to prevent watchdog
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    
    // Cleanup (unreachable in normal operation)
    signal_acquisition_stop();
    inference_cleanup();
    ESP_LOGI(TAG, "System shutdown complete");
}