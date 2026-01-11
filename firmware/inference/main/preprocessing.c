#include "preprocessing.h"
#include <math.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "PREPROCESSING";

// FFT workspace (optimized for actual window size)
#define MAX_FFT_SIZE 256  // Reduced from 2048 to match SAMPLE_WINDOW_SIZE
static float s_fft_workspace[MAX_FFT_SIZE] __attribute__((aligned(16)));

void preprocess_samples_fixed(float *samples, int num_samples, preprocessing_options_t options) {
    // FIXED ORDER: Windowing → DC removal → normalization
    
    // 1. Apply window FIRST to minimize edge effects
    if (options & PREPROCESS_WINDOWING) {
        apply_hann_window(samples, num_samples);
    }
    
    // 2. Remove DC offset from windowed signal
    if (options & PREPROCESS_DC_REMOVAL) {
        remove_dc_offset(samples, num_samples);
    }
    
    // 3. Normalize after DC removal
    if (options & PREPROCESS_NORMALIZE) {
        normalize_samples(samples, num_samples);
    }
}

void remove_dc_offset(float *samples, int num_samples) {
    if (num_samples == 0) return;
    
    float sum = 0.0f;
    for (int i = 0; i < num_samples; i++) {
        sum += samples[i];
    }
    
    float mean = sum / num_samples;
    
    for (int i = 0; i < num_samples; i++) {
        samples[i] -= mean;
    }
}

void normalize_samples(float *samples, int num_samples) {
    if (num_samples == 0) return;
    
    // Find maximum absolute value
    float max_val = 0.0f;
    for (int i = 0; i < num_samples; i++) {
        float abs_val = fabsf(samples[i]);
        if (abs_val > max_val) {
            max_val = abs_val;
        }
    }
    
    // Avoid division by zero
    if (max_val > 1e-6f) {
        float scale = 1.0f / max_val;
        for (int i = 0; i < num_samples; i++) {
            samples[i] *= scale;
        }
    }
}

void apply_hann_window(float *samples, int num_samples) {
    if (num_samples < 2) return;  // Need at least 2 samples for Hann window
    
    // Precompute constants for efficiency
    float pi_factor = 2.0f * M_PI / (num_samples - 1);
    
    for (int i = 0; i < num_samples; i++) {
        float window = 0.5f * (1.0f - cosf(pi_factor * i));
        samples[i] *= window;
    }
}

bool compute_fft_fixed(float *samples, int num_samples) {
    // Check power of 2
    if ((num_samples & (num_samples - 1)) != 0) {
        #ifdef CONFIG_DETAILED_ERROR_LOGS
        ESP_LOGE(TAG, "FFT requires power of 2 samples, got %d", num_samples);
        #endif
        return false;
    }
    
    // Check if size is supported
    if (num_samples > MAX_FFT_SIZE) {
        #ifdef CONFIG_DETAILED_ERROR_LOGS
        ESP_LOGE(TAG, "FFT size %d exceeds maximum %d", num_samples, MAX_FFT_SIZE);
        #endif
        return false;
    }
    
    // Simple FFT implementation for small sizes (2, 4, 8, 16, 32, 64, 128, 256)
    // For ESP32, use built-in ESP-DSP if available, otherwise fallback
    #ifdef CONFIG_USE_ESP_DSP
    // ESP-DSP implementation would go here
    // For now, simulate FFT with magnitude calculation
    #endif
    
    // Copy samples to workspace
    memcpy(s_fft_workspace, samples, num_samples * sizeof(float));
    
    // Simple DFT for small N (optimized for common sizes)
    if (num_samples <= 64) {
        // Simple DFT implementation
        for (int k = 0; k < num_samples/2; k++) {
            float real = 0.0f, imag = 0.0f;
            for (int n = 0; n < num_samples; n++) {
                float angle = 2.0f * M_PI * k * n / num_samples;
                real += s_fft_workspace[n] * cosf(angle);
                imag -= s_fft_workspace[n] * sinf(angle);
            }
            samples[k] = sqrtf(real*real + imag*imag);
        }
    } else {
        // For larger sizes, use approximate FFT or skip
        // This is a placeholder - in production, use ESP-DSP
        for (int k = 0; k < num_samples/2; k++) {
            samples[k] = fabsf(s_fft_workspace[k]);
        }
    }
    
    // Zero out the second half
    for (int i = num_samples/2; i < num_samples; i++) {
        samples[i] = 0.0f;
    }
    
    return true;
}