/**
 * @file inference.c
 * @brief Signal classification inference engine
 * @details Implements rule-based and (placeholder) neural network inference
 *          for signal type classification. Includes statistics tracking.
 * 
 * @author Darkhan Zhanibekuly
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Currently uses rule-based classifier; replace with TensorFlow Lite Micro
 * @note Allocates 1MB tensor arena for model (adjust based on model size)
 */

#include "inference.h"
#include "common.h"
#include "statistics.h"
#include "feature_utils.h"
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
 * @brief Extract basic features from window for rule-based classification
 */
static void extract_basic_features(const window_buffer_t *window, float *features) {
    // Extract basic features for rule-based classification
    float mean = feature_utils_mean(window->samples, ML_WINDOW_SIZE);
    float variance = feature_utils_variance(window->samples, ML_WINDOW_SIZE, mean);
    
    // Calculate zero crossing rate
    float *dc_removed = get_float_buffer(ML_WINDOW_SIZE);
    if (dc_removed) {
        feature_utils_remove_dc_offset(window->samples, ML_WINDOW_SIZE, dc_removed);
        
        uint32_t zero_crossings = 0;
        for (int i = 1; i < ML_WINDOW_SIZE; i++) {
            if ((dc_removed[i-1] > 0.0f && dc_removed[i] <= 0.0f) || 
                (dc_removed[i-1] <= 0.0f && dc_removed[i] > 0.0f)) {
                zero_crossings++;
            }
        }
        features[3] = (float)zero_crossings / (ML_WINDOW_SIZE - 1);
        release_float_buffer(dc_removed);
    } else {
        features[3] = 0.0f;
    }
    
    // Calculate skewness
    features[4] = feature_utils_skewness(window->samples, ML_WINDOW_SIZE, mean, variance);
    
    // Calculate crest factor
    float rms = feature_utils_rms(window->samples, ML_WINDOW_SIZE);
    features[6] = feature_utils_crest_factor(window->samples, ML_WINDOW_SIZE, rms);
    
    // Calculate periodicity (simplified)
    float periodicity = 0.0f;
    if (ML_WINDOW_SIZE >= 64) {
        // Simple autocorrelation at lag = 1/4 window
        int lag = ML_WINDOW_SIZE / 4;
        float corr = 0.0f;
        for (int i = 0; i < ML_WINDOW_SIZE - lag; i++) {
            corr += (window->samples[i] - mean) * (window->samples[i + lag] - mean);
        }
        periodicity = fabsf(corr / (ML_WINDOW_SIZE - lag)) / (variance + 1e-6f);
    }
    features[8] = periodicity;
    
    // Calculate asymmetry
    uint16_t min_val, max_val;
    feature_utils_min_max(window->samples, ML_WINDOW_SIZE, &min_val, &max_val);
    int min_idx = 0, max_idx = 0;
    for (int i = 0; i < ML_WINDOW_SIZE; i++) {
        if (window->samples[i] == min_val) min_idx = i;
        if (window->samples[i] == max_val) max_idx = i;
    }
    features[10] = fabsf((float)(max_idx - min_idx) / ML_WINDOW_SIZE);
    
    // Store other features
    features[0] = mean;
    features[1] = variance;
    features[2] = rms;
}

/**
 * @brief Enhanced signal classifier with sawtooth detection
 * 
 * Classifies signals based on multiple heuristics including:
 * - Variance and zero-crossing rate
 * - Skewness (for sawtooth detection)
 * - Crest factor
 * - Form factor
 * 
 * @param[in] window Pointer to window buffer
 * @return ml_class_t Classified signal type
 */
static ml_class_t classify_simple(const window_buffer_t *window) {
    float features[16] = {0};
    extract_basic_features(window, features);
    
    float zcr = features[3];          // Zero crossing rate
    float variance = features[1];     // Variance
    float skewness = features[4];     // Skewness
    float crest_factor = features[6]; // Crest factor
    float periodicity = features[8];  // New: periodicity
    float asymmetry = features[10];   // New: asymmetry
    
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
        return ML_CLASS_NOISE;
    }
    
    // 2. Check for sine (high ZCR, high periodicity)
    if (zcr > SINE_ZCR_MIN && periodicity > SINE_PERIODICITY_MIN) {
        return ML_CLASS_SINE;
    }
    
    // 3. Check for square (very low ZCR, low crest factor)
    if (zcr < SQUARE_ZCR_MAX && crest_factor < SQUARE_CREST_MAX) {
        return ML_CLASS_SQUARE;
    }
    
    // 4. Check for sawtooth (high skewness, high asymmetry)
    if (fabsf(skewness) > SAWTOOTH_SKEW_MIN && 
        asymmetry > SAWTOOTH_ASYMMETRY_MIN) {
        return ML_CLASS_SAWTOOTH;
    }
    
    // 5. Check for triangle (low skewness, moderate ZCR)
    if (fabsf(skewness) < TRIANGLE_SKEW_MAX && 
        zcr > 0.1f && zcr < 0.3f) {
        return ML_CLASS_TRIANGLE;
    }
    
    // Default based on ZCR
    if (zcr < 0.1f) return ML_CLASS_SQUARE;
    if (zcr > 0.3f) return ML_CLASS_SINE;
    return ML_CLASS_TRIANGLE;
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
 * @param[in] window Pointer to window buffer
 * @return ml_output_t Classification result with metadata
 * 
 * @note Updates inference statistics internally
 * @note Confidence calculated based on feature distances from thresholds
 */
ml_output_t run_inference(const window_buffer_t *window) {
    uint64_t start_time = esp_timer_get_time();
    
    ml_output_t result;
    result.window_id = window->window_id;
    
    // Validate window against ML contract
    if (!signal_acquisition_validate_window(window)) {
        ESP_LOGW(TAG, "Window validation failed for window %lu", window->window_id);
        s_stats.contract_violations++;
        
        // Return error result
        result.predicted_class = ML_CLASS_NOISE;
        result.confidence = 0.0f;
        result.inference_time_us = esp_timer_get_time() - start_time;
        return result;
    }
    
    // Run classification FIRST
    result.predicted_class = classify_simple(window);
    
    // Calculate confidence based on feature values
    float confidence = 0.70f;  // Base confidence
    
    // Extract features for confidence calculation
    float features[16] = {0};
    extract_basic_features(window, features);
    
    // Adjust confidence based on how clearly features match the classification
    switch (result.predicted_class) {
        case ML_CLASS_SINE:
            if (features[3] > 0.4f) confidence = 0.95f;  // Very high ZCR
            break;
        case ML_CLASS_SQUARE:
            if (features[3] < 0.02f) confidence = 0.90f;  // Very low ZCR
            break;
        case ML_CLASS_TRIANGLE:
            if (fabsf(features[4]) < 0.1f) confidence = 0.85f;  // Symmetrical
            break;
        case ML_CLASS_SAWTOOTH:
            if (fabsf(features[4]) > 0.5f) confidence = 0.90f;  // Strong asymmetry
            break;
        case ML_CLASS_NOISE:
            if (features[1] < 50.0f) confidence = 0.95f;  // Very low variance
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
    if (result.predicted_class >= 0 && result.predicted_class < ML_CLASS_COUNT) {
        s_stats.per_class_counts[result.predicted_class]++;
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
 * @brief Convert ML class to human-readable string
 * 
 * @param[in] class ML class enumeration value
 * @return const char* Readable string representation
 */
const char* inference_class_to_string(ml_class_t class) {
    return ml_class_to_string(class);
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