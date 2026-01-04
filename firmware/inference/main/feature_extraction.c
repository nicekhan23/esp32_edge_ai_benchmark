/**
 * @file feature_extraction.c
 * @brief Signal feature extraction for machine learning
 * @details Calculates time-domain and frequency-domain features from signal windows.
 *          Includes mean, variance, RMS, zero-crossing rate, skewness, kurtosis, 
 *          crest factor and simple FFT magnitudes.
 * 
 * @author Your Name
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Uses floating-point math; ensure FPU is enabled
 * @note FFT implementation is a placeholder; replace with optimized library
 * 
 * @copyright (c) 2025 ESP32 Signal Processing Project
 */

#include "feature_extraction.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Private helper functions

/**
 * @brief Calculate mean of sample array
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @return float Arithmetic mean
 */
static float calculate_mean_value(const uint16_t *samples, uint32_t count) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        sum += samples[i];
    }
    return sum / count;
}

/**
 * @brief Remove DC offset from sample array
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[out] output Array to store mean-removed samples
 */
static void remove_dc_offset(const uint16_t *samples, uint32_t count, float *output) {
    float mean = calculate_mean_value(samples, count);
    for (uint32_t i = 0; i < count; i++) {
        output[i] = (float)samples[i] - mean;
    }
}

/**
 * @brief Calculate zero-crossing rate of sample array
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @return float Zero-crossing rate (fraction of crossings)
 */
float calculate_zero_crossing_rate(const uint16_t *samples, uint32_t count) {
    if (count < 2) return 0.0f;
    
    // Use DC-removed samples
    float *dc_removed = malloc(count * sizeof(float));
    if (!dc_removed) return 0.0f;
    
    remove_dc_offset(samples, count, dc_removed);
    
    uint32_t zero_crossings = 0;
    
    for (uint32_t i = 1; i < count; i++) {
        if ((dc_removed[i-1] > 0.0f && dc_removed[i] <= 0.0f) || 
            (dc_removed[i-1] <= 0.0f && dc_removed[i] > 0.0f)) {
            zero_crossings++;
        }
    }
    
    free(dc_removed);
    return (float)zero_crossings / (count - 1);
}

/**
 * @brief Calculate variance of sample array
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[in] mean Pre-calculated mean of samples
 * @return float Variance (average squared deviation from mean)
 */
static float calculate_variance_value(const uint16_t *samples, uint32_t count, float mean) {
    float sum_sq_diff = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float diff = samples[i] - mean;
        sum_sq_diff += diff * diff;
    }
    return sum_sq_diff / count;
}

/**
 * @brief Calculate skewness of sample array
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[in] mean Pre-calculated mean of samples
 * @param[in] variance Pre-calculated variance
 * @return float Skewness (measure of asymmetry)
 */
static float calculate_skewness_value(const uint16_t *samples, uint32_t count, float mean, float variance) {
    if (count < 3 || variance < 1e-6f) return 0.0f;
    
    float sum_cube = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float diff = samples[i] - mean;
        sum_cube += diff * diff * diff;
    }
    
    // Population skewness formula
    return (sum_cube / count) / powf(variance, 1.5f);
}

/**
 * @brief Calculate kurtosis of sample array
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[in] mean Pre-calculated mean of samples
 * @param[in] variance Pre-calculated variance
 * @return float Kurtosis (measure of tailedness)
 */
static float calculate_kurtosis_value(const uint16_t *samples, uint32_t count, float mean, float variance) {
    if (count < 4 || variance < 1e-6f) return 0.0f;
    
    float sum_quad = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float diff = samples[i] - mean;
        sum_quad += diff * diff * diff * diff;
    }
    
    // Population kurtosis formula (excess kurtosis would subtract 3)
    return (sum_quad / count) / (variance * variance);
}

/**
 * @brief Calculate signal periodicity using autocorrelation
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @return float Periodicity score (0.0 to 1.0)
 */
