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
#include <math.h>

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
 * @brief Enhanced signal classifier with sawtooth detection
 * 
 * Classifies signals based on multiple heuristics including:
 * - Variance and zero-crossing rate
 * - Skewness (for sawtooth detection)
 * - Crest factor
 * - Form factor
 * 
 * @param[in] features Pointer to feature vector
 * @return signal_type_t Classified signal type
 */
static signal_type_t classify_simple(const feature_vector_t *features) {
    float zcr = features->features[3];          // Zero crossing rate
    float variance = features->features[1];     // Variance
    float skewness = features->features[4];     // Skewness
    float crest_factor = features->features[6]; // Crest factor
    float form_factor = features->features[7];  // Form factor
    
    // Threshold values (calibrate based on actual signal data)
    const float NOISE_VARIANCE_THRESHOLD = 100.0f;
    const float SINE_ZCR_THRESHOLD = 0.35f;
    const float SQUARE_ZCR_THRESHOLD = 0.05f;
    const float SAWTOOTH_SKEWNESS_THRESHOLD = 0.3f;
    const float TRIANGLE_ZCR_MIN = 0.1f;
    const float TRIANGLE_ZCR_MAX = 0.25f;
    const float SAWTOOTH_ZCR_MIN = 0.15f;
    const float SAWTOOTH_ZCR_MAX = 0.3f;
    
    // Step 1: Check for noise (low variance)
    if (variance < NOISE_VARIANCE_THRESHOLD) {
        return SIGNAL_NOISE;
    }
    
    // Step 2: Check for sine wave (high ZCR)
    if (zcr > SINE_ZCR_THRESHOLD) {
        return SIGNAL_SINE;
    }
    
    // Step 3: Check for square wave (very low ZCR)
    if (zcr < SQUARE_ZCR_THRESHOLD) {
        return SIGNAL_SQUARE;
    }
    
    // Step 4: Distinguish between triangle and sawtooth
    if (zcr >= TRIANGLE_ZCR_MIN && zcr <= TRIANGLE_ZCR_MAX) {
        // Check skewness for sawtooth (asymmetric)
        if (fabsf(skewness) > SAWTOOTH_SKEWNESS_THRESHOLD) {
            return SIGNAL_SAWTOOTH;
        } else {
            return SIGNAL_TRIANGLE;
        }
    }
    
    // Step 5: Additional sawtooth detection with ZCR range
    if (zcr >= SAWTOOTH_ZCR_MIN && zcr <= SAWTOOTH_ZCR_MAX) {
        // Sawtooth often has moderate crest factor (1.4-1.7) and form factor (~1.11)
        if (crest_factor > 1.4f && crest_factor < 1.8f && 
            form_factor > 1.05f && form_factor < 1.15f) {
            return SIGNAL_SAWTOOTH;
        }
    }
    
    // Default: triangle wave
    return SIGNAL_TRIANGLE;
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
 * @note Confidence calculated based on feature distances from thresholds
 */
inference_result_t run_inference(const feature_vector_t *features) {
    uint64_t start_time = esp_timer_get_time();
    
    inference_result_t result;
    result.window_id = features->window_id;
    
    // Run classification
    result.type = classify_simple(features);
    
    // Calculate confidence based on feature values
    float confidence = 0.70f;  // Base confidence
    
    // Adjust confidence based on how clearly features match the classification
    switch (result.type) {
        case SIGNAL_SINE:
            if (features->features[3] > 0.4f) confidence = 0.95f;  // Very high ZCR
            break;
        case SIGNAL_SQUARE:
            if (features->features[3] < 0.02f) confidence = 0.90f;  // Very low ZCR
            break;
        case SIGNAL_TRIANGLE:
            if (fabsf(features->features[4]) < 0.1f) confidence = 0.85f;  // Symmetrical
            break;
        case SIGNAL_SAWTOOTH:
            if (fabsf(features->features[4]) > 0.5f) confidence = 0.90f;  // Strong asymmetry
            break;
        case SIGNAL_NOISE:
            if (features->features[1] < 50.0f) confidence = 0.95f;  // Very low variance
            break;
        default:
            confidence = 0.50f;
            break;
    }
    
    result.confidence = confidence;
    result.inference_time_us = esp_timer_get_time() - start_time;
    
    // Update statistics
    s_stats.total_inferences++;
    if (result.type >= 0 && result.type < SIGNAL_COUNT) {
        s_stats.per_type_counts[result.type]++;
    }
    
    // Update average inference time (moving average)
    if (s_stats.total_inferences > 1) {
        s_stats.avg_inference_time_us = 
            (s_stats.avg_inference_time_us * (s_stats.total_inferences - 1) + 
             result.inference_time_us) / s_stats.total_inferences;
    } else {
        s_stats.avg_inference_time_us = result.inference_time_us;
    }
    
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
        case SIGNAL_SAWTOOTH: return "SAWTOOTH";
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