/**
 * @file benchmark.c
 * @brief Performance measurement and system monitoring
 * @details Tracks timing, CPU usage, memory usage, and reliability metrics
 *          for the signal processing pipeline.
 * 
 * @author Your Name
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Uses ESP timer for microsecond timing
 * @note CPU usage estimation is simplified; refine for production
 * 
 * @copyright (c) 2025 ESP32 Signal Processing Project
 */

#include "benchmark.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <stdio.h>

static const char *TAG = "BENCHMARK";                   /**< Logging tag */

/**
 * @brief Benchmark state structure
 */
static benchmark_metrics_t s_metrics = {0};              /**< Global metrics storage */
static uint64_t s_window_start_time = 0;                 /**< Start time of current window */
static uint64_t s_inference_start_time = 0;              /**< Start time of current inference */
static uint64_t s_last_cpu_update = 0;                   /**< Last CPU usage update time */
static uint32_t s_idle_count = 0;                        /**< Estimated idle counts */
static uint32_t s_total_count = 0;                       /**< Total update counts */

/**
 * @brief Initialize benchmark subsystem
 * 
 * Resets all metrics and timers.
 * 
 * @post s_metrics zeroed, min times set to UINT64_MAX
 */
void benchmark_init(void) {
    memset(&s_metrics, 0, sizeof(s_metrics));
    s_metrics.min_window_time_us = UINT64_MAX;
    s_metrics.min_inference_time_us = UINT64_MAX;
    s_last_cpu_update = esp_timer_get_time();
    ESP_LOGI(TAG, "Benchmarking initialized");
}

/**
 * @brief Start timing a window processing cycle
 * 
 * Records current timestamp for later duration calculation.
 * 
 * @pre Called at start of window processing
 * @post s_window_start_time updated
 */
void benchmark_start_window(void) {
    s_window_start_time = esp_timer_get_time();
}

/**
 * @brief End timing a window processing cycle
 * 
 * Calculates duration and updates window processing statistics.
 * 
 * @pre benchmark_start_window() must have been called
 * @post s_metrics updated with new window timing data
 */
void benchmark_end_window(void) {
    uint64_t end_time = esp_timer_get_time();
    uint64_t duration = end_time - s_window_start_time;
    
    s_metrics.windows_processed++;
    s_metrics.total_processing_time_us += duration;
    
    if (duration > s_metrics.max_window_time_us) {
        s_metrics.max_window_time_us = duration;
    }
    if (duration < s_metrics.min_window_time_us) {
        s_metrics.min_window_time_us = duration;
    }
    
    s_metrics.avg_processing_time_us = 
        (float)s_metrics.total_processing_time_us / s_metrics.windows_processed;
}

/**
 * @brief Start timing an inference operation
 * 
 * Records current timestamp for later duration calculation.
 * 
 * @pre Called at start of inference
 * @post s_inference_start_time updated
 */
void benchmark_start_inference(void) {
    s_inference_start_time = esp_timer_get_time();
}

/**
 * @brief End timing an inference operation
 * 
 * Calculates duration and updates inference statistics using moving average.
 * 
 * @pre benchmark_start_inference() must have been called
 * @post s_metrics updated with new inference timing data
 */
void benchmark_end_inference(void) {
    uint64_t end_time = esp_timer_get_time();
    uint64_t duration = end_time - s_inference_start_time;
    
    s_metrics.inferences_completed++;
    
    if (duration > s_metrics.max_inference_time_us) {
        s_metrics.max_inference_time_us = duration;
    }
    if (duration < s_metrics.min_inference_time_us) {
        s_metrics.min_inference_time_us = duration;
    }
    
    s_metrics.avg_inference_time_us = 
        (s_metrics.avg_inference_time_us * (s_metrics.inferences_completed - 1) + 
         duration) / s_metrics.inferences_completed;
}

/**
 * @brief Update dynamic metrics (CPU, memory, etc.)
 * 
 * Called periodically to refresh CPU usage, memory usage, and other
 * dynamic system metrics.
 * 
 * @note CPU usage estimation is simplified (simulates idle detection)
 * @note Memory usage from esp_get_free_heap_size()
 */
void benchmark_update_metrics(void) {
    // Update CPU usage (simplified)
    uint64_t now = esp_timer_get_time();
    if (now - s_last_cpu_update > 1000000) {  // Every second
        // Simple CPU usage estimation
        s_metrics.cpu_usage_percent = 
            (1.0f - (float)s_idle_count / s_total_count) * 100.0f;
        
        s_idle_count = 0;
        s_total_count = 0;
        s_last_cpu_update = now;
    }
    
    // Update memory usage
    s_metrics.memory_usage_kb = esp_get_free_heap_size() / 1024.0f;
    
    // Increment counters for CPU usage calculation
    s_total_count++;
    // This would normally check if we're in idle task
    // For now, we'll simulate idle detection
    if (s_total_count % 10 == 0) {
        s_idle_count++;
    }
}

/**
 * @brief Get current benchmark metrics
 * 
 * Updates dynamic metrics and returns a copy of the current metrics.
 * 
 * @return benchmark_metrics_t Current performance metrics
 */
benchmark_metrics_t benchmark_get_metrics(void) {
    benchmark_update_metrics();
    return s_metrics;
}

/**
 * @brief Log benchmark summary via ESP_LOGI
 * 
 * Prints formatted metrics to log output.
 */
void benchmark_log_summary(void) {
    benchmark_update_metrics();
    
    ESP_LOGI(TAG, "=== BENCHMARK SUMMARY ===");
    ESP_LOGI(TAG, "Windows processed: %u", s_metrics.windows_processed);
    ESP_LOGI(TAG, "Inferences completed: %u", s_metrics.inferences_completed);
    ESP_LOGI(TAG, "Avg window time: %.2f us", s_metrics.avg_processing_time_us);
    ESP_LOGI(TAG, "Avg inference time: %.2f us", s_metrics.avg_inference_time_us);
    ESP_LOGI(TAG, "Max inference time: %llu us", s_metrics.max_inference_time_us);
    ESP_LOGI(TAG, "Min inference time: %llu us", s_metrics.min_inference_time_us);
    ESP_LOGI(TAG, "CPU Usage: %.1f%%", s_metrics.cpu_usage_percent);
    ESP_LOGI(TAG, "Memory Usage: %.1f KB", s_metrics.memory_usage_kb);
    ESP_LOGI(TAG, "Missed windows: %u", s_metrics.missed_windows);
    ESP_LOGI(TAG, "Buffer overruns: %u", s_metrics.buffer_overruns);
    ESP_LOGI(TAG, "==========================");
}

/**
 * @brief Reset all benchmark metrics to zero
 * 
 * @post All metrics cleared, min times reset to UINT64_MAX
 */
void benchmark_reset(void) {
    memset(&s_metrics, 0, sizeof(s_metrics));
    s_metrics.min_window_time_us = UINT64_MAX;
    s_metrics.min_inference_time_us = UINT64_MAX;
    ESP_LOGI(TAG, "Benchmark reset");
}