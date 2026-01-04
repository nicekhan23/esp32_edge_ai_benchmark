/**
 * @file output.h
 * @brief Header for multi-format output and logging system
 */

#pragma once

#include "signal_acquisition.h"
#include "feature_extraction.h"
#include "inference.h"
#include "benchmark.h"

/**
 * @brief Output mode enumeration
 */
typedef enum {
    OUTPUT_MODE_CSV,
    OUTPUT_MODE_JSON,
    OUTPUT_MODE_HUMAN,
    OUTPUT_MODE_SILENT
} output_mode_t;

/**
 * @brief Output configuration structure
 */
typedef struct {
    output_mode_t mode;
    bool print_raw_data;
    bool print_features;
    bool print_inference;
    bool print_stats;
    uint32_t output_interval_ms;
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

// Function declarations (removed duplicate output_window_validation)
bool output_task_init(const output_config_t *config);
bool output_queue_send(output_message_t *msg);
QueueHandle_t output_get_queue(void);
bool output_queue_is_full(void);
void output_queue_stats(uint32_t *count, uint32_t *size);
bool output_init(const output_config_t *config);
void output_set_mode(output_mode_t mode);
void output_raw_window(const window_buffer_t *window);
void output_features(const feature_vector_t *features);
void output_inference_result(const inference_result_t *result);
void output_window_validation(const window_buffer_t *window, const feature_vector_t *features);  // SINGLE DECLARATION
void output_benchmark_summary(const benchmark_metrics_t *metrics);
void output_acquisition_stats(const acquisition_stats_t *stats);
void output_inference_stats(const inference_stats_t *stats);
void output_system_info(void);
void output_flush(void);
void output_cleanup(void);
void output_ml_dataset_row(const window_buffer_t *window, 
                           const feature_vector_t *features,
                           const inference_result_t *result);