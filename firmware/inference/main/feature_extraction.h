/**
 * @file feature_extraction.h
 * @brief Header for signal feature extraction module
 * @details Defines feature vector structure and API for calculating
 *          time-domain and frequency-domain features from signal windows.
 * 
 * @author Your Name
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Uses floating-point calculations; ensure FPU is enabled
 * @note FFT implementation is a placeholder; replace with optimized library
 * 
 * @copyright (c) 2025 ESP32 Signal Processing Project
 */

#pragma once

#include "signal_acquisition.h"
#include <stdint.h>

#define FEATURE_VECTOR_SIZE 16  /**< Dimensionality of feature vector for ML */

/**
 * @brief Feature vector structure
 * 
 * Contains extracted features along with metadata for tracking
 * and correlation with source windows.
 */
typedef struct {
    float features[FEATURE_VECTOR_SIZE];  /**< Array of extracted features */
    uint64_t timestamp_us;                /**< Microsecond timestamp from source window */
    uint32_t window_id;                   /**< Reference to source window ID */
} feature_vector_t;

/**
 * @brief Extract full feature vector from signal window
 * 
 * Computes comprehensive feature set including time-domain statistics
 * and frequency-domain characteristics for machine learning inference.
 * 
 * @param[in] window Pointer to window buffer containing raw samples
 * @param[out] features Pointer to feature vector to populate
 * 
 * @note Feature order: [mean, variance, RMS, ZCR, FFT0-7, sample_rate, window_size]
 * @note Copies window metadata to feature vector
 */
void extract_features(const window_buffer_t *window, feature_vector_t *features);

/**
 * @brief Print feature vector for debugging
 * 
 * Outputs all feature values to console in human-readable format.
 * 
 * @param[in] features Pointer to feature vector structure
 */
void print_features(const feature_vector_t *features);

/**
 * @brief Calculate mean of sample array
 * 
 * Computes arithmetic mean (average) of sample values.
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @return float Arithmetic mean
 */
float calculate_mean(const uint16_t *samples, uint32_t count);

/**
 * @brief Calculate variance of sample array
 * 
 * Computes variance (average squared deviation from mean).
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[in] mean Pre-calculated mean of samples
 * @return float Variance value
 */
float calculate_variance(const uint16_t *samples, uint32_t count, float mean);

/**
 * @brief Calculate Root Mean Square (RMS) of samples
 * 
 * Computes quadratic mean (RMS) of sample values.
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @return float RMS value
 */
float calculate_rms(const uint16_t *samples, uint32_t count);

/**
 * @brief Calculate Zero Crossing Rate (ZCR)
 * 
 * Computes rate of sign changes, normalized by window length.
 * Useful for distinguishing periodic from random signals.
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @return float ZCR (0.0 to 1.0)
 */
float calculate_zero_crossing_rate(const uint16_t *samples, uint32_t count);

/**
 * @brief Calculate FFT-based frequency features
 * 
 * Extracts magnitude features from frequency domain (placeholder).
 * Replace with proper FFT implementation (e.g., ESP-DSP library).
 * 
 * @param[in] samples Input time-domain samples
 * @param[in] count Number of samples (should be power of two)
 * @param[out] fft_features Output array for FFT magnitude features
 * 
 * @note Currently returns simple magnitudes; implement with esp32-dsp for production
 */
void calculate_fft_features(const uint16_t *samples, uint32_t count, float *fft_features);