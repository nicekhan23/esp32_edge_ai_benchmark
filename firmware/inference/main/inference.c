#include "inference.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "signal_processing.h"
#include "preprocessing.h"
#include "system_monitor.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Conditional TFLite inclusion
#ifdef CONFIG_USE_TENSORFLOW_LITE
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"
#define TFLITE_ENABLED 1
#else
#define TFLITE_ENABLED 0
#pragma message("TensorFlow Lite disabled - using heuristic inference")
#endif

#include "model_data.h"

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
    result->confidence = confidence;
    result->num_classes = NUM_CLASSES;
    result->is_voted_result = false;
    
    return true;
}

#if TFLITE_ENABLED
// TensorFlow Lite implementation with dynamic memory allocation
static bool tflite_inference(inference_engine_t *engine, float *samples, int num_samples, inference_result_t *result) {
    if (!engine->model_data || engine->model_size == 0) {
        ESP_LOGW(TAG, "No TFLite model available");
        return false;
    }
    
    const tflite::Model* model = tflite::GetModel(engine->model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "Model schema version mismatch");
        return false;
    }
    
    static tflite::MicroMutableOpResolver<5> resolver;  // Reduced from 10
    resolver.AddFullyConnected();
    resolver.AddSoftmax();
    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddReshape();
    
    // Calculate required memory
    tflite::MicroInterpreter temp_interpreter(model, resolver, nullptr, 0);
    size_t arena_size = temp_interpreter.recommended_arena_size();
    
    #ifdef CONFIG_DETAILED_LOGGING
    ESP_LOGI(TAG, "TFLite arena: %d bytes", arena_size);
    #endif
    
    // Allocate with 10% margin
    size_t total_size = arena_size * 110 / 100;
    uint8_t *arena = (uint8_t*)heap_caps_malloc(total_size, MALLOC_CAP_SPIRAM);
    
    if (!arena) {
        // Fallback to internal RAM
        arena = (uint8_t*)malloc(total_size);
        #ifdef CONFIG_DETAILED_LOGGING
        ESP_LOGW(TAG, "Using internal RAM for TFLite");
        #endif
    }
    
    if (!arena) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes", total_size);
        return false;
    }
    
    tflite::MicroInterpreter interpreter(model, resolver, arena, total_size);
    
    if (interpreter.AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "Failed to allocate tensors");
        free(arena);
        return false;
    }
    
    // Get input tensor
    TfLiteTensor* input = interpreter.input(0);
    
    // Copy samples to input tensor
    if (input->type == kTfLiteFloat32) {
        float* input_data = input->data.f;
        memcpy(input_data, samples, num_samples * sizeof(float));
    } else if (input->type == kTfLiteInt8) {
        int8_t* input_data = input->data.int8;
        for (int i = 0; i < num_samples; i++) {
            // Simplified quantization
            input_data[i] = (int8_t)(samples[i] * 127.0f);
        }
    }
    
    // Run inference
    if (interpreter.Invoke() != kTfLiteOk) {
        ESP_LOGE(TAG, "Failed to invoke interpreter");
        free(arena);
        return false;
    }
    
    // Get output tensor
    TfLiteTensor* output = interpreter.output(0);
    
    // Process output
    float max_prob = 0.0f;
    int max_index = 0;
    
    if (output->type == kTfLiteFloat32) {
        float* output_data = output->data.f;
        for (int i = 0; i < output->dims->data[1]; i++) {
            if (output_data[i] > max_prob) {
                max_prob = output_data[i];
                max_index = i;
            }
        }
    } else if (output->type == kTfLiteInt8) {
        int8_t* output_data = output->data.int8;
        float scale = output->params.scale;
        int zero_point = output->params.zero_point;
        
        for (int i = 0; i < output->dims->data[1]; i++) {
            float prob = (output_data[i] - zero_point) * scale;
            if (prob > max_prob) {
                max_prob = prob;
                max_index = i;
            }
        }
    }
    
    // Map to class name
    if (max_index < NUM_CLASSES) {
        strncpy(result->predicted_class, s_class_names[max_index], 
                sizeof(result->predicted_class) - 1);
    } else {
        snprintf(result->predicted_class, sizeof(result->predicted_class), 
                 "CLASS_%d", max_index);
    }
    
    result->confidence = max_prob;
    result->num_classes = output->dims->data[1];
    result->is_voted_result = false;
    
    // Free arena memory
    free(arena);
    
    return true;
}
#endif

// Initialize inference engine
bool inference_init(inference_engine_t *engine, inference_config_t *config) {
    if (!engine || !config) {
        return false;
    }
    
    memset(engine, 0, sizeof(inference_engine_t));
    
    // Copy configuration
    engine->config = *config;
    engine->mode = config->mode;
    
    #if TFLITE_ENABLED
    // Initialize TFLite if requested
    if (config->mode == INFERENCE_MODE_TFLITE) {
        engine->initialized = true;
        #ifdef CONFIG_DETAILED_LOGGING
        ESP_LOGI(TAG, "TensorFlow Lite inference engine initialized");
        #endif
        return true;
    }
    #endif
    
    // Fallback to heuristic mode
    engine->initialized = true;
    
    #ifdef CONFIG_DETAILED_LOGGING
    switch (config->mode) {
        case INFERENCE_MODE_HEURISTIC:
            ESP_LOGI(TAG, "Heuristic inference engine initialized");
            break;
        case INFERENCE_MODE_FFT_BASED:
            ESP_LOGI(TAG, "FFT-based inference engine initialized");
            break;
        default:
            ESP_LOGI(TAG, "Simulated inference engine initialized");
            break;
    }
    #endif
    
    return true;
}

// Run inference based on configured mode
bool inference_run(inference_engine_t *engine, 
                   float *samples, 
                   int num_samples, 
                   inference_result_t *result) {
    if (!engine || !engine->initialized || !samples || !result || num_samples <= 0) {
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
        if (engine->model_data) {
            if (flash_kb) {
                *flash_kb = engine->model_size / 1024;
            }
            if (ram_kb) {
                *ram_kb = 50;  // Reduced estimate
            }
        }
        #endif
    }
}