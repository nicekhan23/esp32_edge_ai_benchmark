/**
 * @file feature_utils.c
 * @brief Shared feature calculation utilities implementation
 * @details Common mathematical operations used across feature extraction
 *          and signal processing modules.
 * 
 * @author Your Name
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Uses floating-point math; ensure FPU is enabled
 * @note All functions are thread-safe (no shared state)
 * 
 * @copyright (c) 2025 ESP32 Signal Processing Project
 */

#include "feature_utils.h"
#include <math.h>
#include <string.h>
#include "common.h"

#define MAX_POOL_BUFFERS 4
#define MAX_POOL_BUFFER_SIZE 1024  // Adjust based on WINDOW_SIZE

static float float_pool[MAX_POOL_BUFFERS][MAX_POOL_BUFFER_SIZE];
static bool pool_allocated[MAX_POOL_BUFFERS] = {false};
/**
 * @brief Get a float buffer from memory pool
 */
float* get_float_buffer(uint32_t size) {
    if (size > MAX_POOL_BUFFER_SIZE) {
        // Fallback to dynamic allocation if pool is too small
        return (float*)malloc(size * sizeof(float));
    }
    
    // Find free slot in pool
    for (int i = 0; i < MAX_POOL_BUFFERS; i++) {
        if (!pool_allocated[i]) {
            pool_allocated[i] = true;
            return float_pool[i];
        }
    }
    
    // Pool exhausted, use dynamic allocation
    return (float*)malloc(size * sizeof(float));
}

/**
 * @brief Release float buffer back to pool
 */
void release_float_buffer(float* buffer) {
    if (!buffer) return;
    
    // Check if buffer is from pool
    for (int i = 0; i < MAX_POOL_BUFFERS; i++) {
        if (buffer == float_pool[i]) {
            pool_allocated[i] = false;
            return;
        }
    }
    
    // Buffer was dynamically allocated
    free(buffer);
}

/**
 * @brief Calculate mean of sample array
 */
float feature_utils_mean(const uint16_t *samples, uint32_t count) {
    if (count == 0) return 0.0f;
    
    float sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        sum += (float)samples[i];
    }
    return sum / count;
}

/**
 * @brief Remove DC offset from sample array
 */
float feature_utils_remove_dc_offset(const uint16_t *samples, uint32_t count, float *output) {
    if (count == 0 || !output) return 0.0f;
    
    float mean = feature_utils_mean(samples, count);
    for (uint32_t i = 0; i < count; i++) {
        output[i] = (float)samples[i] - mean;
    }
    return mean;
}

/**
 * @brief Calculate variance of sample array
 */
float feature_utils_variance(const uint16_t *samples, uint32_t count, float mean) {
    if (count == 0) return 0.0f;
    
    float sum_sq_diff = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float diff = (float)samples[i] - mean;
        sum_sq_diff += diff * diff;
    }
    return sum_sq_diff / count;
}

/**
 * @brief Calculate Root Mean Square (RMS) of samples
 */
float feature_utils_rms(const uint16_t *samples, uint32_t count) {
    if (count == 0) return 0.0f;
    
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float sample = (float)samples[i];
        sum_sq += sample * sample;
    }
    return sqrtf(sum_sq / count);
}

/**
 * @brief Calculate Root Mean Square (RMS) of float samples
 */
float feature_utils_rms_float(const float *samples, uint32_t count) {
    if (count == 0) return 0.0f;
    
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        sum_sq += samples[i] * samples[i];
    }
    return sqrtf(sum_sq / count);
}

/**
 * @brief Calculate skewness of sample array
 */
float feature_utils_skewness(const uint16_t *samples, uint32_t count, float mean, float variance) {
    if (count < 3 || variance < 1e-6f) return 0.0f;
    
    float sum_cube = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float diff = (float)samples[i] - mean;
        sum_cube += diff * diff * diff;
    }
    
    // Population skewness formula
    return (sum_cube / count) / powf(variance, 1.5f);
}

/**
 * @brief Calculate kurtosis of sample array
 */
float feature_utils_kurtosis(const uint16_t *samples, uint32_t count, float mean, float variance) {
    if (count < 4 || variance < 1e-6f) return 0.0f;
    
    float sum_quad = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float diff = (float)samples[i] - mean;
        sum_quad += diff * diff * diff * diff;
    }
    
    // Population kurtosis formula (excess kurtosis would subtract 3)
    return (sum_quad / count) / (variance * variance);
}

