/**
 * @file benchmark.h
 * @brief Header for performance measurement and system monitoring
 * @details Defines metrics structures and API for timing, CPU usage,
 *          memory usage, and reliability tracking of signal processing pipeline.
 * 
 * @author Darkhan Zhanibekuly
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Uses ESP timer for microsecond precision timing
 * @note CPU usage estimation is simplified; refine for production use
 */

#pragma once

#include <stdint.h>
#include "signal_acquisition.h"
#include "inference.h"

/**
 * @brief Benchmark metrics structure
 * 
 * Comprehensive performance and reliability metrics collected during
 * system operation. All timing values in microseconds.
 */
typedef struct {
    // Timing statistics
    uint64_t total_processing_time_us;    /**< Cumulative window processing time */
    uint64_t max_window_time_us;          /**< Maximum single-window processing time */
    uint64_t min_window_time_us;          /**< Minimum single-window processing time */
    uint64_t max_inference_time_us;       /**< Maximum single-inference time */
    uint64_t min_inference_time_us;       /**< Minimum single-inference time */
    
    // Performance metrics
    uint32_t windows_processed;           /**< Total windows processed */
    uint32_t inferences_completed;        /**< Total inferences completed */
    float avg_processing_time_us;         /**< Average window processing time */
    float avg_inference_time_us;          /**< Average inference time (moving average) */
    float cpu_usage_percent;              /**< Estimated CPU usage percentage */
    float memory_usage_kb;                /**< Current heap memory usage in KB */
    
    // Reliability metrics
    uint32_t missed_windows;              /**< Windows lost due to processing delays */
    uint32_t buffer_overruns;             /**< Circular buffer overrun events */
    uint32_t inference_errors;            /**< Inference execution errors */
} benchmark_metrics_t;

/**
 * @brief Initialize benchmark subsystem
 * 
 * Resets all metrics and timers to initial state.
 * 
 * @post All metrics zeroed, min times set to UINT64_MAX
 */
void benchmark_init(void);

/**
 * @brief Start timing a window processing cycle
 * 
 * Records current timestamp for window processing duration calculation.
 * 
 * @pre Should be called at start of window processing
 * @post Internal timer started for window processing
 */
void benchmark_start_window(void);

/**
 * @brief End timing a window processing cycle
 * 
 * Calculates duration and updates window processing statistics.
 * 
 * @pre benchmark_start_window() must have been called
 * @post Window timing metrics updated
 */
void benchmark_end_window(void);

/**
 * @brief Start timing an inference operation
 * 
 * Records current timestamp for inference duration calculation.
 * 
 * @pre Should be called at start of inference
 * @post Internal timer started for inference
 */
void benchmark_start_inference(void);

/**
 * @brief End timing an inference operation
 * 
 * Calculates duration and updates inference statistics.
 * 
 * @pre benchmark_start_inference() must have been called
 * @post Inference timing metrics updated with moving average
 */
void benchmark_end_inference(void);

/**
 * @brief Update dynamic metrics (CPU, memory, etc.)
 * 
 * Refreshes CPU usage, memory usage, and other dynamic system metrics.
 * Should be called periodically or before retrieving metrics.
 * 
 * @note CPU usage estimation is simplified (simulates idle detection)
 * @note Memory usage from esp_get_free_heap_size()
 */
void benchmark_update_metrics(void);

/**
 * @brief Get current benchmark metrics
 * 
 * Updates dynamic metrics and returns a copy of current performance metrics.
 * 
 * @return benchmark_metrics_t Current benchmark metrics
 */
benchmark_metrics_t benchmark_get_metrics(void);

/**
 * @brief Log benchmark summary via ESP_LOGI
 * 
 * Prints formatted metrics to ESP32 log output.
 * 
 * @note Uses ESP_LOGI for consistent system logging
 */
void benchmark_log_summary(void);

/**
 * @brief Reset all benchmark metrics to zero
 * 
 * Clears all accumulated metrics and resets min/max values.
 * 
 * @post All metrics zeroed, min times set to UINT64_MAX
 */
void benchmark_reset(void);