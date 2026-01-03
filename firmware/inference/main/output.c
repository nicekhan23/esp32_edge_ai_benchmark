/**
 * @file output.c
 * @brief Multi-format output and logging system for signal processing
 * @details Provides configurable output in CSV, JSON, human-readable, and silent modes.
 *          Handles raw data, features, inference results, and system statistics.
 * 
 * @author Your Name
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Supports real-time streaming and periodic summary outputs
 * @note Thread-safe for multi-task environments
 * 
 * @copyright (c) 2025 ESP32 Signal Processing Project
 */

#include "output.h"
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "OUTPUT";                      /**< Logging tag for module */
static QueueHandle_t s_output_queue = NULL;               /**< Output message queue */
static TaskHandle_t s_output_task_handle = NULL;         /**< Output task handle */
static volatile bool s_output_running = false;            /**< Output task running flag */
static bool s_csv_header_printed = false;                /**< CSV header printed flag - ADD THIS LINE */

/**
 * @brief Default output configuration
 * 
 * Human-readable mode with features, inference, and stats enabled,
 * printing every 1000ms.
 */
static output_config_t s_config = {
    .mode = OUTPUT_MODE_HUMAN,
    .print_raw_data = false,
    .print_features = true,
    .print_inference = true,
    .print_stats = false,
    .output_interval_ms = 1000
};

/**
 * @brief Output rate limiting structure
 * 
 * Tracks message counts and limits per output type to prevent flooding.
 */
typedef struct {
    uint32_t msg_counts[OUTPUT_TYPE_COUNT];
    uint64_t last_reset_us;
    uint32_t limits[OUTPUT_TYPE_COUNT];
} output_rate_limit_t;

static output_rate_limit_t s_rate_limit = {
    .last_reset_us = 0,
    .limits = {
        [OUTPUT_RAW_WINDOW] = 10,     // 10 raw windows/sec max
        [OUTPUT_FEATURES] = 20,       // 20 features/sec max
        [OUTPUT_INFERENCE] = 50,      // 50 inferences/sec max
        [OUTPUT_BENCHMARK_SUMMARY] = 1,
        [OUTPUT_ACQUISITION_STATS] = 1,
        [OUTPUT_INFERENCE_STATS] = 1,
        [OUTPUT_SYSTEM_INFO] = 1
    }
};

/**
 * @brief Change output mode dynamically
 * 
 * @param[in] mode New output mode (CSV, JSON, HUMAN, SILENT)
 */
void output_set_mode(output_mode_t mode) {
    s_config.mode = mode;
}

/**
 * @brief Output raw window data according to current mode
 * 
 * In CSV mode: prints all samples with metadata.
 * In HUMAN mode: prints first 5 samples for brevity.
 * In SILENT mode: no output.
 * 
 * @param[in] window Pointer to window buffer structure
 * 
 * @note Respects s_config.print_raw_data flag
 * @note In CSV mode, does not include newline (for feature/inference continuation)
 */
void output_raw_window(const window_buffer_t *window) {
    if (!s_config.print_raw_data) return;
    
    if (s_config.mode == OUTPUT_MODE_CSV) {
        printf("%" PRIu64 ",%" PRIu32 ",%f",
               window->timestamp_us, 
               window->window_id, 
               window->sample_rate_hz);
        
        for (int i = 0; i < WINDOW_SIZE; i++) {
            printf(",%u", window->samples[i]);
        }
    } else if (s_config.mode == OUTPUT_MODE_HUMAN) {
        printf("[Window %lu] %" PRIu64 " us: ", 
               window->window_id, window->timestamp_us);
        for (int i = 0; i < 5; i++) {  // Print first 5 samples
            printf("%u ", window->samples[i]);
        }
        printf("...\n");
    }
}

/**
 * @brief Output feature vector according to current mode
 * 
 * In CSV mode: appends features to current line.
 * In HUMAN mode: prints first 4 features.
 * 
 * @param[in] features Pointer to feature vector structure
 * 
 * @note Respects s_config.print_features flag
 * @note In CSV mode, assumes called after output_raw_window on same line
 */
