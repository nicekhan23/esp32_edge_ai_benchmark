#include "esp_log.h"
#include "signal_processing.h"
#include <string.h>
#include <math.h>

static const char *TAG = "SIGNAL_VALID";

// Default thresholds (can be adjusted based on application)
#define DEFAULT_SATURATION_THRESHOLD 0.95f    // 95% of full scale
#define DEFAULT_MIN_AMPLITUDE 0.1f            // 10% of full scale
#define DEFAULT_MAX_DC_OFFSET 0.3f            // 30% DC offset
#define DEFAULT_MAX_NOISE 0.1f                // 10% noise level

signal_quality_t validate_signal(float *samples, int num_samples) {
    if (!samples || num_samples <= 0) {
        return SIGNAL_INVALID;
    }
    
    float min = INFINITY, max = -INFINITY;
    float sum = 0.0f, sum_sq = 0.0f;
    int zero_crossings = 0;
    
    for (int i = 0; i < num_samples; i++) {
        float val = samples[i];
        sum += val;
        sum_sq += val * val;
        
        if (val < min) min = val;
        if (val > max) max = val;
        
        if (i > 0 && val * samples[i-1] < 0) {
            zero_crossings++;
        }
    }
    
    float mean = sum / num_samples;
    float rms = sqrtf(sum_sq / num_samples);
    float peak_to_peak = max - min;
    float peak = fmaxf(fabsf(min), fabsf(max));
    
    // Check for saturation
    if (peak > DEFAULT_SATURATION_THRESHOLD) {
        ESP_LOGW(TAG, "Signal saturated: peak=%.3f, threshold=%.3f", 
                 peak, DEFAULT_SATURATION_THRESHOLD);
        return SIGNAL_SATURATED;
    }
    
    // Check signal level
    if (peak_to_peak < DEFAULT_MIN_AMPLITUDE) {
        ESP_LOGW(TAG, "Signal too small: pp=%.3f, threshold=%.3f", 
                 peak_to_peak, DEFAULT_MIN_AMPLITUDE);
        return SIGNAL_TOO_SMALL;
    }
    
    // Check DC offset
    if (fabsf(mean) > DEFAULT_MAX_DC_OFFSET) {
        ESP_LOGW(TAG, "DC offset too high: mean=%.3f, threshold=%.3f", 
                 fabsf(mean), DEFAULT_MAX_DC_OFFSET);
        return SIGNAL_DC_OFFSET;
    }
    
    // Check noise level (simplified SNR estimate)
    float signal_power = fabsf(mean);
    float total_power = rms;
    float noise_estimate = total_power - signal_power;
    
    if (noise_estimate > DEFAULT_MAX_NOISE) {
        ESP_LOGW(TAG, "Signal too noisy: noise=%.3f, threshold=%.3f", 
                 noise_estimate, DEFAULT_MAX_NOISE);
        return SIGNAL_TOO_NOISY;
    }
    
    ESP_LOGD(TAG, "Signal OK: pp=%.3f, mean=%.3f, noise=%.3f, zcr=%.3f", 
             peak_to_peak, mean, noise_estimate, (float)zero_crossings / num_samples);
    
    return SIGNAL_OK;
}

void calculate_signal_stats(float *samples, int num_samples, signal_stats_t *stats) {
    if (!samples || !stats || num_samples <= 0) {
        return;
    }
    
    memset(stats, 0, sizeof(signal_stats_t));
    
    float min = INFINITY, max = -INFINITY;
    float sum = 0.0f, sum_abs = 0.0f, sum_sq = 0.0f;
    int zero_crossings = 0;
    
    for (int i = 0; i < num_samples; i++) {
        float val = samples[i];
        sum += val;
        sum_abs += fabsf(val);
        sum_sq += val * val;
        
        if (val < min) min = val;
        if (val > max) max = val;
        
        if (i > 0 && val * samples[i-1] < 0) {
            zero_crossings++;
        }
    }
    
    stats->mean = sum / num_samples;
    stats->rms = sqrtf(sum_sq / num_samples);
    stats->peak_to_peak = max - min;
    stats->zero_crossing_rate = (float)zero_crossings / num_samples;
    
    // Calculate crest factor (peak / RMS)
    float peak = fmaxf(fabsf(min), fabsf(max));
    stats->crest_factor = (stats->rms > 0.001f) ? peak / stats->rms : 0.0f;
    
    // Simple SNR estimate
    float signal_power = fabsf(stats->mean);
    stats->snr_estimate = (stats->rms - signal_power) > 0.001f ? 
                          signal_power / (stats->rms - signal_power) : 0.0f;
}

bool is_signal_suitable(float *samples, int num_samples, float min_amplitude, float max_dc_offset) {
    signal_quality_t quality = validate_signal(samples, num_samples);
    
    if (quality == SIGNAL_OK) {
        // Additional checks with custom thresholds
        signal_stats_t stats;
        calculate_signal_stats(samples, num_samples, &stats);
        
        if (stats.peak_to_peak >= min_amplitude && fabsf(stats.mean) <= max_dc_offset) {
            return true;
        }
    }
    
    return false;
}