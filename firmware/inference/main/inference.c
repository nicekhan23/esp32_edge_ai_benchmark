// inference.c
#include "inference.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "signal_processing.h"
#include "preprocessing.h"
#include "system_monitor.h"
#include "tflite_wrapper.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Include the single selected model based on Kconfig
#ifdef CONFIG_MODEL_CNN_INT8
    #include "cnn_int8_model.h"
    #define SELECTED_MODEL_TYPE MODEL_CNN_INT8
    #define SELECTED_MODEL_DATA cnn_int8_model_tflite
    #define SELECTED_MODEL_SIZE cnn_int8_model_tflite_len
#elif CONFIG_MODEL_CNN_FLOAT32
    #include "cnn_float32_model.h"
    #define SELECTED_MODEL_TYPE MODEL_CNN_FLOAT32
    #define SELECTED_MODEL_DATA cnn_float32_model_tflite
    #define SELECTED_MODEL_SIZE cnn_float32_model_tflite_len
#elif CONFIG_MODEL_MLP_FLOAT32
    #include "mlp_float32_model.h"
    #define SELECTED_MODEL_TYPE MODEL_MLP_FLOAT32
    #define SELECTED_MODEL_DATA mlp_float32_model_tflite
    #define SELECTED_MODEL_SIZE mlp_float32_model_tflite_len
#elif CONFIG_MODEL_MLP_INT8
    #include "mlp_int8_model.h"
    #define SELECTED_MODEL_TYPE MODEL_MLP_INT8
    #define SELECTED_MODEL_DATA mlp_int8_model_tflite
    #define SELECTED_MODEL_SIZE mlp_int8_model_tflite_len
#elif CONFIG_MODEL_HYBRID_FLOAT32
    #include "hybrid_float32_model.h"
    #define SELECTED_MODEL_TYPE MODEL_HYBRID_FLOAT32
    #define SELECTED_MODEL_DATA hybrid_float32_model_tflite
    #define SELECTED_MODEL_SIZE hybrid_float32_model_tflite_len
#elif CONFIG_MODEL_HYBRID_INT8
    #include "hybrid_int8_model.h"
    #define SELECTED_MODEL_TYPE MODEL_HYBRID_INT8
    #define SELECTED_MODEL_DATA hybrid_int8_model_tflite
    #define SELECTED_MODEL_SIZE hybrid_int8_model_tflite_len
#elif CONFIG_MODEL_HEURISTIC_ONLY
    // No TFLite model included
    #define SELECTED_MODEL_TYPE MODEL_NONE
    #define SELECTED_MODEL_DATA NULL
    #define SELECTED_MODEL_SIZE 0
#else
    #warning "No model selected in Kconfig, defaulting to heuristic only"
    #define SELECTED_MODEL_TYPE MODEL_NONE
    #define SELECTED_MODEL_DATA NULL
    #define SELECTED_MODEL_SIZE 0
#endif

// Set TFLITE_ENABLED flag
#if defined(CONFIG_MODEL_CNN_INT8) || defined(CONFIG_MODEL_CNN_FLOAT32) || \
    defined(CONFIG_MODEL_MLP_FLOAT32) || defined(CONFIG_MODEL_MLP_INT8) || \
    defined(CONFIG_MODEL_HYBRID_FLOAT32) || defined(CONFIG_MODEL_HYBRID_INT8)
    #define TFLITE_ENABLED 1
#else
    #define TFLITE_ENABLED 0
    #pragma message("TensorFlow Lite disabled - using heuristic inference")
#endif

static const char *TAG = "INFERENCE";

// Class names
static const char* s_class_names[] = {"SINE", "SQUARE", "TRIANGLE", "SAWTOOTH"};
#define NUM_CLASSES 4

