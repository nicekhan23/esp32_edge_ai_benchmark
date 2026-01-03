/**
 * @file output.h
 * @brief Header for multi-format output and logging system
 * @details Defines output modes, configuration structures, and API functions
 *          for formatted data presentation in CSV, JSON, human-readable, and silent modes.
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

#pragma once

#include "signal_acquisition.h"
#include "feature_extraction.h"
#include "inference.h"
#include "benchmark.h"

/**
 * @brief Output mode enumeration
 * 
 * Defines available output formats for data presentation.
 */
typedef enum {
    OUTPUT_MODE_CSV,     /**< Comma-separated values format (machine-readable) */
    OUTPUT_MODE_JSON,    /**< JSON format (structured data interchange) */
    OUTPUT_MODE_HUMAN,   /**< Human-readable text format (console output) */
    OUTPUT_MODE_SILENT   /**< No output (for performance testing) */
} output_mode_t;

/**
 * @brief Output configuration structure
 * 
 * Configurable parameters controlling what data is output and in what format.
 */
typedef struct {
    output_mode_t mode;               /**< Output format/mode */
    bool print_raw_data;              /**< Enable/disable raw sample output */
    bool print_features;              /**< Enable/disable feature vector output */
    bool print_inference;             /**< Enable/disable inference result output */
    bool print_stats;                 /**< Enable/disable statistics output */
    uint32_t output_interval_ms;      /**< Interval for periodic statistics (milliseconds) */
} output_config_t;

/**
 * @brief Output message type enumeration
 */
typedef enum {
    OUTPUT_RAW_WINDOW,
    OUTPUT_FEATURES,
    OUTPUT_INFERENCE,
    OUTPUT_BENCHMARK_SUMMARY,
    OUTPUT_ACQUISITION_STATS,
    OUTPUT_INFERENCE_STATS,
    OUTPUT_SYSTEM_INFO,
    OUTPUT_TYPE_COUNT
} output_message_type_t;

/**
 * @brief Output message structure
 */
typedef struct {
    output_message_type_t type;
    union {
        window_buffer_t window;
        feature_vector_t features;
        inference_result_t inference;
        benchmark_metrics_t benchmark;
        acquisition_stats_t acq_stats;
        inference_stats_t inf_stats;
    } data;
    uint64_t timestamp_us;
} output_message_t;

/**
 * @brief Initialize output task with queue
 * 
 * Creates the output task and message queue for asynchronous output.
 * 
 * @param[in] config Output configuration
 * @return true if initialization successful, false otherwise
 */
bool output_task_init(const output_config_t *config);

/**
 * @brief Send message to output queue
 * 
 * Non-blocking send to output queue for asynchronous processing.
 * 
 * @param[in] msg Pointer to output message
 * @return true if message queued successfully, false if queue full
 */
bool output_queue_send(output_message_t *msg);

/**
 * @brief Get output queue handle
 * 
 * @return QueueHandle_t Handle to output message queue
 */
QueueHandle_t output_get_queue(void);

/**
 * @brief Check if output queue is full
 * 
 * @return true if queue is full, false otherwise
 */
bool output_queue_is_full(void);

/**
 * @brief Get output queue usage statistics
 * 
 * @param[out] count Current number of messages in queue
 * @param[out] size Queue capacity
 */
void output_queue_stats(uint32_t *count, uint32_t *size);

/**
 * @brief Initialize output subsystem
 * 
 * Sets output mode and parameters, prints CSV header if needed,
 * and displays system information banner.
 * 
 * @param[in] config Pointer to output configuration (NULL for defaults)
 * 
 * @note If config is NULL, uses default human-readable configuration
 * @note CSV header is printed only once per session
 */
bool output_init(const output_config_t *config);

/**
 * @brief Dynamically change output mode
 * 
 * @param[in] mode New output mode
 * 
 * @note Does not affect other configuration parameters
 */
void output_set_mode(output_mode_t mode);

