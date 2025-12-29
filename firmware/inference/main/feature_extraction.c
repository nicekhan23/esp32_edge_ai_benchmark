/**
 * @file feature_extraction.c
 * @brief Signal feature extraction for machine learning
 * @details Calculates time-domain and frequency-domain features from signal windows.
 *          Includes mean, variance, RMS, zero-crossing rate, and simple FFT magnitudes.
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
        sum_sq += samples[i] * samples[i];
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
    
    uint32_t zero_crossings = 0;
    for (uint32_t i = 1; i < count; i++) {
        if ((samples[i-1] > 0 && samples[i] <= 0) || 
            (samples[i-1] <= 0 && samples[i] > 0)) {
            zero_crossings++;
        }
    }
    return (float)zero_crossings / (count - 1);
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
    for (uint32_t i = 0; i < 8 && i < count/2; i++) {
        fft_features[i] = (float)samples[i*2];
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
 * @note Feature order: [mean, variance, RMS, ZCR, FFT0-7, sample_rate, window_size]
 * @note Copies window metadata (timestamp, ID) to feature vector
 */
void extract_features(const window_buffer_t *window, feature_vector_t *features) {
    // Time-domain features
    float mean = calculate_mean(window->samples, WINDOW_SIZE);
    float variance = calculate_variance(window->samples, WINDOW_SIZE, mean);
    float rms = calculate_rms(window->samples, WINDOW_SIZE);
    float zcr = calculate_zero_crossing_rate(window->samples, WINDOW_SIZE);
    
    // Frequency-domain features
    float fft_features[8];
    calculate_fft_features(window->samples, WINDOW_SIZE, fft_features);
    
    // Populate feature vector
    features->features[0] = mean;
    features->features[1] = variance;
    features->features[2] = rms;
    features->features[3] = zcr;
    
    for (int i = 0; i < 8; i++) {
        features->features[4 + i] = fft_features[i];
    }
    
    // Additional features can be calculated here
    features->features[12] = (float)window->sample_rate_hz;
    features->features[13] = (float)WINDOW_SIZE;
    
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
    printf("Features [ID:%lu]: ", features->window_id);
    for (int i = 0; i < FEATURE_VECTOR_SIZE; i++) {
        printf("%.2f ", features->features[i]);
    }
    printf("\n");
}