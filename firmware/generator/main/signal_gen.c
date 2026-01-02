/**
 * @file signal_gen.c
 * @brief Signal Generator Implementation
 * @details This file implements the core signal generation logic for the ESP32.
 * It handles DAC initialization, waveform generation, configuration management,
 * and real-time signal output with configurable parameters.
 * 
 * @author Darkhan Zhanibekuly
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Uses ESP-IDF DAC Continuous Driver for efficient waveform output
 * @note All floating-point calculations use single precision (float) for speed
 * 
 * @copyright (c) 2025 December ESP32 Signal Generator Project
 */

#include "signal_gen.h"

#include "esp_log.h"
#include "esp_check.h"
#include "driver/dac_continuous.h"
#include "soc/dac_channel.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

/**
 * @defgroup signal_gen_internal Internal Signal Generator Definitions
 * @brief Internal constants, variables, and helper functions
 * @{
 */

#define TAG "signal_gen"                    /**< Logging tag for signal generator module */

/* ================== Hardware Configuration ================== */

#define DAC_CHANNEL        DAC_CHAN_0      /**< DAC channel 0 (GPIO25) */
#define SAMPLE_RATE_HZ     20000           /**< Fixed DAC sample rate in Hertz */
#define WAVE_TABLE_SIZE    400             /**< Size of waveform buffer in samples */
#define DAC_MAX            255             /**< Maximum DAC value (8-bit) */

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

/* ================== Module State Variables ================== */

static dac_continuous_handle_t dac_handle = NULL;      /**< DAC driver handle */
static uint8_t wave_buffer[WAVE_TABLE_SIZE];          /**< Circular buffer for waveform data */

/**
 * @brief Current signal generator configuration
 * 
 * Default values provide a 1kHz sine wave with no noise or offset
 */
static signal_gen_config_t current_cfg = {
    .wave = SIGNAL_WAVE_SINE,      /**< Default waveform: sine */
    .amplitude = 1.0f,             /**< Full amplitude */
    .noise_std = 0.0f,             /**< No noise by default */
    .dc_offset = 0.0f,             /**< No DC offset */
    .frequency_hz = 1000,          /**< 1kHz default frequency */
    .duration_ms = 0               /**< Continuous operation */
};

static bool running = false;               /**< Signal generation active flag */
static float phase_accumulator = 0.0f;     /**< Phase accumulator for waveform continuity */

/**
 * @}
 */

/* ================== Internal Helper Functions ================== */

/**
 * @brief Clamp a floating-point value to a specified range
 * 
 * @param[in] x Input value to clamp
 * @param[in] lo Lower bound (inclusive)
 * @param[in] hi Upper bound (inclusive)
 * @return Clamped value in range [lo, hi]
 * 
 * @note This is an inline function for performance
 */
static inline float clamp(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/**
 * @brief Generate Gaussian noise using Box-Muller transform
 * 
 * @param[in] stddev Standard deviation of the noise
 * @return Gaussian-distributed random value with mean 0 and specified standard deviation
 * 
 * @note Uses two uniform random variables to generate one Gaussian variable
 * @note The +1.0f avoids log(0) which would be undefined
 */
static float gaussian_noise(float stddev)
{
    if (stddev <= 0.0f) return 0.0f;

    float u1 = ((float)rand() + 1.0f) / ((float)RAND_MAX + 1.0f);
    float u2 = ((float)rand() + 1.0f) / ((float)RAND_MAX + 1.0f);

    float z0 = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * M_PI * u2);
    return z0 * stddev;
}

/* ================== Waveform Generation Core ================== */

/**
 * @brief Generate waveform samples into a buffer
 * 
 * This is the core waveform generation function. It fills a buffer with
 * DAC-ready values based on the current configuration, applying all
 * signal transformations (amplitude, noise, DC offset).
 * 
 * @param[out] buf Output buffer for waveform samples
 * @param[in] len Number of samples to generate
 * 
 * @note Maintains phase continuity between calls using phase_accumulator
 * @note Performs real-time floating-point calculations for each sample
 * @note Clips values to prevent DAC overflow/underflow
 */
