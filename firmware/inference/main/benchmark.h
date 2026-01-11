#ifndef MODEL_BENCHMARK_H
#define MODEL_BENCHMARK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MODEL_TYPE_CNN,
    MODEL_TYPE_RNN,
    MODEL_TYPE_CNN_INT8,
    MODEL_TYPE_RNN_INT8,
    MODEL_TYPE_HEURISTIC,
    MODEL_TYPE_COUNT
} model_type_t;

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
 * @brief Initialize model benchmarking
 */
void model_benchmark_init(void);

/**
 * @brief Run benchmark for all models
 * 
 * @param samples Test samples
 * @param num_samples Number of samples
 * @param ground_truth Ground truth label
 */
void model_run_benchmark(float *samples, int num_samples, const char *ground_truth);

/**
 * @brief Get benchmark results
 * 
 * @param results Array to store results
 * @param max_results Maximum number of results
 * @return int Number of results filled
 */
int model_get_benchmark_results(model_benchmark_t *results, int max_results);

/**
 * @brief Log benchmark results
 */
void model_log_benchmark_results(void);

/**
 * @brief Get recommended model based on constraints
 * 
 * @param max_flash_kb Maximum flash size in KB
 * @param max_ram_kb Maximum RAM in KB
 * @param min_accuracy Minimum required accuracy
 * @return model_type_t Recommended model type
 */
model_type_t model_get_recommended(size_t max_flash_kb, size_t max_ram_kb, float min_accuracy);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_BENCHMARK_H */