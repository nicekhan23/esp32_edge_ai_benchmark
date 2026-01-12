#ifndef MODEL_BENCHMARK_H
#define MODEL_BENCHMARK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Model types that match your benchmark.c
typedef enum {
    MODEL_CNN_FLOAT32,
    MODEL_CNN_INT8,
    MODEL_MLP_FLOAT32,
    MODEL_MLP_INT8,
    MODEL_HYBRID_FLOAT32,
    MODEL_HYBRID_INT8,
    MODEL_TYPE_COUNT
} model_type_t;

// Simplified benchmark result structure
typedef struct {
    model_type_t type;
    const char *name;
    float accuracy;
    uint32_t inference_time_us;
    size_t flash_size_kb;
    size_t ram_usage_kb;
    uint32_t test_count;
} model_benchmark_t;

/**
 * @brief Initialize benchmark system
 */
void model_benchmark_init(void);

/**
 * @brief Run benchmark suite for all models
 * 
 * @param samples Test samples
 * @param num_samples Number of samples
 * @param ground_truth Ground truth label
 */
void run_benchmark_suite(float *samples, int num_samples, const char *ground_truth);

/**
 * @brief Get benchmark results
 * 
 * @param results Array to store results
 * @param max_results Maximum number of results to return
 * @return Number of results actually returned
 */
int model_get_benchmark_results(model_benchmark_t *results, int max_results);

/**
 * @brief Get recommended model based on constraints
 * 
 * @param max_flash_kb Maximum flash size in KB
 * @param max_ram_kb Maximum RAM in KB
 * @param min_accuracy Minimum required accuracy (0.0-1.0)
 * @return Recommended model type
 */
model_type_t model_get_recommended(size_t max_flash_kb, size_t max_ram_kb, float min_accuracy);

/**
 * @brief Log benchmark results to console
 */
void model_log_benchmark_results(void);

/**
 * @brief Run benchmark with current samples
 * 
 * @param samples Test samples
 * @param num_samples Number of samples
 * @param ground_truth Ground truth label
 */
void model_run_benchmark(float *samples, int num_samples, const char *ground_truth);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_BENCHMARK_H */