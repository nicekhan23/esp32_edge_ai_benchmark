#ifndef SIGNAL_PROCESSING_H
#define SIGNAL_PROCESSING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Signal quality types
typedef enum {
    SIGNAL_OK = 0,
    SIGNAL_SATURATED,
    SIGNAL_TOO_SMALL,
    SIGNAL_TOO_NOISY,
    SIGNAL_DC_OFFSET,
    SIGNAL_INVALID
} signal_quality_t;

// Signal statistics
typedef struct {
    float mean;
    float rms;
    float peak_to_peak;
    float zero_crossing_rate;
    float crest_factor;
    float snr_estimate;
} signal_stats_t;

// Preprocessing options
typedef enum {
    PREPROCESS_DC_REMOVAL = 0x01,
    PREPROCESS_NORMALIZE = 0x02,
    PREPROCESS_WINDOWING = 0x04,
    PREPROCESS_ALL = 0x07
} preprocessing_options_t;

/**
 * @brief Validate signal quality
 */
signal_quality_t validate_signal(float *samples, int num_samples);

/**
 * @brief Calculate signal statistics
 */
void calculate_signal_stats(float *samples, int num_samples, signal_stats_t *stats);

/**
 * @brief Check if signal is suitable for inference
 */
bool is_signal_suitable(float *samples, int num_samples, float min_amplitude, float max_dc_offset);

/**
 * @brief Preprocess signal samples
 */
void preprocess_samples_fixed(float *samples, int num_samples, preprocessing_options_t options);

/**
 * @brief Remove DC offset
 */
void remove_dc_offset(float *samples, int num_samples);

/**
 * @brief Normalize samples to [-1, 1] range
 */
void normalize_samples(float *samples, int num_samples);

/**
 * @brief Apply Hann window to samples
 */
void apply_hann_window(float *samples, int num_samples);

/**
 * @brief Calculate FFT of samples
 */
bool compute_fft_fixed(float *samples, int num_samples);

#ifdef __cplusplus
}
#endif

#endif /* SIGNAL_PROCESSING_H */