void output_features(const feature_vector_t *features) {
    if (!s_config.print_features) return;
    
    if (s_config.mode == OUTPUT_MODE_CSV) {
        for (int i = 0; i < FEATURE_VECTOR_SIZE; i++) {
            printf(",%.4f", features->features[i]);
        }
    } else if (s_config.mode == OUTPUT_MODE_HUMAN) {
        printf("  Features: ");
        for (int i = 0; i < 4; i++) {  // Print first 4 features
            printf("%.2f ", features->features[i]);
        }
        printf("...\n");
    }
}

/**
 * @brief Output inference result with signal type and confidence
 * 
 * In CSV mode: completes line with inference data.
 * In HUMAN mode: prints formatted result.
 * In JSON mode: outputs JSON object per window.
 * 
 * @param[in] result Pointer to inference result structure
 * 
 * @note Respects s_config.print_inference flag
 * @note In CSV mode, adds newline and flushes stdout
 */
void output_inference_result(const inference_result_t *result) {
    if (!s_config.print_inference) return;
    
    const char *type_str = signal_type_to_string(result->type);
    
    if (s_config.mode == OUTPUT_MODE_CSV) {
        printf(",%s,%.3f,%" PRIu64 "\n", 
               type_str, result->confidence, result->inference_time_us);
        fflush(stdout);
    } else if (s_config.mode == OUTPUT_MODE_HUMAN) {
        printf("  Inference: %s (%.1f%%) in %" PRIu64 " us\n", 
               type_str, result->confidence * 100, result->inference_time_us);
    } else if (s_config.mode == OUTPUT_MODE_JSON) {
        printf("{\"window_id\":%lu,\"signal_type\":\"%s\",\"confidence\":%.3f,\"time_us\":%" PRIu64 "}\n",
               result->window_id, type_str, result->confidence, result->inference_time_us);
    }
}

/**
 * @brief Output benchmark performance summary
 * 
 * Prints formatted performance metrics including processing rates,
 * CPU/memory usage, and error counts.
 * 
 * @param[in] metrics Pointer to benchmark metrics structure
 * 
 * @note Respects s_config.print_stats flag
 * @note Intended for periodic (e.g., every 100 windows) reporting
 */
void output_benchmark_summary(const benchmark_metrics_t *metrics) {
    if (!s_config.print_stats) return;
    
    printf("\n=== PERFORMANCE SUMMARY ===\n");
    printf("Processing Rate: %.1f windows/sec\n", 
           1000000.0 / metrics->avg_processing_time_us);
    printf("Inference Rate: %.1f inferences/sec\n", 
           1000000.0 / metrics->avg_inference_time_us);
    printf("CPU Usage: %.1f%%\n", metrics->cpu_usage_percent);
    printf("Memory Usage: %.1f KB\n", metrics->memory_usage_kb);
    printf("Missed Windows: %lu\n", metrics->missed_windows);
    printf("===========================\n");
}

/**
 * @brief Output acquisition subsystem statistics
 * 
 * Prints counts of samples, windows, buffer overruns, and sampling errors.
 * 
 * @param[in] stats Pointer to acquisition statistics structure
 * 
 * @note Always prints regardless of s_config.print_stats (acquisition is critical)
 */
void output_acquisition_stats(const acquisition_stats_t *stats) {
    printf("\n=== ACQUISITION STATS ===\n");
    printf("Samples: %lu\n", stats->samples_processed);
    printf("Windows: %lu\n", stats->windows_captured);
    printf("Overruns: %lu\n", stats->buffer_overruns);
    printf("Errors: %lu\n", stats->sampling_errors);
    printf("========================\n");
}

/**
 * @brief Output inference subsystem statistics
 * 
 * Prints inference counts, average time, accuracy, and per-signal-type breakdown.
 * 
 * @param[in] stats Pointer to inference statistics structure
 * 
 * @note Uses signal_type_to_string for type labels
 */
void output_inference_stats(const inference_stats_t *stats) {
    printf("\n=== INFERENCE STATS ===\n");
    printf("Total: %lu\n", stats->total_inferences);
    printf("Average Time: %.1f us\n", stats->avg_inference_time_us);
    printf("Accuracy: %.1f%%\n", stats->accuracy * 100);
    
    for (int i = 0; i < SIGNAL_COUNT; i++) {
        printf("%s: %lu\n", signal_type_to_string(i), stats->per_type_counts[i]);
    }
    printf("======================\n");
}

