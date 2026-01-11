#ifndef PREPROCESSING_H
#define PREPROCESSING_H

#include <stdint.h>
#include <stdbool.h>
#include "signal_processing.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Preprocess signal samples (FIXED ORDER)
 * 
 * @param samples Array of samples (modified in place)
 * @param num_samples Number of samples
 * @param options Bitmask of preprocessing options
 */
void preprocess_samples_fixed(float *samples, int num_samples, preprocessing_options_t options);

/**
 * @brief Remove DC offset (subtract mean)
 * 
 * @param samples Array of samples
 * @param num_samples Number of samples
 */
void remove_dc_offset(float *samples, int num_samples);

/**
 * @brief Normalize samples to [-1, 1] range
 * 
 * @param samples Array of samples
 * @param num_samples Number of samples
 */
void normalize_samples(float *samples, int num_samples);

/**
 * @brief Apply Hann window to samples
 * 
 * @param samples Array of samples
 * @param num_samples Number of samples
 */
void apply_hann_window(float *samples, int num_samples);

/**
 * @brief Calculate FFT of samples using ESP-DSP (no dynamic allocation)
 * 
 * @param samples Real part of samples (input), magnitude (output)
 * @param num_samples Number of samples (must be power of 2)
 * @return true if successful
 */
bool compute_fft_fixed(float *samples, int num_samples);

#ifdef __cplusplus
}
#endif

#endif /* PREPROCESSING_H */