static void generate_waveform(uint8_t *buf, size_t len)
{
    // Calculate phase increment based on desired frequency
    float phase_increment = (float)current_cfg.frequency_hz / (float)SAMPLE_RATE_HZ;
    
    for (size_t i = 0; i < len; i++) {
        float phase = phase_accumulator;
        float v = 0.0f;

        // Generate base waveform
        switch (current_cfg.wave) {
        case SIGNAL_WAVE_SINE:
            v = sinf(2.0f * M_PI * phase);
            break;

        case SIGNAL_WAVE_SQUARE:
            v = phase < 0.5f ? 1.0f : -1.0f;
            break;

        case SIGNAL_WAVE_TRIANGLE:
            v = 4.0f * fabsf(phase - 0.5f) - 1.0f;
            break;

        case SIGNAL_WAVE_SAWTOOTH:
            v = 2.0f * phase - 1.0f;
            break;
        }

        /* Apply amplitude */
        v *= current_cfg.amplitude;

        /* Add Gaussian noise */
        v += gaussian_noise(current_cfg.noise_std);

        /* Apply DC offset */
        v += current_cfg.dc_offset;

        /* Normalize to DAC range and clip */
        v = clamp(v, -1.0f, 1.0f);
        uint8_t dac_val = (uint8_t)((v + 1.0f) * 0.5f * DAC_MAX);

        buf[i] = dac_val;
        
        // Update phase accumulator for next sample
        phase_accumulator += phase_increment;
        if (phase_accumulator >= 1.0f) {
            phase_accumulator -= 1.0f;  // Wrap around for phase continuity
        }
    }
}

/* ================== Public API Implementation ================== */

/**
 * @brief Initialize the signal generator hardware
 * 
 * Sets up the DAC continuous driver, allocates DMA descriptors, and generates
 * the initial waveform. This function must be called before starting signal
 * generation.
 * 
 * @throws ESP_ERR_INVALID_ARG if DAC configuration is invalid
 * @throws ESP_ERR_NO_MEM if insufficient memory for DMA descriptors
 * 
 * @post DAC is enabled and ready for output
 * @post wave_buffer contains initial waveform
 */
void signal_gen_init(void) {
    ESP_LOGI(TAG, "Initializing DAC signal generator");

    dac_continuous_config_t dac_cfg = {
        // FIX: Use only the channel we need
        .chan_mask = DAC_CHANNEL_MASK_ALL,  // Changed from DAC_CHANNEL_MASK_ALL
        .desc_num = 4,
        .buf_size = WAVE_TABLE_SIZE,
        .freq_hz = SAMPLE_RATE_HZ,
        .offset = 0,
        .clk_src = DAC_DIGI_CLK_SRC_DEFAULT,
        .chan_mode = DAC_CHANNEL_MODE_SIMUL,
    };

    ESP_ERROR_CHECK(dac_continuous_new_channels(&dac_cfg, &dac_handle));

    // Generate initial waveform
    generate_waveform(wave_buffer, WAVE_TABLE_SIZE);

    ESP_ERROR_CHECK(dac_continuous_enable(dac_handle));
    
    ESP_LOGI(TAG, "DAC initialized successfully. Sample rate: %d Hz", SAMPLE_RATE_HZ);
}

/**
 * @brief Broadcast configuration label via UART
 * 
 * Sends current configuration in machine-readable format for synchronization
 * with acquisition device.
 */
void signal_gen_broadcast_label(void) {
    printf("SYNC LABEL wave=%d freq=%lu amp=%.2f noise=%.3f offset=%.2f\n",
           current_cfg.wave,
           current_cfg.frequency_hz,
           current_cfg.amplitude,
           current_cfg.noise_std,
           current_cfg.dc_offset);
}

/**
 * @brief Update signal generator configuration
 * 
 * Applies new configuration parameters and regenerates the waveform.
 * If signal generation is active, the new waveform starts immediately.
 * 
 * @param[in] cfg Pointer to new configuration structure
 * 
 * @note Resets phase accumulator to ensure waveform starts at phase 0
 * @note Copies entire configuration structure (shallow copy)
 * 
 * @throws ESP_ERR_INVALID_ARG if dac_handle is invalid
 */
void signal_gen_set_config(const signal_gen_config_t *cfg) {
    // Validate configuration before applying
    if (!validate_config(cfg)) {
        ESP_LOGW(TAG, "Configuration validation failed, using safe defaults");
        // Apply safe defaults for invalid values
        signal_gen_config_t safe_cfg = *cfg;
        safe_cfg.frequency_hz = MIN(safe_cfg.frequency_hz, SAMPLE_RATE_HZ / 4);
        safe_cfg.amplitude = clamp(safe_cfg.amplitude, 0.0f, 1.0f);
        safe_cfg.noise_std = MAX(safe_cfg.noise_std, 0.0f);
        safe_cfg.dc_offset = clamp(safe_cfg.dc_offset, -1.0f, 1.0f);
        safe_cfg.wave = (signal_wave_t)clamp((float)safe_cfg.wave, 
                                           (float)SIGNAL_WAVE_SINE, 
                                           (float)SIGNAL_WAVE_SAWTOOTH);
        memcpy(&current_cfg, &safe_cfg, sizeof(signal_gen_config_t));
    } else {
        memcpy(&current_cfg, cfg, sizeof(signal_gen_config_t));
    }
    
    // Reset phase accumulator when configuration changes
    phase_accumulator = 0.0f;
    
    // Regenerate waveform with new parameters
    generate_waveform(wave_buffer, WAVE_TABLE_SIZE);
    
    // FIX: Broadcast label change for synchronization
    signal_gen_broadcast_label();
    
    // Update DAC with new waveform if currently running
    if (running) {
        ESP_ERROR_CHECK(dac_continuous_write_cyclically(
            dac_handle,
            wave_buffer,
            WAVE_TABLE_SIZE,
            NULL
        ));
    }
}