/**
 * @brief Calculate Crest Factor (peak-to-RMS ratio)
 */
float feature_utils_crest_factor(const uint16_t *samples, uint32_t count, float rms) {
    if (count == 0) return 0.0f;
    
    // Calculate RMS if not provided
    if (rms < 1e-6f) {
        rms = feature_utils_rms(samples, count);
    }
    
    if (rms < 1e-6f) return 0.0f;
    
    // Find peak-to-peak value
    uint16_t min_val, max_val;
    feature_utils_min_max(samples, count, &min_val, &max_val);
    
    float peak_to_peak = (float)(max_val - min_val);
    // Crest factor = peak amplitude / RMS
    // For peak-to-peak, we use half of it as peak amplitude
    return (peak_to_peak / 2.0f) / rms;
}

/**
 * @brief Calculate Form Factor (RMS/mean absolute value)
 */
float feature_utils_form_factor(const uint16_t *samples, uint32_t count, float rms) {
    if (count == 0) return 0.0f;
    
    // Calculate RMS if not provided
    if (rms < 1e-6f) {
        rms = feature_utils_rms(samples, count);
    }
    
    if (rms < 1e-6f) return 0.0f;
    
    // Calculate mean absolute value
    float sum_abs = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        sum_abs += fabsf((float)samples[i]);
    }
    float mean_abs = sum_abs / count;
    
    if (mean_abs < 1e-6f) return 0.0f;
    return rms / mean_abs;
}

/**
 * @brief Find minimum and maximum values in sample array
 */
void feature_utils_min_max(const uint16_t *samples, uint32_t count, uint16_t *min_val, uint16_t *max_val) {
    if (count == 0 || !min_val || !max_val) return;
    
    *min_val = samples[0];
    *max_val = samples[0];
    
    for (uint32_t i = 1; i < count; i++) {
        if (samples[i] < *min_val) *min_val = samples[i];
        if (samples[i] > *max_val) *max_val = samples[i];
    }
}

/**
 * @brief Calculate mean absolute value of samples
 */
float feature_utils_mean_absolute(const uint16_t *samples, uint32_t count) {
    if (count == 0) return 0.0f;
    
    float sum_abs = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        sum_abs += fabsf((float)samples[i]);
    }
    return sum_abs / count;
}

/**
 * @brief Calculate standard deviation of samples
 */
float feature_utils_std_dev(const uint16_t *samples, uint32_t count, float mean) {
    float variance = feature_utils_variance(samples, count, mean);
    return sqrtf(variance);
}

/**
 * @brief Calculate simple moving average
 */
void feature_utils_moving_average(const uint16_t *samples, uint32_t count, uint32_t window_size, float *output) {
    if (count == 0 || window_size == 0 || !output) return;
    
    if (window_size > count) window_size = count;
    
    // Initialize output with zeros
    memset(output, 0, count * sizeof(float));
    
    // Calculate moving average
    for (uint32_t i = 0; i < count; i++) {
        uint32_t start = (i > window_size - 1) ? i - window_size + 1 : 0;
        uint32_t end = i;
        uint32_t window_count = end - start + 1;
        
        float sum = 0.0f;
        for (uint32_t j = start; j <= end; j++) {
            sum += (float)samples[j];
        }
        output[i] = sum / window_count;
    }
}

/**
 * @brief Normalize samples to range [0, 1]
 */
void feature_utils_normalize(const uint16_t *samples, uint32_t count, float *output) {
    if (count == 0 || !output) return;
    
    uint16_t min_val, max_val;
    feature_utils_min_max(samples, count, &min_val, &max_val);
    
    float range = (float)(max_val - min_val);
    if (range < 1e-6f) {
        // All values are the same
        for (uint32_t i = 0; i < count; i++) {
            output[i] = 0.5f;
        }
    } else {
        for (uint32_t i = 0; i < count; i++) {
            output[i] = ((float)samples[i] - min_val) / range;
        }
    }
}

/**
 * @brief Calculate energy of signal (sum of squares)
 */
float feature_utils_energy(const uint16_t *samples, uint32_t count) {
    if (count == 0) return 0.0f;
    
    float energy = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float sample = (float)samples[i];
        energy += sample * sample;
    }
    return energy;
}

/**
 * @brief Calculate power of signal (average energy)
 */
float feature_utils_power(const uint16_t *samples, uint32_t count) {
    if (count == 0) return 0.0f;
    return feature_utils_energy(samples, count) / count;
}