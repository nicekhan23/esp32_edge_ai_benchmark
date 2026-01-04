/**
 * @file main.c
 * @brief Main application entry point for ESP32 ML signal processing system
 * @details Coordinates signal acquisition, feature extraction, inference, and output.
 *          Implements the main processing loop with periodic statistics reporting.
 * 
 * @author Darkhan Zhanibekuly
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Runs indefinitely; designed for embedded real-time operation
 * @note Uses FreeRTOS tasks and queues for modular data flow
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

// Configuration flag - set to true for raw data collection mode
#define RAW_DATA_COLLECTION_MODE 1

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
        .mode = OUTPUT_MODE_CSV,      // Use CSV for data collection
        .print_raw_data = true,       // Enable raw data output
        .print_features = false,      // Disable features
        .print_inference = false,     // Disable inference
        .print_stats = false,         // Disable stats
        .output_interval_ms = 0       // No periodic stats
    };
    
    if (!output_init(&output_config)) {
        ESP_LOGE(TAG, "Output initialization failed");
        return;
    }
    
    // Initialize only what's needed for raw data collection
    signal_acquisition_init();
    signal_acquisition_init_uart();
    
#if !RAW_DATA_COLLECTION_MODE
    // Normal mode: initialize everything
    inference_init();
    benchmark_init();
#endif
    
    ESP_LOGI(TAG, "System initialized - %s", 
             RAW_DATA_COLLECTION_MODE ? "RAW DATA COLLECTION MODE" : "NORMAL PROCESSING MODE");
    
    // Get window queue
    QueueHandle_t window_queue = signal_acquisition_get_window_queue();
    if (!window_queue) {
        ESP_LOGE(TAG, "Window queue not available");
        return;
    }
    
    // Start acquisition
    signal_acquisition_start();
    
    ESP_LOGI(TAG, "Starting data collection...");
    ESP_LOGI(TAG, "Waiting for UART signal generator to send SYNC LABEL commands...");
    ESP_LOGI(TAG, "Format: SYNC LABEL wave=<0-4>");
    ESP_LOGI(TAG, "0:SINE 1:SQUARE 2:TRIANGLE 3:SAWTOOTH 4:NOISE");
    
#if RAW_DATA_COLLECTION_MODE
    // ================================================================
    // RAW DATA COLLECTION MODE
    // ================================================================
    ESP_LOGI(TAG, "=== COLLECTING CLEAN DATASET ===");
    ESP_LOGI(TAG, "Instructions:");
    ESP_LOGI(TAG, "1. Connect signal generator to ADC pin");
    ESP_LOGI(TAG, "2. Send 'SYNC LABEL wave=X' via UART");
    ESP_LOGI(TAG, "3. Collect 1000+ windows per signal type");
    ESP_LOGI(TAG, "4. Save output to CSV file");
    ESP_LOGI(TAG, "=================================\n");
    
    window_buffer_t window;
    uint32_t windows_collected = 0;
    
    while (1) {
        // Wait for window (blocking)
        if (xQueueReceive(window_queue, &window, portMAX_DELAY)) {
            windows_collected++;
            
            // Create inference result placeholder
            ml_output_t result = {
                .predicted_class = window.label,  // Use ground truth label
                .confidence = 1.0f,               // Not applicable
                .inference_time_us = 0,
                .window_id = window.window_id
            };
            
            // Create feature vector placeholder (empty for raw mode)
            feature_vector_t features;
            memset(&features, 0, sizeof(features));
            features.window_id = window.window_id;
            features.timestamp_us = window.timestamp_us;
            
            // Output as ML dataset row (CSV format with metadata)
            output_ml_dataset_row(&window, &features, &result);
            
            // Log progress every 100 windows
            if (windows_collected % 100 == 0) {
                ESP_LOGI(TAG, "Collected %lu windows, current label: %s (%d)", 
                         windows_collected, 
                         ml_class_to_string(window.label),
                         window.label);
            }
            
            // Small delay to prevent watchdog
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    
#else
    // ================================================================
    // NORMAL PROCESSING MODE (original code)
    // ================================================================
    ESP_LOGI(TAG, "Entering normal processing loop...\n");
    
    // Main processing loop
    window_buffer_t window;
    feature_vector_t features;
    
    uint32_t stats_counter = 0;
    const uint32_t STATS_INTERVAL = 500;
    
    while (1) {
        // Wait for window (blocking)
        if (xQueueReceive(window_queue, &window, portMAX_DELAY)) {
            benchmark_start_window();
            
            // Extract features
            extract_features(&window, &features);
            
            // Run inference
            benchmark_start_inference();
            ml_output_t result = run_inference(&window);
            benchmark_end_inference();
            
            benchmark_end_window();
            
            // Export ML training data (optional)
            output_ml_dataset_row(&window, &features, &result);
            
            // Original output logic (keep for monitoring)
            output_message_t msg;
            msg.timestamp_us = esp_timer_get_time();
            
            if (output_config.print_inference) {
                msg.type = OUTPUT_INFERENCE;
                msg.data.inference = result;
                output_queue_send(&msg);
            }
            
            // Periodic statistics
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
            }
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }
#endif
    
    // Cleanup (unreachable in normal operation)
    signal_acquisition_stop();
#if !RAW_DATA_COLLECTION_MODE
    inference_cleanup();
#endif
    output_cleanup();
    ESP_LOGI(TAG, "System shutdown complete");
}