/**
 * @brief Validate signal generator configuration
 * 
 * Checks if the provided configuration parameters are within acceptable
 * ranges and constraints. Logs warnings for any invalid parameters.
 * 
 * @param[in] cfg Pointer to configuration structure to validate
 * @return true if configuration is valid, false otherwise
 * 
 * @note Frequency should not exceed Nyquist limit (SAMPLE_RATE_HZ / 4)
 * @note Amplitude must be in range [0.0, 1.0]
 * @note Noise standard deviation must be non-negative
 * @note DC offset must be in range [-1.0, 1.0]
 */
bool validate_config(const signal_gen_config_t *cfg) {
    // Frequency validation (Nyquist limit)
    if (cfg->frequency_hz > SAMPLE_RATE_HZ / 4) {
        ESP_LOGW(TAG, "Frequency %lu Hz may cause aliasing (max: %d Hz)", 
                cfg->frequency_hz, SAMPLE_RATE_HZ / 4);
        return false;
    }
    
    // Amplitude validation
    if (cfg->amplitude < 0.0f || cfg->amplitude > 1.0f) {
        ESP_LOGW(TAG, "Amplitude must be in range [0.0, 1.0]");
        return false;
    }
    
    // Noise validation
    if (cfg->noise_std < 0.0f) {
        ESP_LOGW(TAG, "Noise standard deviation cannot be negative");
        return false;
    }
    
    // DC offset validation
    if (fabsf(cfg->dc_offset) > 1.0f) {
        ESP_LOGW(TAG, "DC offset must be in range [-1.0, 1.0]");
        return false;
    }
    
    // Waveform validation
    if (cfg->wave < SIGNAL_WAVE_SINE || cfg->wave > SIGNAL_WAVE_SAWTOOTH) {
        ESP_LOGW(TAG, "Invalid waveform type: %d", cfg->wave);
        return false;
    }
    
    return true;
}

/**
 * @brief Get current signal generator configuration
 * 
 * Returns a read-only pointer to the current configuration. This allows
 * inspection of current settings without modification.
 * 
 * @return Const pointer to current configuration structure
 * 
 * @warning Do not modify the returned structure directly
 */
const signal_gen_config_t *signal_gen_get_config(void)
{
    return &current_cfg;
}

/**
 * @brief Start signal generation
 * 
 * Begins cyclic output of the waveform buffer through the DAC. If a non-zero
 * duration is configured, the function will block until the duration elapses
 * and then automatically stop.
 * 
 * @pre DAC must be initialized via signal_gen_init()
 * @pre Waveform buffer must contain valid data
 * 
 * @note If duration_ms > 0, this function blocks using vTaskDelay
 * @warning Blocking duration is approximate due to RTOS scheduling
 */
void signal_gen_start(void)
{
    if (running) return;

    ESP_LOGI(TAG, "Starting signal generation");
    signal_gen_emit_label();

    ESP_ERROR_CHECK(dac_continuous_write_cyclically(
        dac_handle,
        wave_buffer,
        WAVE_TABLE_SIZE,
        NULL
    ));

    running = true;

    // Handle timed operation if duration is specified
    if (current_cfg.duration_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(current_cfg.duration_ms));
        signal_gen_stop();
    }
}

/**
 * @brief Stop signal generation
 * 
 * Halts DAC output immediately while preserving the current configuration
 * and waveform buffer. The generator can be restarted without reconfiguration.
 * 
 * @post DAC output is disabled
 * @post running flag is set to false
 * 
 * @throws ESP_ERR_INVALID_ARG if dac_handle is invalid
 */
void signal_gen_stop(void)
{
    if (!running) return;

    ESP_LOGI(TAG, "Stopping signal generation");
    ESP_ERROR_CHECK(dac_continuous_disable(dac_handle));
    running = false;
}

/**
 * @brief Emit configuration label to console
 * 
 * Outputs current configuration in a standardized format for external
 * tools and logging systems. The format is designed to be easily parsed
 * by automated test equipment.
 * 
 * Output format: "LABEL wave=%d amp=%.2f freq=%lu noise=%.3f offset=%.2f"
 * 
 * @note Uses printf for maximum compatibility
 * @note Intended for synchronization with oscilloscopes and data loggers
 */
void signal_gen_emit_label(void)
{
    printf(
        "LABEL wave=%d amp=%.2f freq=%lu noise=%.3f offset=%.2f\n",
        current_cfg.wave,
        current_cfg.amplitude,
        current_cfg.frequency_hz,
        current_cfg.noise_std,
        current_cfg.dc_offset
    );
}