float calculate_periodicity(const uint16_t *samples, uint32_t count) {
    if (count < 64) return 0.0f;
    
    float mean = calculate_mean_value(samples, count);
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
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @return float Harmonic ratio
 */
float calculate_harmonic_ratio(const uint16_t *samples, uint32_t count) {
    // Simple placeholder - replace with actual FFT
    // For sawtooth: rich harmonics
    // For triangle: odd harmonics only
    // For sine: pure tone (no harmonics)
    
    // Calculate mean removed samples
    float mean = calculate_mean_value(samples, count);
    float *normalized = malloc(count * sizeof(float));
    if (!normalized) return 0.0f;
    
    for (int i = 0; i < count; i++) {
        normalized[i] = samples[i] - mean;
    }
    
    // Simple harmonic estimation using zero crossings
    float zcr = calculate_zero_crossing_rate(samples, count);
    
    // Heuristic: sawtooth has higher variance and moderate ZCR
    free(normalized);
    
    if (zcr > 0.3f) return 0.1f;  // Sine-like
    if (zcr < 0.05f) return 0.3f; // Square-like
    return 0.6f;  // Triangle/Sawtooth
}

/**
 * @brief Calculate slope asymmetry (detects sawtooth vs triangle)
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @return float Asymmetry ratio (0.5 = symmetric, >0.5 = positive saw, <0.5 = negative saw)
 */
float calculate_asymmetry(const uint16_t *samples, uint32_t count) {
    if (count < 2) return 0.5f;
    
    uint16_t min_val = samples[0];
    uint16_t max_val = samples[0];
    int min_idx = 0;
    int max_idx = 0;
    
    // Find min and max
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
 * @brief Public wrapper for mean calculation
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @return float Arithmetic mean
 */
float calculate_mean(const uint16_t *samples, uint32_t count) {
    return calculate_mean_value(samples, count);
}

/**
 * @brief Public wrapper for variance calculation
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[in] mean Pre-calculated mean of samples
 * @return float Variance
 */
float calculate_variance(const uint16_t *samples, uint32_t count, float mean) {
    return calculate_variance_value(samples, count, mean);
}

/**
 * @brief Calculate Root Mean Square (RMS) of samples
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @return float RMS value
 */
float calculate_rms(const uint16_t *samples, uint32_t count) {
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        sum_sq += (float)samples[i] * (float)samples[i];
    }
    return sqrtf(sum_sq / count);
}

/**
 * @brief Calculate Crest Factor (peak-to-RMS ratio)
 * 
 * Computes peak-to-RMS ratio, useful for distinguishing waveform types.
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[in] rms RMS value of samples (if 0, will be calculated)
 * @return float Crest factor
 */
float calculate_crest_factor(const uint16_t *samples, uint32_t count, float rms) {
    if (count == 0) return 0.0f;
    
    // Calculate RMS if not provided
    if (rms < 1e-6f) {
        rms = calculate_rms(samples, count);
    }
    
    if (rms < 1e-6f) return 0.0f;
    
    // Find peak-to-peak value
    uint16_t min_val = samples[0];
    uint16_t max_val = samples[0];
    
    for (uint32_t i = 1; i < count; i++) {
        if (samples[i] < min_val) min_val = samples[i];
        if (samples[i] > max_val) max_val = samples[i];
    }
    
    float peak_to_peak = (float)(max_val - min_val);
    // Crest factor = peak amplitude / RMS
    // For peak-to-peak, we use half of it as peak amplitude
    return (peak_to_peak / 2.0f) / rms;
}

/**
 * @brief Calculate Form Factor (RMS/mean absolute value)
 * 
 * Additional feature for waveform shape discrimination.
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[in] rms RMS value of samples (if 0, will be calculated)
 * @return float Form factor
 */
float calculate_form_factor(const uint16_t *samples, uint32_t count, float rms) {
    if (count == 0) return 0.0f;
    
    // Calculate RMS if not provided
    if (rms < 1e-6f) {
        rms = calculate_rms(samples, count);
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
 * @brief Placeholder FFT feature calculation
 * 
 * Currently returns simple magnitudes; replace with proper FFT (e.g., ESP-DSP).
 * 
 * @param[in] samples Input time-domain samples
 * @param[in] count Number of samples (should be power of two)
 * @param[out] fft_features Output array of FFT magnitudes (first 8 bins)
 * 
 * @note Implement with esp32-dsp library for production use
 */
void calculate_fft_features(const uint16_t *samples, uint32_t count, float *fft_features) {
    // Simple magnitude calculation (replace with actual FFT)
    // For sawtooth: expect strong harmonic content (1/f decay)
    for (uint32_t i = 0; i < 8 && i < count/2; i++) {
        // Simple placeholder: use sample values with harmonic decay
        // In real FFT, sawtooth waves have harmonics that decay as 1/n
        if (i * 2 < count) {
            fft_features[i] = (float)samples[i * 2] / (i + 1.0f);
        } else {
            fft_features[i] = 0.0f;
        }
    }
}

/**
 * @brief Extract full feature vector from signal window
 * 
 * Computes time-domain and frequency-domain features, populating
 * a 16-dimensional feature vector for ML inference.
 * 
 * @param[in] window Pointer to window buffer
 * @param[out] features Pointer to feature vector to populate
 * 
 * @note Feature order: [mean, variance, RMS, ZCR, skewness, kurtosis, crest_factor, form_factor, FFT0-6, sample_rate]
 * @note Copies window metadata (timestamp, ID) to feature vector
 */
void extract_features(const window_buffer_t *window, feature_vector_t *features) {
    // Basic features
    float mean = calculate_mean(window->samples, WINDOW_SIZE);
    float variance = calculate_variance(window->samples, WINDOW_SIZE, mean);
    float rms = calculate_rms(window->samples, WINDOW_SIZE);
    float zcr = calculate_zero_crossing_rate(window->samples, WINDOW_SIZE);
    
    // Advanced features
    float skewness = calculate_skewness_value(window->samples, WINDOW_SIZE, mean, variance);
    float kurtosis = calculate_kurtosis_value(window->samples, WINDOW_SIZE, mean, variance);
    float crest_factor = calculate_crest_factor(window->samples, WINDOW_SIZE, rms);
    float form_factor = calculate_form_factor(window->samples, WINDOW_SIZE, rms);
    
    // New features for better discrimination
    float periodicity = calculate_periodicity(window->samples, WINDOW_SIZE);
    float harmonic_ratio = calculate_harmonic_ratio(window->samples, WINDOW_SIZE);
    float asymmetry = calculate_asymmetry(window->samples, WINDOW_SIZE);
    
    // Peak statistics
    uint16_t min_val = window->samples[0];
    uint16_t max_val = window->samples[0];
    for (int i = 1; i < WINDOW_SIZE; i++) {
        if (window->samples[i] < min_val) min_val = window->samples[i];
        if (window->samples[i] > max_val) max_val = window->samples[i];
    }
    float peak_to_peak = max_val - min_val;
    float duty_cycle_estimate = 0.5f;  // Placeholder
    
    // Populate feature vector (now 16 features)
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
    
    // Copy metadata
    features->timestamp_us = window->timestamp_us;
    features->window_id = window->window_id;
}

/**
 * @brief Print feature vector for debugging
 * 
 * @param[in] features Pointer to feature vector
 */
void print_features(const feature_vector_t *features) {
    printf("Features [ID:%lu]:\n", features->window_id);
    printf("  Mean: %.2f, Variance: %.2f, RMS: %.2f, ZCR: %.3f\n", 
           features->features[0], features->features[1], 
           features->features[2], features->features[3]);
    printf("  Skewness: %.3f, Kurtosis: %.3f, Crest: %.3f, Form: %.3f\n",
           features->features[4], features->features[5],
           features->features[6], features->features[7]);
    printf("  FFT: ");
    for (int i = 8; i < 15; i++) {
        printf("%.2f ", features->features[i]);
    }
    printf("\n  Sample Rate: %.1f kHz\n", features->features[15]);
}