#ifndef INFERENCE_H
#define INFERENCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "clock_sync.h"
#include "benchmark.h"

#ifdef __cplusplus
extern "C" {
#endif

// Inference modes
typedef enum {
    INFERENCE_MODE_TFLITE,
    INFERENCE_MODE_SIMULATED,
    INFERENCE_MODE_HEURISTIC,
    INFERENCE_MODE_FFT_BASED
} inference_mode_t;

// Inference configuration
typedef struct {
    inference_mode_t mode;
    model_type_t model_type;  // ✅ Use existing type from benchmark.h
    float confidence_threshold;
    uint32_t voting_window;
    bool enable_voting;
    bool enable_fft;
} inference_config_t;

// Inference result
typedef struct {
    char predicted_class[32];
    float confidence;
    float *probabilities;
    int num_classes;
    uint32_t timestamp_ms;
    bool is_voted_result;
} inference_result_t;

// Inference engine
typedef struct {
    void *model_data;
    size_t model_size;
    inference_mode_t mode;
    void *interpreter;
    void *input_tensor;
    void *output_tensor;
    bool initialized;
    inference_config_t config;
} inference_engine_t;

// Feature extraction structure
typedef struct {
    float zero_crossing_rate;
    float crest_factor;       // Peak / RMS
    float form_factor;        // RMS / average rectified
    float harmonic_ratio;     // Energy in harmonics vs fundamental
    float symmetry_score;     // For square/triangle waves
    float dominant_frequency;
} signal_features_t;

/**
 * @brief Initialize inference engine with configuration
 * 
 * @param engine Pointer to inference engine
 * @param config Inference configuration
 * @return true if initialization successful
 */
bool inference_init(inference_engine_t *engine, inference_config_t *config);

/**
 * @brief Run inference on samples
 * 
 * @param engine Inference engine
 * @param samples Input samples
 * @param num_samples Number of samples
 * @param result Inference result
 * @return true if inference successful
 */
bool inference_run(inference_engine_t *engine, 
                   float *samples, 
                   int num_samples, 
                   inference_result_t *result);

/**
 * @brief Run inference with voting system
 * 
 * @param engine Inference engine
 * @param config Inference configuration
 * @param samples Input samples (circular buffer)
 * @param buffer_size Total buffer size
 * @param final_result Final voted result
 * @return true if successful
 */
bool inference_run_with_voting(inference_engine_t *engine,
                               inference_config_t *config,
                               float *samples,
                               int buffer_size,
                               inference_result_t *final_result);

/**
 * @brief Extract features from signal for heuristic classification
 * 
 * @param samples Input samples
 * @param num_samples Number of samples
 * @param features Extracted features
 */
void extract_features(float *samples, int num_samples, signal_features_t *features);

/**
 * @brief Process inference result (logging, metrics, etc.)
 * 
 * @param result Inference result
 * @param ground_truth Ground truth label (if available)
 * @param sync Clock synchronization info
 */
void process_inference_result(inference_result_t *result, 
                              const char *ground_truth,
                              clock_sync_t *sync);

/**
 * @brief Convert class name to index
 * 
 * @param class_name Class name string
 * @return int Class index (-1 if not found)
 */
int class_name_to_index(const char *class_name);

/**
 * @brief Deinitialize inference engine
 * 
 * @param engine Inference engine to deinitialize
 */
void inference_deinit(inference_engine_t *engine);

/**
 * @brief Get memory usage of inference engine
 * 
 * @param engine Inference engine
 * @param ram_kb RAM usage in KB
 * @param flash_kb Flash usage in KB
 */
void inference_get_memory_usage(inference_engine_t *engine, 
                                size_t *ram_kb, 
                                size_t *flash_kb);

#ifdef __cplusplus
}
#endif

#endif /* INFERENCE_H */