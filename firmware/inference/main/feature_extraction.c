/**
 * @file feature_extraction.c
 * @brief Signal feature extraction for machine learning
 * @details Calculates time-domain and frequency-domain features from signal windows.
 *          Includes mean, variance, RMS, zero-crossing rate, skewness, kurtosis, 
 *          crest factor and simple FFT magnitudes.
 * 
 * @author Darkhan Zhanibekuly
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Uses floating-point math; ensure FPU is enabled
 * @note FFT implementation is a placeholder; replace with optimized library
 */

#include "feature_extraction.h"
#include "common.h"
#include "feature_utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Private helper functions specific to this module

/**
 * @brief Calculate zero-crossing rate of sample array
 */
float calculate_zero_crossing_rate(const uint16_t *samples, uint32_t count) {
    if (count < 2) return 0.0f;
    
    // Use memory pool for DC removal buffer
    float *dc_removed = (float*)malloc(count * sizeof(float));
    if (!dc_removed) return 0.0f;
    
    feature_utils_remove_dc_offset(samples, count, dc_removed);
    
    uint32_t zero_crossings = 0;
    
    for (uint32_t i = 1; i < count; i++) {
        if ((dc_removed[i-1] > 0.0f && dc_removed[i] <= 0.0f) || 
            (dc_removed[i-1] <= 0.0f && dc_removed[i] > 0.0f)) {
            zero_crossings++;
        }
    }
    
    // Release buffer back to pool
    free(dc_removed);
    
    return (float)zero_crossings / (count - 1);
}

/**
 * @brief Calculate signal periodicity using autocorrelation
 */
float calculate_periodicity(const uint16_t *samples, uint32_t count) {
    if (count < 64) return 0.0f;
    
    // Use shared utility for mean calculation
    float mean = feature_utils_mean(samples, count);
    float max_corr = 0.0f;
    
    // Only check reasonable lags (up to 1/4 of window)
    for (int lag = 8; lag < count/4; lag++) {
        float correlation = 0.0f;
        float norm1 = 0.0f;
        float norm2 = 0.0f;
        
        for (int i = 0; i < count - lag; i++) {
            float x1 = samples[i] - mean;
            float x2 = samples[i + lag] - mean;
            correlation += x1 * x2;
            norm1 += x1 * x1;
            norm2 += x2 * x2;
        }
        
        if (norm1 > 0.0f && norm2 > 0.0f) {
            float corr = correlation / sqrtf(norm1 * norm2);
            if (corr > max_corr) {
                max_corr = corr;
            }
        }
    }
    
    return max_corr;
}

/**
 * @brief Calculate harmonic ratio (energy in harmonics vs fundamental)
 */
float calculate_harmonic_ratio(const uint16_t *samples, uint32_t count) {
    // Simple placeholder - replace with actual FFT
    float *normalized = get_float_buffer(count);
    if (!normalized) return 0.0f;
    
    // Calculate mean removed samples
    float mean = feature_utils_mean(samples, count);
    for (int i = 0; i < count; i++) {
        normalized[i] = samples[i] - mean;
    }
    
    // Simple harmonic estimation using zero crossings
    float zcr = calculate_zero_crossing_rate(samples, count);
    
    // Heuristic: sawtooth has higher variance and moderate ZCR
    release_float_buffer(normalized);
    
    if (zcr > 0.3f) return 0.1f;  // Sine-like
    if (zcr < 0.05f) return 0.3f; // Square-like
    return 0.6f;  // Triangle/Sawtooth
}

/**
 * @brief Calculate slope asymmetry (detects sawtooth vs triangle)
 */
float calculate_asymmetry(const uint16_t *samples, uint32_t count) {
    if (count < 2) return 0.5f;
    
    uint16_t min_val, max_val;
    int min_idx = 0;
    int max_idx = 0;
    
    // Find min and max using shared utility
    min_val = samples[0];
    max_val = samples[0];
    
    for (int i = 1; i < count; i++) {
        if (samples[i] < min_val) {
            min_val = samples[i];
            min_idx = i;
        }
        if (samples[i] > max_val) {
            max_val = samples[i];
            max_idx = i;
        }
    }
    
    if (max_val == min_val) return 0.5f;
    
    // Analyze rise time vs fall time
    float normalized_min = (float)(min_idx) / count;
    float normalized_max = (float)(max_idx) / count;
    
    // Asymmetry ratio
    float asymmetry = fabsf(normalized_max - normalized_min);
    
    return asymmetry;
}