/**
 * @brief Output raw window data
 * 
 * Formats and prints raw ADC samples according to current output mode.
 * 
 * @param[in] window Pointer to window buffer structure
 * 
 * @note Respects print_raw_data configuration flag
 * @note In CSV mode, prints without newline for continuation
 */
void output_raw_window(const window_buffer_t *window);

/**
 * @brief Output feature vector
 * 
 * Formats and prints feature vector data according to current output mode.
 * 
 * @param[in] features Pointer to feature vector structure
 * 
 * @note Respects print_features configuration flag
 * @note In CSV mode, assumes called after output_raw_window on same line
 */
void output_features(const feature_vector_t *features);

/**
 * @brief Output inference result
 * 
 * Formats and prints classification results with confidence and timing.
 * 
 * @param[in] result Pointer to inference result structure
 * 
 * @note Respects print_inference configuration flag
 * @note In CSV mode, completes line and flushes output
 */
void output_inference_result(const inference_result_t *result);

/**
 * @brief Output detailed validation metrics for a window
 * 
 * Prints min/max, mean, peak-to-peak, estimated amplitude,
 * RMS, ZCR, skewness, crest factor, and expected frequency.
 * 
 * @param[in] window Pointer to window buffer structure
 * @param[in] features Pointer to feature vector structure
 * @note Intended for debugging and validation purposes
 */
void output_window_validation(const window_buffer_t *window, const feature_vector_t *features);

/**
 * @brief Output benchmark performance summary
 * 
 * Prints formatted performance metrics including processing rates,
 * CPU/memory usage, and error counts.
 * 
 * @param[in] metrics Pointer to benchmark metrics structure
 * 
 * @note Respects print_stats configuration flag
 */
void output_benchmark_summary(const benchmark_metrics_t *metrics);

/**
 * @brief Output acquisition subsystem statistics
 * 
 * Prints counts of samples, windows, buffer overruns, and sampling errors.
 * 
 * @param[in] stats Pointer to acquisition statistics structure
 * 
 * @note Always prints regardless of print_stats flag (critical system info)
 */
void output_acquisition_stats(const acquisition_stats_t *stats);

/**
 * @brief Output inference subsystem statistics
 * 
 * Prints inference counts, average time, accuracy, and per-signal-type breakdown.
 * 
 * @param[in] stats Pointer to inference statistics structure
 * 
 * @note Uses signal_type_to_string for type labels
 */
void output_inference_stats(const inference_stats_t *stats);

/**
 * @brief Output system configuration and startup banner
 * 
 * Prints fixed system parameters: sampling rate, window size, overlap,
 * feature vector size, and current output mode.
 * 
 * @note Called automatically by output_init()
 */
void output_system_info(void);

/**
 * @brief Flush output buffer
 * 
 * Ensures all pending stdout data is written immediately.
 * Useful for real-time streaming and log integrity.
 */
void output_flush(void);

/**
 * @brief Cleanup output subsystem resources
 * 
 * Stops output task and frees queue memory.
 */
void output_cleanup(void);

/**
 * @brief Output detailed validation metrics for a window
 * 
 * Prints min/max, mean, peak-to-peak, estimated amplitude,
 * RMS, ZCR, skewness, crest factor, and expected frequency.
 * 
 * @param[in] window Pointer to window buffer structure
 * @param[in] features Pointer to feature vector structure
 * @note Intended for debugging and validation purposes
 */
void output_window_validation(const window_buffer_t *window, const feature_vector_t *features);

/**
 * @brief Output ML dataset row in CSV format
 * 
 * Prints a complete CSV row with timestamp, window ID, label,
 * sample rate, features, inference result, and raw samples.
 * 
 * @param[in] window Pointer to window buffer structure
 * @param[in] features Pointer to feature vector structure
 * @param[in] result Pointer to inference result structure
 * 
 * @note Always prints regardless of output mode (for logging)
 */
void output_ml_dataset_row(const window_buffer_t *window, 
                           const feature_vector_t *features,
                           const inference_result_t *result);