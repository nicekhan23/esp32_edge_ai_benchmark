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
 * @brief Calculate Zero Crossing Rate (ZCR)
 * 
 * Counts sign changes between consecutive samples, normalized by window length.
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @return float ZCR (0.0 to 1.0)
 */
float calculate_zero_crossing_rate(const uint16_t *samples, uint32_t count) {
    if (count < 2) return 0.0f;
    
    // Calculate mean for DC offset removal
    float mean = calculate_mean_value(samples, count);
    
    uint32_t zero_crossings = 0;
    float prev_sample = (float)samples[0] - mean;
    
    for (uint32_t i = 1; i < count; i++) {
        float current_sample = (float)samples[i] - mean;
        
        // Check for sign change
        if ((prev_sample > 0.0f && current_sample <= 0.0f) || 
            (prev_sample <= 0.0f && current_sample > 0.0f)) {
            zero_crossings++;
        }
        prev_sample = current_sample;
    }
    
    return (float)zero_crossings / (count - 1);
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
    // Time-domain basic features
    float mean = calculate_mean(window->samples, WINDOW_SIZE);
    float variance = calculate_variance(window->samples, WINDOW_SIZE, mean);
    float rms = calculate_rms(window->samples, WINDOW_SIZE);
    float zcr = calculate_zero_crossing_rate(window->samples, WINDOW_SIZE);
    
    // Time-domain advanced features (for sawtooth detection)
    float skewness = calculate_skewness_value(window->samples, WINDOW_SIZE, mean, variance);
    float kurtosis = calculate_kurtosis_value(window->samples, WINDOW_SIZE, mean, variance);
    float crest_factor = calculate_crest_factor(window->samples, WINDOW_SIZE, rms);
    float form_factor = calculate_form_factor(window->samples, WINDOW_SIZE, rms);
    
    // Frequency-domain features
    float fft_features[8];
    calculate_fft_features(window->samples, WINDOW_SIZE, fft_features);
    
    // Populate feature vector (16 features total)
    features->features[0] = mean;           // Feature 0: Mean
    features->features[1] = variance;       // Feature 1: Variance
    features->features[2] = rms;            // Feature 2: RMS
    features->features[3] = zcr;            // Feature 3: Zero Crossing Rate
    features->features[4] = skewness;       // Feature 4: Skewness (sawtooth asymmetry)
    features->features[5] = kurtosis;       // Feature 5: Kurtosis (waveform shape)
    features->features[6] = crest_factor;   // Feature 6: Crest Factor
    features->features[7] = form_factor;    // Feature 7: Form Factor
    
    // FFT features (7 features)
    for (int i = 0; i < 7 && i < 8; i++) {
        features->features[8 + i] = fft_features[i];
    }
    
    // Additional metadata as features
    features->features[15] = (float)window->sample_rate_hz / 1000.0f; // Normalized sample rate
    
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