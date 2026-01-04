/**
 * @file benchmark.c
 * @brief Performance measurement and system monitoring
 * @details Tracks timing, CPU usage, memory usage, and reliability metrics
 *          for the signal processing pipeline.
 * 
 * @author Darkhan Zhanibekuly
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Uses ESP timer for microsecond timing
 * @note CPU usage estimation is simplified; refine for production
 */

#include "benchmark.h"
#include "common.h"
#include "statistics.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "BENCHMARK";

/**
 * @brief Benchmark state structure
 */
static benchmark_metrics_t s_metrics = {0};
static uint64_t s_window_start_time = 0;
static uint64_t s_inference_start_time = 0;
static uint64_t s_last_cpu_update = 0;
static uint32_t s_idle_count = 0;
static uint32_t s_total_count = 0;
static distribution_stats_t s_window_time_stats;
static rate_calculator_t s_window_rate_calc;
static histogram_t s_inference_time_hist;

// Use common statistics utilities
static moving_average_t s_window_time_avg = {0};
static moving_average_t s_inference_time_avg = {0};
static min_max_tracker_t s_window_time_tracker = {0};
static min_max_tracker_t s_inference_time_tracker = {0};

/**
 * @brief Initialize benchmark subsystem
 */
void benchmark_init(void) {
    memset(&s_metrics, 0, sizeof(s_metrics));
    
    // Initialize trackers
    s_metrics.min_window_time_us = UINT64_MAX;
    s_metrics.min_inference_time_us = UINT64_MAX;
    
    // Initialize common utilities
    moving_average_init(&s_window_time_avg);
    moving_average_init(&s_inference_time_avg);
    min_max_tracker_init(&s_window_time_tracker);
    min_max_tracker_init(&s_inference_time_tracker);
    distribution_stats_init(&s_window_time_stats);
    rate_calculator_init(&s_window_rate_calc, 1000); // 1-second window
    histogram_init(&s_inference_time_hist, 0.0f, 10000.0f, 20); // 0-10ms, 20 bins

    
    s_last_cpu_update = get_time_us();
    ESP_LOGI(TAG, "Benchmarking initialized");
}

/**
 * @brief Start timing a window processing cycle
 */
void benchmark_start_window(void) {
    s_window_start_time = get_time_us();
}

/**
 * @brief End timing a window processing cycle
 */
void benchmark_end_window(void) {
    uint64_t end_time = get_time_us();
    uint64_t duration = end_time - s_window_start_time;
    
    s_metrics.windows_processed++;
    s_metrics.total_processing_time_us += duration;
    
    // Update min/max using common tracker
    min_max_tracker_update(&s_window_time_tracker, (float)duration);
    
    // Update average using common utility
    moving_average_update(&s_window_time_avg, (float)duration);

    distribution_stats_add(&s_window_time_stats, (float)duration);
    
    // Update metrics structure
    s_metrics.max_window_time_us = (uint64_t)s_window_time_tracker.max;
    s_metrics.min_window_time_us = (uint64_t)s_window_time_tracker.min;
    s_metrics.avg_processing_time_us = s_window_time_avg.value;
}

/**
 * @brief Start timing an inference operation
 */
void benchmark_start_inference(void) {
    s_inference_start_time = get_time_us();
}

/**
 * @brief End timing an inference operation
 */
void benchmark_end_inference(void) {
    uint64_t end_time = get_time_us();
    uint64_t duration = end_time - s_inference_start_time;
    
    s_metrics.inferences_completed++;
    
    // Update min/max using common tracker
    min_max_tracker_update(&s_inference_time_tracker, (float)duration);
    
    // Update average using common utility
    moving_average_update(&s_inference_time_avg, (float)duration);

    histogram_add(&s_inference_time_hist, (float)duration);
    
    // Update metrics structure
    s_metrics.max_inference_time_us = (uint64_t)s_inference_time_tracker.max;
    s_metrics.min_inference_time_us = (uint64_t)s_inference_time_tracker.min;
    s_metrics.avg_inference_time_us = s_inference_time_avg.value;
}

/**
 * @brief Update dynamic metrics (CPU, memory, etc.)
 */
void benchmark_update_metrics(void) {
    // Update CPU usage (simplified)
    uint64_t now = get_time_us();
    if (now - s_last_cpu_update > 1000000) {  // Every second
        if (s_total_count > 0) {
            s_metrics.cpu_usage_percent = 
                (1.0f - (float)s_idle_count / s_total_count) * 100.0f;
        } else {
            s_metrics.cpu_usage_percent = 0.0f;
        }
        
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
 */
benchmark_metrics_t benchmark_get_metrics(void) {
    benchmark_update_metrics();
    return s_metrics;
}

/**
 * @brief Log benchmark summary via ESP_LOGI
 */
void benchmark_log_summary(void) {
    benchmark_update_metrics();
    
    // Use common logging macro
    LOG_INFO(TAG, "=== BENCHMARK SUMMARY ===");
    LOG_INFO(TAG, "Windows processed: %u", s_metrics.windows_processed);
    LOG_INFO(TAG, "Inferences completed: %u", s_metrics.inferences_completed);
    LOG_INFO(TAG, "Avg window time: %.2f us", s_metrics.avg_processing_time_us);
    LOG_INFO(TAG, "Avg inference time: %.2f us", s_metrics.avg_inference_time_us);
    LOG_INFO(TAG, "Max inference time: %llu us", s_metrics.max_inference_time_us);
    LOG_INFO(TAG, "Min inference time: %llu us", s_metrics.min_inference_time_us);
    LOG_INFO(TAG, "CPU Usage: %.1f%%", s_metrics.cpu_usage_percent);
    LOG_INFO(TAG, "Memory Usage: %.1f KB", s_metrics.memory_usage_kb);
    LOG_INFO(TAG, "Missed windows: %u", s_metrics.missed_windows);
    LOG_INFO(TAG, "Buffer overruns: %u", s_metrics.buffer_overruns);
    LOG_INFO(TAG, "==========================");
}

/**
 * @brief Reset all benchmark metrics to zero
 */
void benchmark_reset(void) {
    memset(&s_metrics, 0, sizeof(s_metrics));
    
    // Reset common utilities
    moving_average_reset(&s_window_time_avg);
    moving_average_reset(&s_inference_time_avg);
    min_max_tracker_reset(&s_window_time_tracker);
    min_max_tracker_reset(&s_inference_time_tracker);
    
    s_metrics.min_window_time_us = UINT64_MAX;
    s_metrics.min_inference_time_us = UINT64_MAX;
    
    ESP_LOGI(TAG, "Benchmark reset");
}

/**
 * @brief Calculate processing rate in windows per second
 */
float benchmark_get_window_rate(void) {
    if (s_metrics.avg_processing_time_us > 0) {
        return 1000000.0f / s_metrics.avg_processing_time_us;
    }
    return 0.0f;
}

/**
 * @brief Calculate inference rate in inferences per second
 */
float benchmark_get_inference_rate(void) {
    if (s_metrics.avg_inference_time_us > 0) {
        return 1000000.0f / s_metrics.avg_inference_time_us;
    }
    return 0.0f;
}