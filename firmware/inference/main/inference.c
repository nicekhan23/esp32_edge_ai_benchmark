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
#include "common.h"
#include "statistics.h"
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

static moving_average_t s_confidence_avg;
static min_max_tracker_t s_inference_time_tracker;

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
    float periodicity = features->features[8];  // New: periodicity
    float asymmetry = features->features[10];   // New: asymmetry
    
    // Debug print (enable for testing)
    // ESP_LOGI(TAG, "ZCR: %.3f, Var: %.1f, Skew: %.3f, Crest: %.3f, Period: %.3f, Asym: %.3f", 
    //          zcr, variance, skewness, crest_factor, periodicity, asymmetry);
    
    // Updated thresholds (tune these based on your data)
    const float NOISE_VAR_THRESH = 50.0f;
    const float SINE_ZCR_MIN = 0.35f;
    const float SQUARE_ZCR_MAX = 0.08f;
    const float SAWTOOTH_SKEW_MIN = 0.4f;
    const float TRIANGLE_SKEW_MAX = 0.2f;
    const float SAWTOOTH_ASYMMETRY_MIN = 0.7f;
    const float SQUARE_CREST_MAX = 1.2f;
    const float SINE_PERIODICITY_MIN = 0.8f;
    
    // 1. Check for noise
    if (variance < NOISE_VAR_THRESH) {
        return SIGNAL_NOISE;
    }
    
    // 2. Check for sine (high ZCR, high periodicity)
    if (zcr > SINE_ZCR_MIN && periodicity > SINE_PERIODICITY_MIN) {
        return SIGNAL_SINE;
    }
    
    // 3. Check for square (very low ZCR, low crest factor)
    if (zcr < SQUARE_ZCR_MAX && crest_factor < SQUARE_CREST_MAX) {
        return SIGNAL_SQUARE;
    }
    
    // 4. Check for sawtooth (high skewness, high asymmetry)
    if (fabsf(skewness) > SAWTOOTH_SKEW_MIN && 
        asymmetry > SAWTOOTH_ASYMMETRY_MIN) {
        return SIGNAL_SAWTOOTH;
    }
    
    // 5. Check for triangle (low skewness, moderate ZCR)
    if (fabsf(skewness) < TRIANGLE_SKEW_MAX && 
        zcr > 0.1f && zcr < 0.3f) {
        return SIGNAL_TRIANGLE;
    }
    
    // Default based on ZCR
    if (zcr < 0.1f) return SIGNAL_SQUARE;
    if (zcr > 0.3f) return SIGNAL_SINE;
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

    moving_average_init(&s_confidence_avg);
    min_max_tracker_init(&s_inference_time_tracker);
    
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
    
    // Run classification FIRST
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
    
    // NOW update the trackers with the ACTUAL calculated values
    min_max_tracker_update(&s_inference_time_tracker, (float)result.inference_time_us);
    moving_average_update(&s_confidence_avg, result.confidence);
    
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