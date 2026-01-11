// benchmark.c - Optimized version
#include "benchmark.h"
#include "inference.h"
#include "system_monitor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "BENCHMARK";

// Define MODEL constants if not already defined in benchmark.h
#ifndef MODEL_CNN_FLOAT32
#define MODEL_CNN_FLOAT32 0
#define MODEL_CNN_INT8    1
#define MODEL_MLP_FLOAT32 2
#define MODEL_MLP_INT8    3
#define MODEL_HYBRID_FLOAT32 4
#define MODEL_HYBRID_INT8 5
#endif

typedef struct {
    int model_type;
    const char* model_name;
    void* (*get_model_data)(void);
    size_t (*get_model_size)(void);
    inference_mode_t inference_mode;
    bool is_quantized;
} benchmark_model_config_t;

// Add these stub definitions if you don't have actual model data
static const unsigned char cnn_int8_model_data[1] = {0};
static const size_t cnn_int8_model_size = 0;
static const unsigned char cnn_float32_model_data[1] = {0};
static const size_t cnn_float32_model_size = 0;
static const unsigned char mlp_int8_model_data[1] = {0};
static const size_t mlp_int8_model_size = 0;
static const unsigned char mlp_float32_model_data[1] = {0};
static const size_t mlp_float32_model_size = 0;
static const unsigned char hybrid_int8_model_data[1] = {0};
static const size_t hybrid_int8_model_size = 0;
static const unsigned char hybrid_float32_model_data[1] = {0};
static const size_t hybrid_float32_model_size = 0;

// REMOVE THE DUPLICATE FUNCTIONS BELOW - KEEP ONLY THESE:

// Model data getter functions - FIXED SIGNATURES (no bool parameter)
static void* get_cnn_model_float(void) {
    return (void*)cnn_float32_model_data;
}

static size_t get_cnn_size_float(void) {
    return cnn_float32_model_size;
}

static void* get_cnn_model_int8(void) {
    return (void*)cnn_int8_model_data;
}

static size_t get_cnn_size_int8(void) {
    return cnn_int8_model_size;
}

static void* get_mlp_model_float(void) {
    return (void*)mlp_float32_model_data;
}

static size_t get_mlp_size_float(void) {
    return mlp_float32_model_size;
}

static void* get_mlp_model_int8(void) {
    return (void*)mlp_int8_model_data;
}

static size_t get_mlp_size_int8(void) {
    return mlp_int8_model_size;
}

static void* get_hybrid_model_float(void) {
    return (void*)hybrid_float32_model_data;
}

static size_t get_hybrid_size_float(void) {
    return hybrid_float32_model_size;
}

static void* get_hybrid_model_int8(void) {
    return (void*)hybrid_int8_model_data;
}

static size_t get_hybrid_size_int8(void) {
    return hybrid_int8_model_size;
}

// All model configurations will go here
static benchmark_model_config_t benchmark_configs[] = {
    {MODEL_CNN_FLOAT32, "CNN_F32", get_cnn_model_float, get_cnn_size_float, INFERENCE_MODE_TFLITE, false},
    {MODEL_CNN_INT8,    "CNN_INT8", get_cnn_model_int8, get_cnn_size_int8, INFERENCE_MODE_TFLITE, true},
    {MODEL_MLP_FLOAT32, "MLP_F32", get_mlp_model_float, get_mlp_size_float, INFERENCE_MODE_TFLITE, false},
    {MODEL_MLP_INT8,    "MLP_INT8", get_mlp_model_int8, get_mlp_size_int8, INFERENCE_MODE_TFLITE, true},
    {MODEL_HYBRID_FLOAT32, "HYB_F32", get_hybrid_model_float, get_hybrid_size_float, INFERENCE_MODE_TFLITE, false},
    {MODEL_HYBRID_INT8, "HYB_INT8", get_hybrid_model_int8, get_hybrid_size_int8, INFERENCE_MODE_TFLITE, true},
};

void run_benchmark_suite(float *samples, int num_samples, const char* ground_truth) {
    ESP_LOGI(TAG, "=== STARTING BENCHMARK SUITE ===");
    ESP_LOGI(TAG, "Ground truth: %s", ground_truth);
    
    int num_models = sizeof(benchmark_configs)/sizeof(benchmark_configs[0]);
    
    for (int i = 0; i < num_models; i++) {
        ESP_LOGI(TAG, "\n--- Testing: %s ---", benchmark_configs[i].model_name);
        
        inference_engine_t engine;
        inference_config_t config = {
            .mode = benchmark_configs[i].inference_mode,
            .confidence_threshold = 0.3f,
            .voting_window = 1,
            .enable_voting = false,
            .enable_fft = false
        };
        
        // Initialize with specific model
        if (!inference_init(&engine, &config)) {
            ESP_LOGE(TAG, "Failed to initialize inference engine for %s", 
                     benchmark_configs[i].model_name);
            continue;
        }
        
        // Get model data dynamically
        engine.model_data = benchmark_configs[i].get_model_data();
        engine.model_size = benchmark_configs[i].get_model_size();
        
        if (!engine.model_data || engine.model_size == 0) {
            ESP_LOGW(TAG, "No model data for %s, skipping", benchmark_configs[i].model_name);
            inference_deinit(&engine);
            continue;
        }
        
        // Run inference multiple times
        uint64_t total_time = 0;
        int correct = 0;
        int total_runs = 10;
        
        for (int run = 0; run < total_runs; run++) {
            inference_result_t result;
            uint64_t start = esp_timer_get_time();
            
            if (inference_run(&engine, samples, num_samples, &result)) {
                uint64_t end = esp_timer_get_time();
                total_time += (end - start);
                
                bool is_correct = (strcmp(result.predicted_class, ground_truth) == 0);
                if (is_correct) correct++;
                
                #ifdef CONFIG_BENCHMARK_DETAILED_LOGS
                ESP_LOGD(TAG, "Run %d: %s (%.2f) in %llu us - %s",
                         run+1, result.predicted_class, result.confidence,
                         (end-start), is_correct ? "✓" : "✗");
                #endif
            }
            
            vTaskDelay(10 / portTICK_PERIOD_MS);  // Small delay between runs
        }
        
        // Calculate statistics
        float accuracy = (float)correct / total_runs * 100.0f;
        float avg_latency = (float)total_time / total_runs / 1000.0f;  // ms
        
        ESP_LOGI(TAG, "Results: %.1f%% accuracy, %.2f ms avg latency", 
                 accuracy, avg_latency);
        
        inference_deinit(&engine);
    }
    
    ESP_LOGI(TAG, "=== BENCHMARK SUITE COMPLETE ===");
}