/**
 * @file inference.c
 * @brief Signal classification inference engine
 * @details Implements rule-based and (placeholder) neural network inference
 *          for signal type classification. Includes statistics tracking.
 * 
 * @author Your Name
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Currently uses rule-based classifier; replace with TensorFlow Lite Micro
 * @note Allocates 1MB tensor arena for model (adjust based on model size)
 * 
 * @copyright (c) 2025 ESP32 Signal Processing Project
 */

#include "inference.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

// Configuration
#define TENSOR_ARENA_SIZE (1024 * 1024)  /**< 1MB for TensorFlow Lite model */
#define MODEL_PATH "/spiffs/model.tflite" /**< Path to model in SPIFFS */

static const char *TAG = "INFERENCE";                   /**< Logging tag */

/**
 * @brief Tensor arena for TensorFlow Lite Micro
 * 
 * Statically allocated memory for model weights, activations, and tensors.
 */
static uint8_t *tensor_arena = NULL;

/**
 * @brief Inference statistics
 */
static inference_stats_t s_stats = {0};

/**
 * @brief Simple rule-based signal classifier (placeholder)
 * 
 * Classifies signals based on variance and zero-crossing rate heuristics.
 * Replace with proper ML model inference.
 * 
 * @param[in] features Pointer to feature vector
 * @return signal_type_t Classified signal type
 */
static signal_type_t classify_simple(const feature_vector_t *features) {
    float zcr = features->features[3];  // Zero crossing rate
    float variance = features->features[1];  // Variance
    
    if (variance < 100.0f) {
        return SIGNAL_NOISE;  // Low variance -> likely noise
    } else if (zcr > 0.3f) {
        return SIGNAL_SINE;   // High zero crossing -> sine
    } else if (zcr < 0.1f) {
        return SIGNAL_SQUARE; // Low zero crossing -> square
    } else {
        return SIGNAL_TRIANGLE;
    }
}

/**
 * @brief Initialize inference engine
 * 
 * Allocates tensor arena and loads TensorFlow Lite model (placeholder).
 * 
 * @throws ERR_NO_MEM if tensor arena allocation fails
 * 
 * @note Currently only allocates memory; model loading is TODO
 */
void inference_init(void) {
    ESP_LOGI(TAG, "Initializing inference engine");
    
    // Allocate tensor arena
    tensor_arena = (uint8_t*)malloc(TENSOR_ARENA_SIZE);
    if (!tensor_arena) {
        ESP_LOGE(TAG, "Failed to allocate tensor arena");
        return;
    }
    
    // TODO: Load TensorFlow Lite model
    // tflite::MicroInterpreter* interpreter = ...
    
    ESP_LOGI(TAG, "Inference engine initialized");
}

/**
 * @brief Run inference on feature vector
 * 
 * Performs classification and returns result with confidence and timing.
 * 
 * @param[in] features Pointer to feature vector
 * @return inference_result_t Classification result with metadata
 * 
 * @note Updates inference statistics internally
 * @note Placeholder confidence fixed at 0.85
 */
inference_result_t run_inference(const feature_vector_t *features) {
    uint64_t start_time = esp_timer_get_time();
    
    inference_result_t result;
    result.window_id = features->window_id;
    
    // Run classification
    result.type = classify_simple(features);
    result.confidence = 0.85f;  // Placeholder confidence
    
    result.inference_time_us = esp_timer_get_time() - start_time;
    
    // Update statistics
    s_stats.total_inferences++;
    if (result.type >= 0 && result.type < SIGNAL_COUNT) {
        s_stats.per_type_counts[result.type]++;
    }
    
    // Update average inference time (moving average)
    s_stats.avg_inference_time_us = 
        (s_stats.avg_inference_time_us * (s_stats.total_inferences - 1) + 
         result.inference_time_us) / s_stats.total_inferences;
    
    return result;
}

/**
 * @brief Convert signal type enum to string
 * 
 * @param[in] type Signal type enum value
 * @return const char* Readable string representation
 */
const char* signal_type_to_string(signal_type_t type) {
    switch (type) {
        case SIGNAL_SINE: return "SINE";
        case SIGNAL_SQUARE: return "SQUARE";
        case SIGNAL_TRIANGLE: return "TRIANGLE";
        case SIGNAL_NOISE: return "NOISE";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Get current inference statistics
 * 
 * @return inference_stats_t Copy of current statistics
 */
inference_stats_t get_inference_stats(void) {
    return s_stats;
}

/**
 * @brief Cleanup inference engine resources
 * 
 * Frees tensor arena and other allocated memory.
 */
void inference_cleanup(void) {
    if (tensor_arena) {
        free(tensor_arena);
        tensor_arena = NULL;
    }
    ESP_LOGI(TAG, "Inference engine cleaned up");
}