/**
 * @brief Output system configuration and startup banner
 * 
 * Prints fixed system parameters: sampling rate, window size, overlap,
 * feature vector size, and current output mode.
 * 
 * @note Called automatically by output_init()
 */
void output_system_info(void) {
    printf("\n=== ESP32 ML SIGNAL PROCESSING ===\n");
    printf("Sampling Rate: %d Hz\n", SAMPLING_RATE_HZ);
    printf("Window Size: %d\n", WINDOW_SIZE);
    printf("Overlap: %d\n", WINDOW_OVERLAP);
    printf("Feature Vector Size: %d\n", FEATURE_VECTOR_SIZE);
    printf("Output Mode: %d\n", s_config.mode);
    printf("==================================\n\n");
}

/**
 * @brief Flush output buffer
 * 
 * Ensures all pending stdout data is written immediately.
 * Useful for real-time streaming and logging.
 */
void output_flush(void) {
    fflush(stdout);
}

/**
 * @brief Check and update rate limiting for output messages
 * 
 * @param[in] type Output message type
 * @return true if message can be sent, false if rate limit exceeded
 */
static bool rate_limit_check(output_message_type_t type) {
    uint64_t now = esp_timer_get_time();
    
    // Reset counters every second
    if (now - s_rate_limit.last_reset_us > 1000000) {
        memset(s_rate_limit.msg_counts, 0, sizeof(s_rate_limit.msg_counts));
        s_rate_limit.last_reset_us = now;
    }
    
    // Check limit
    if (s_rate_limit.msg_counts[type] < s_rate_limit.limits[type]) {
        s_rate_limit.msg_counts[type]++;
        return true;
    }
    
    return false;
}

/**
 * @brief Output task main loop
 * 
 * Waits for messages on the output queue and processes them
 * according to the current configuration and rate limits.
 * 
 * @param[in] arg Pointer to output configuration
 */
static void output_task(void *arg) {
    output_config_t *config = (output_config_t *)arg;
    output_message_t msg;
    
    ESP_LOGI(TAG, "Output task started");
    
    while (s_output_running) {
        if (xQueueReceive(s_output_queue, &msg, portMAX_DELAY)) {
            // Apply rate limiting
            if (!rate_limit_check(msg.type)) {
                continue; // Skip this message due to rate limiting
            }
            
            switch (msg.type) {
                case OUTPUT_RAW_WINDOW:
                    if (config->print_raw_data) {
                        output_raw_window(&msg.data.window);
                    }
                    break;
                    
                case OUTPUT_FEATURES:
                    if (config->print_features) {
                        output_features(&msg.data.features);
                    }
                    break;
                    
                case OUTPUT_INFERENCE:
                    if (config->print_inference) {
                        output_inference_result(&msg.data.inference);
                    }
                    break;
                    
                case OUTPUT_BENCHMARK_SUMMARY:
                    if (config->print_stats) {
                        output_benchmark_summary(&msg.data.benchmark);
                    }
                    break;
                    
                case OUTPUT_ACQUISITION_STATS:
                    if (config->print_stats) {
                        output_acquisition_stats(&msg.data.acq_stats);
                    }
                    break;
                    
                case OUTPUT_INFERENCE_STATS:
                    if (config->print_stats) {
                        output_inference_stats(&msg.data.inf_stats);
                    }
                    break;
                    
                case OUTPUT_SYSTEM_INFO:
                    output_system_info();
                    break;
                    
                case OUTPUT_TYPE_COUNT:
                    // Handle OUTPUT_TYPE_COUNT (sentinel value, should not be used)
                    ESP_LOGW(TAG, "Invalid output message type: OUTPUT_TYPE_COUNT");
                    break;
                    
                default:
                    // Handle any unknown message types
                    ESP_LOGW(TAG, "Unknown output message type: %d", msg.type);
                    break;
            }
            
            // Small yield to prevent task watchdog
            taskYIELD();
        }
    }
    
    vTaskDelete(NULL);
}

/**
 * @brief Initialize output task with queue
 * 
 * Creates the output task and message queue for asynchronous output.
 * 
 * @param[in] config Output configuration
 * @return true if initialization successful, false otherwise
 */