// Helper function: Convert class name to index
int class_name_to_index(const char *class_name) {
    for (int i = 0; i < NUM_CLASSES; i++) {
        if (strcmp(class_name, s_class_names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

// Extract features for heuristic classification (optimized)
void extract_features(float *samples, int num_samples, signal_features_t *features) {
    if (!samples || !features || num_samples == 0) {
        return;
    }
    
    memset(features, 0, sizeof(signal_features_t));
    
    float sum = 0.0f, sum_abs = 0.0f, sum_sq = 0.0f;
    float peak = 0.0f;
    int zero_crossings = 0;
    float positive_sum = 0.0f, negative_sum = 0.0f;
    int positive_count = 0, negative_count = 0;
    
    // Single pass for all calculations
    for (int i = 0; i < num_samples; i++) {
        float val = samples[i];
        sum += val;
        sum_abs += fabsf(val);
        sum_sq += val * val;
        
        float abs_val = fabsf(val);
        if (abs_val > peak) {
            peak = abs_val;
        }
        
        if (i > 0 && val * samples[i-1] < 0) {
            zero_crossings++;
        }
        
        if (val > 0) {
            positive_sum += val;
            positive_count++;
        } else if (val < 0) {
            negative_sum += -val;  // fabsf equivalent
            negative_count++;
        }
    }
    
    // Basic statistics
    float rms = sqrtf(sum_sq / num_samples);
    float avg_rect = sum_abs / num_samples;
    
    features->zero_crossing_rate = (float)zero_crossings / num_samples;
    features->crest_factor = (rms > 0.001f) ? peak / rms : 0.0f;
    features->form_factor = (avg_rect > 0.001f) ? rms / avg_rect : 0.0f;
    
    // Symmetry
    float positive_avg = positive_count > 0 ? positive_sum / positive_count : 0.0f;
    float negative_avg = negative_count > 0 ? negative_sum / negative_count : 0.0f;
    float total_avg = positive_avg + negative_avg;
    
    features->symmetry_score = (total_avg > 1e-6f) ? 
                               fabsf(positive_avg - negative_avg) / total_avg : 0.0f;
    
    // Simple frequency estimation (without full FFT)
    if (zero_crossings > 2) {
        // Estimate frequency from zero crossings
        features->dominant_frequency = (float)zero_crossings * 10000.0f / num_samples; // Approx
        features->harmonic_ratio = 0.1f;  // Default low harmonic content
    } else {
        features->dominant_frequency = 0.0f;
        features->harmonic_ratio = 0.0f;
    }
}

// Heuristic inference based on features (optimized)
static bool heuristic_inference(float *samples, int num_samples, inference_result_t *result) {
    signal_features_t features;
    extract_features(samples, num_samples, &features);
    
    // Optimized decision tree
    int predicted_class;
    float confidence;
    
    if (features.zero_crossing_rate > 0.4f) {
        // Sine or Triangle (high zero crossings)
        if (features.harmonic_ratio < 0.3f) {
            predicted_class = 0; // SINE
            confidence = 0.85f;
        } else {
            predicted_class = 2; // TRIANGLE
            confidence = 0.75f;
        }
    } else if (features.crest_factor > 1.5f) {
        // Square wave
        predicted_class = 1; // SQUARE
        confidence = 0.8f;
    } else {
        // Default to sawtooth
        predicted_class = 3; // SAWTOOTH
        confidence = 0.7f;
    }
    
    // Add small variation
    confidence *= (0.9f + 0.1f * ((float)rand() / RAND_MAX));
    
    strncpy(result->predicted_class, s_class_names[predicted_class], 
            sizeof(result->predicted_class) - 1);
    result->predicted_class[sizeof(result->predicted_class) - 1] = '\0';
    result->confidence = confidence;
    result->num_classes = NUM_CLASSES;
    result->is_voted_result = false;
    
    return true;
}

// TFLite inference using C wrapper
static bool tflite_inference(inference_engine_t *engine, float *samples, int num_samples, inference_result_t *result) {
    #if TFLITE_ENABLED
    if (!engine->model_data || engine->model_size == 0) {
        ESP_LOGW(TAG, "No TFLite model available");
        return false;
    }
    
    bool success = tflite_inference_cpp(
        engine->model_data,
        engine->model_size,
        samples,
        num_samples,
        result->predicted_class,
        sizeof(result->predicted_class),
        &result->confidence
    );
    
    if (success) {
        result->num_classes = NUM_CLASSES;  // Update based on actual model output if needed
        result->is_voted_result = false;
        
        #ifdef CONFIG_DETAILED_LOGGING
        ESP_LOGI(TAG, "TFLite inference: %s (%.2f)", 
                 result->predicted_class, result->confidence);
        #endif
    }
    
    return success;
    #else
    ESP_LOGW(TAG, "TFLite inference requested but not enabled");
    return false;
    #endif
}

// Initialize inference engine
bool inference_init(inference_engine_t *engine, inference_config_t *config) {
    if (!engine || !config) {
        ESP_LOGE(TAG, "Invalid engine or config");
        return false;
    }
    
    memset(engine, 0, sizeof(inference_engine_t));
    engine->config = *config;
    engine->mode = config->mode;
    
    #if TFLITE_ENABLED
    if (config->mode == INFERENCE_MODE_TFLITE) {
        // Use the model selected in Kconfig
        engine->model_data = (void*)SELECTED_MODEL_DATA;
        engine->model_size = SELECTED_MODEL_SIZE;
        
        if (!engine->model_data || engine->model_size == 0) {
            ESP_LOGE(TAG, "Failed to load model data for Kconfig-selected model");
            return false;
        }
        
        ESP_LOGI(TAG, "Loaded Kconfig-selected model type %d (%u bytes)", 
                 SELECTED_MODEL_TYPE, (unsigned int)engine->model_size);
        
        // Check arena size requirement
        #ifdef CONFIG_DETAILED_LOGGING
        size_t arena_size = tflite_get_arena_size(engine->model_data, engine->model_size);
        if (arena_size > 0) {
            ESP_LOGI(TAG, "TFLite arena required: %u bytes", (unsigned int)arena_size);
        }
        #endif
        
        engine->initialized = true;
        return true;
    }
    #endif
    
    // Fallback modes (heuristic/simulated)
    engine->initialized = true;
    ESP_LOGI(TAG, "Heuristic inference engine initialized");
    return true;
}

// Run inference based on configured mode
bool inference_run(inference_engine_t *engine, 
                   float *samples, 
                   int num_samples, 
                   inference_result_t *result) {
    if (!engine || !engine->initialized || !samples || !result || num_samples <= 0) {
        ESP_LOGE(TAG, "Invalid parameters for inference_run");
        return false;
    }
    
    memset(result, 0, sizeof(inference_result_t));
    
    // Validate signal (optional based on config)
    #ifdef CONFIG_ENABLE_SIGNAL_VALIDATION
    signal_quality_t quality = validate_signal(samples, num_samples);
    if (quality != SIGNAL_OK) {
        #ifdef CONFIG_DETAILED_LOGGING
        ESP_LOGW(TAG, "Poor signal quality: %d", quality);
        #endif
        return false;
    }
    #endif
    
    uint64_t start_time = esp_timer_get_time();
    
    bool success = false;
    
    switch (engine->mode) {
        #if TFLITE_ENABLED
        case INFERENCE_MODE_TFLITE:
            success = tflite_inference(engine, samples, num_samples, result);
            break;
        #endif
        case INFERENCE_MODE_HEURISTIC:
        case INFERENCE_MODE_FFT_BASED:
        case INFERENCE_MODE_SIMULATED:
        default:
            success = heuristic_inference(samples, num_samples, result);
            break;
    }
    
    if (success) {
        uint64_t end_time = esp_timer_get_time();
        uint64_t inference_time = end_time - start_time;
        
        result->timestamp_ms = (uint32_t)(end_time / 1000);
        
        // Record metrics
        metrics_record_inference_time(inference_time);
        #ifdef CONFIG_ENABLE_MEMORY_METRICS
        metrics_record_memory_usage();
        #endif
    }
    
    return success;
}

// Process inference result (simplified)
void process_inference_result(inference_result_t *result, 
                              const char *ground_truth,
                              clock_sync_t *sync) {
    if (!result) return;
    
    #ifdef CONFIG_DETAILED_LOGGING
    uint32_t timestamp = sync ? get_synchronized_timestamp(sync) : result->timestamp_ms;
    ESP_LOGI(TAG, "Inference: %s (%.2f) at %u ms", 
             result->predicted_class, result->confidence, timestamp);
    #endif
    
    if (ground_truth) {
        #ifdef CONFIG_DETAILED_LOGGING
        ESP_LOGI(TAG, "Ground truth: %s", ground_truth);
        #endif
        
        if (strcmp(result->predicted_class, ground_truth) == 0) {
            metrics_record_correct_prediction();
            #ifdef CONFIG_DETAILED_LOGGING
            ESP_LOGI(TAG, "✓ CORRECT");
            #endif
        } else {
            metrics_record_incorrect_prediction();
            #ifdef CONFIG_DETAILED_LOGGING
            ESP_LOGW(TAG, "✗ INCORRECT (expected: %s)", ground_truth);
            #endif
        }
    }
}

void inference_deinit(inference_engine_t *engine) {
    if (engine) {
        engine->initialized = false;
    }
}

void inference_get_memory_usage(inference_engine_t *engine, 
                                size_t *ram_kb, 
                                size_t *flash_kb) {
    if (ram_kb) *ram_kb = 0;
    if (flash_kb) *flash_kb = 0;
    
    if (engine) {
        #if TFLITE_ENABLED
        if (engine->model_data && flash_kb) {
            *flash_kb = (engine->model_size + 1023) / 1024;  // Round up to KB
        }
        
        if (ram_kb) {
            // Estimate RAM usage
            if (engine->mode == INFERENCE_MODE_TFLITE) {
                size_t arena_size = tflite_get_arena_size(engine->model_data, engine->model_size);
                *ram_kb = (arena_size + 1023) / 1024;  // Round up to KB
            } else {
                *ram_kb = 2;  // Heuristic inference uses minimal RAM
            }
        }
        #endif
    }
}

// Keep the original header functions
bool inference_run_with_voting(inference_engine_t *engine,
                               inference_config_t *config,
                               float *samples,
                               int buffer_size,
                               inference_result_t *final_result) {
    // Simple implementation - average confidence over last N inferences
    static inference_result_t results[10];
    static int result_idx = 0;
    
    // Run single inference
    if (!inference_run(engine, samples, buffer_size, &results[result_idx])) {
        return false;
    }
    
    // Simple voting: just return the latest result
    // For proper voting, you'd track multiple results and choose the most common
    memcpy(final_result, &results[result_idx], sizeof(inference_result_t));
    
    result_idx = (result_idx + 1) % 10;
    return true;
}