/**
 * @brief Extract full feature vector from signal window
 */
void extract_features(const window_buffer_t *window, feature_vector_t *features) {
    // Basic features using shared utilities
    float mean = feature_utils_mean(window->samples, WINDOW_SIZE);
    float variance = feature_utils_variance(window->samples, WINDOW_SIZE, mean);
    float rms = feature_utils_rms(window->samples, WINDOW_SIZE);
    float zcr = calculate_zero_crossing_rate(window->samples, WINDOW_SIZE);
    
    // Advanced features using shared utilities
    float skewness = feature_utils_skewness(window->samples, WINDOW_SIZE, mean, variance);
    float kurtosis = feature_utils_kurtosis(window->samples, WINDOW_SIZE, mean, variance);
    float crest_factor = feature_utils_crest_factor(window->samples, WINDOW_SIZE, rms);
    float form_factor = feature_utils_form_factor(window->samples, WINDOW_SIZE, rms);
    
    // New features for better discrimination
    float periodicity = calculate_periodicity(window->samples, WINDOW_SIZE);
    float harmonic_ratio = calculate_harmonic_ratio(window->samples, WINDOW_SIZE);
    float asymmetry = calculate_asymmetry(window->samples, WINDOW_SIZE);
    
    // Peak statistics using shared utility
    uint16_t min_val, max_val;
    feature_utils_min_max(window->samples, WINDOW_SIZE, &min_val, &max_val);
    float peak_to_peak = max_val - min_val;
    float duty_cycle_estimate = 0.5f;  // Placeholder
    
    // Energy and power using shared utilities
    float energy = feature_utils_energy(window->samples, WINDOW_SIZE);
    float power = feature_utils_power(window->samples, WINDOW_SIZE);
    
    // Populate feature vector (now 18 features)
    features->features[0] = mean;                 // 0: Mean
    features->features[1] = variance;             // 1: Variance
    features->features[2] = rms;                  // 2: RMS
    features->features[3] = zcr;                  // 3: Zero Crossing Rate
    features->features[4] = skewness;             // 4: Skewness
    features->features[5] = kurtosis;             // 5: Kurtosis
    features->features[6] = crest_factor;         // 6: Crest Factor
    features->features[7] = form_factor;          // 7: Form Factor
    features->features[8] = periodicity;          // 8: Periodicity
    features->features[9] = harmonic_ratio;       // 9: Harmonic Ratio
    features->features[10] = asymmetry;           // 10: Asymmetry
    features->features[11] = peak_to_peak;        // 11: Peak-to-Peak
    features->features[12] = (float)min_val;      // 12: Minimum
    features->features[13] = (float)max_val;      // 13: Maximum
    features->features[14] = duty_cycle_estimate; // 14: Duty Cycle (est)
    features->features[15] = (float)window->sample_rate_hz / 1000.0f; // 15: Sample Rate (kHz)
    features->features[16] = energy;              // 16: Energy
    features->features[17] = power;               // 17: Power
    
    // Copy metadata
    features->timestamp_us = window->timestamp_us;
    features->window_id = window->window_id;
}

/**
 * @brief Print feature vector for debugging
 */
void print_features(const feature_vector_t *features) {
    printf("Features [ID:%lu]:\n", features->window_id);
    printf("  Mean: %.2f, Variance: %.2f, RMS: %.2f, ZCR: %.3f\n", 
           features->features[0], features->features[1], 
           features->features[2], features->features[3]);
    printf("  Skewness: %.3f, Kurtosis: %.3f, Crest: %.3f, Form: %.3f\n",
           features->features[4], features->features[5],
           features->features[6], features->features[7]);
    printf("  Periodicity: %.3f, Harmonic Ratio: %.3f, Asymmetry: %.3f\n",
           features->features[8], features->features[9], features->features[10]);
    printf("  Min: %.0f, Max: %.0f, P2P: %.1f\n",
           features->features[12], features->features[13], features->features[11]);
    printf("  Energy: %.1f, Power: %.1f\n",
           features->features[16], features->features[17]);
    printf("  Sample Rate: %.1f kHz\n", features->features[15]);
}