bool output_task_init(const output_config_t *config) {
    // Create output queue
    s_output_queue = xQueueCreate(50, sizeof(output_message_t));
    if (!s_output_queue) {
        ESP_LOGE(TAG, "Failed to create output queue");
        return false;
    }
    
    // Store configuration
    if (config) {
        s_config = *config;
    }
    
    // Create output task
    s_output_running = true;
    BaseType_t ret = xTaskCreate(
        output_task,
        "output_task",
        4096,
        (void*)&s_config,
        1,  // Lower priority than main processing
        &s_output_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create output task");
        vQueueDelete(s_output_queue);
        s_output_queue = NULL;
        return false;
    }
    
    // Print CSV header if needed
    if (s_config.mode == OUTPUT_MODE_CSV) {
        printf("timestamp_us,window_id,sample_rate");
        for (int i = 0; i < WINDOW_SIZE; i++) {
            printf(",sample_%d", i);
        }
        printf(",features");
        for (int i = 0; i < FEATURE_VECTOR_SIZE; i++) {
            printf(",feature_%d", i);
        }
        printf(",signal_type,confidence,inference_time_us\n");
        s_csv_header_printed = true;
    }
    
    // Send system info message
    output_message_t sysinfo_msg = {
        .type = OUTPUT_SYSTEM_INFO,
        .timestamp_us = esp_timer_get_time()
    };
    output_queue_send(&sysinfo_msg);
    
    return true;
}

/**
 * @brief Send message to output queue
 * 
 * Non-blocking send to output queue for asynchronous processing.
 * 
 * @param[in] msg Pointer to output message
 * @return true if message queued successfully, false if queue full
 */
bool output_queue_send(output_message_t *msg) {
    if (!s_output_queue || !s_output_running) {
        return false;
    }
    
    // Don't block if queue is full
    BaseType_t ret = xQueueSend(s_output_queue, msg, 0);
    return (ret == pdTRUE);
}

/**
 * @brief Get output queue handle
 * 
 * @return QueueHandle_t Handle to output message queue
 */
QueueHandle_t output_get_queue(void) {
    return s_output_queue;
}

/**
 * @brief Check if output queue is full
 * 
 * @return true if queue is full, false otherwise
 */
bool output_queue_is_full(void) {
    if (!s_output_queue) return true;
    return (uxQueueMessagesWaiting(s_output_queue) == uxQueueSpacesAvailable(s_output_queue));
}

/**
 * @brief Get output queue usage statistics
 * 
 * @param[out] count Current number of messages in queue
 * @param[out] size Queue capacity
 */
void output_queue_stats(uint32_t *count, uint32_t *size) {
    if (s_output_queue) {
        *count = uxQueueMessagesWaiting(s_output_queue);
        *size = uxQueueSpacesAvailable(s_output_queue) + *count;
    } else {
        *count = 0;
        *size = 0;
    }
}

/**
 * @brief Initialize output subsystem with configuration
 * 
 * Sets output mode and parameters. For CSV mode, prints column headers.
 * Also calls output_system_info() for startup banner.
 * 
 * @param[in] config Pointer to output configuration (NULL for defaults)
 * 
 * @note If config is NULL, uses default configuration
 * @note CSV header is printed only once per session
 */
bool output_init(const output_config_t *config) {
    if (!output_task_init(config)) {
        ESP_LOGW(TAG, "Output task initialization failed, falling back to synchronous mode");
        
        // Fall back to synchronous mode
        if (config) {
            s_config = *config;
        }
        
        if (s_config.mode == OUTPUT_MODE_CSV) {
            printf("timestamp_us,window_id,sample_rate");
            for (int i = 0; i < WINDOW_SIZE; i++) {
                printf(",sample_%d", i);
            }
            printf(",features");
            for (int i = 0; i < FEATURE_VECTOR_SIZE; i++) {
                printf(",feature_%d", i);
            }
            printf(",signal_type,confidence,inference_time_us\n");
            s_csv_header_printed = true;
        }
        
        output_system_info();
    }
    
    return true;
}

/**
 * @brief Cleanup output subsystem
 * 
 * Stops output task and deletes queue.
 */
void output_cleanup(void) {
    s_output_running = false;
    if (s_output_task_handle) {
        vTaskDelete(s_output_task_handle);
        s_output_task_handle = NULL;
    }
    if (s_output_queue) {
        vQueueDelete(s_output_queue);
        s_output_queue = NULL;
    }
    ESP_LOGI(TAG, "Output subsystem cleaned up");
}