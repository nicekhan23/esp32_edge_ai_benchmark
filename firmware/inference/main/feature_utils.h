/**
 * @file feature_utils.h
 * @brief Shared feature calculation utilities
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

#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Memory pool for temporary float buffers
 */
float* get_float_buffer(uint32_t size);
void release_float_buffer(float* buffer);

/**
 * @brief Calculate mean of sample array
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @return float Arithmetic mean
 */
float feature_utils_mean(const uint16_t *samples, uint32_t count);

/**
 * @brief Remove DC offset from sample array
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[out] output Array to store mean-removed samples (must be pre-allocated)
 * @return float The mean value that was removed
 */
float feature_utils_remove_dc_offset(const uint16_t *samples, uint32_t count, float *output);

/**
 * @brief Calculate variance of sample array
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[in] mean Pre-calculated mean of samples
 * @return float Variance (average squared deviation from mean)
 */
float feature_utils_variance(const uint16_t *samples, uint32_t count, float mean);

/**
 * @brief Calculate Root Mean Square (RMS) of samples
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @return float RMS value
 */
float feature_utils_rms(const uint16_t *samples, uint32_t count);

/**
 * @brief Calculate Root Mean Square (RMS) of float samples
 * 
 * @param[in] samples Array of float samples
 * @param[in] count Number of samples
 * @return float RMS value
 */
float feature_utils_rms_float(const float *samples, uint32_t count);

/**
 * @brief Calculate skewness of sample array
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[in] mean Pre-calculated mean of samples
 * @param[in] variance Pre-calculated variance
 * @return float Skewness (measure of asymmetry)
 */
float feature_utils_skewness(const uint16_t *samples, uint32_t count, float mean, float variance);

/**
 * @brief Calculate kurtosis of sample array
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[in] mean Pre-calculated mean of samples
 * @param[in] variance Pre-calculated variance
 * @return float Kurtosis (measure of tailedness)
 */
float feature_utils_kurtosis(const uint16_t *samples, uint32_t count, float mean, float variance);

/**
 * @brief Calculate Crest Factor (peak-to-RMS ratio)
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[in] rms RMS value of samples (if 0, will be calculated)
 * @return float Crest factor
 */
float feature_utils_crest_factor(const uint16_t *samples, uint32_t count, float rms);

/**
 * @brief Calculate Form Factor (RMS/mean absolute value)
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[in] rms RMS value of samples (if 0, will be calculated)
 * @return float Form factor
 */
float feature_utils_form_factor(const uint16_t *samples, uint32_t count, float rms);

/**
 * @brief Find minimum and maximum values in sample array
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[out] min_val Pointer to store minimum value
 * @param[out] max_val Pointer to store maximum value
 */
void feature_utils_min_max(const uint16_t *samples, uint32_t count, uint16_t *min_val, uint16_t *max_val);

/**
 * @brief Calculate mean absolute value of samples
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @return float Mean absolute value
 */
float feature_utils_mean_absolute(const uint16_t *samples, uint32_t count);

/**
 * @brief Calculate standard deviation of samples
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[in] mean Pre-calculated mean of samples
 * @return float Standard deviation
 */
float feature_utils_std_dev(const uint16_t *samples, uint32_t count, float mean);

/**
 * @brief Calculate simple moving average
 * 
 * @param[in] samples Array of samples
 * @param[in] count Number of samples
 * @param[in] window_size Moving average window size
 * @param[out] output Array to store moving average (must be pre-allocated)
 */
void feature_utils_moving_average(const uint16_t *samples, uint32_t count, uint32_t window_size, float *output);

/**
 * @brief Normalize samples to range [0, 1]
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @param[out] output Array to store normalized samples (must be pre-allocated)
 */
void feature_utils_normalize(const uint16_t *samples, uint32_t count, float *output);

/**
 * @brief Calculate energy of signal (sum of squares)
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @return float Signal energy
 */
float feature_utils_energy(const uint16_t *samples, uint32_t count);

/**
 * @brief Calculate power of signal (average energy)
 * 
 * @param[in] samples Array of ADC samples
 * @param[in] count Number of samples
 * @return float Signal power
 */
float feature_utils_power(const uint16_t *samples, uint32_t count);