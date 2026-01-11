#include "dac_output.h"
#include "waveform_tables.h"
#include "driver/dac_continuous.h"
#include "esp_check.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "dac_output";
static dac_continuous_handle_t dac_handle = NULL;
static waveform_config_t current_config = {
    .type = WAVEFORM_SINE,
    .amplitude = DEFAULT_AMPLITUDE,
    .dc_offset = DEFAULT_DC_OFFSET,
    .frequency_hz = 0
};

// Phase drift simulation
static uint32_t s_phase_offset = 0;
static uint32_t s_phase_increment = 1;  // Samples to shift per cycle

// Map waveform type to LUT
static const uint8_t* get_waveform_lut(waveform_type_t type) {
    switch (type) {
        case WAVEFORM_SINE:     return sine_lut;
        case WAVEFORM_SQUARE:   return square_lut;
        case WAVEFORM_TRIANGLE: return triangle_lut;
        case WAVEFORM_SAWTOOTH: return sawtooth_lut;
        default:                return sine_lut;
    }
}

/**
 * @brief Generate waveform sample with amplitude and offset control
 */
static uint8_t generate_sample(waveform_type_t type, int index, float amplitude, float dc_offset) {
    const uint8_t* lut = get_waveform_lut(type);
    int idx = (index + s_phase_offset) % TABLE_SIZE;
    
    // Scale from 0-255 to 0.0-1.0
    float normalized = lut[idx] / 255.0f;
    
    // Apply amplitude
    normalized *= amplitude;
    
    // Apply DC offset (clamp to 0.0-1.0 range)
    normalized += dc_offset;
    
    // Clamp to valid range
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    
    // Convert back to 8-bit
    return (uint8_t)(normalized * 255.0f);
}

/**
 * @brief Initializes the DAC continuous mode with DMA
 */
void dac_output_init(void) {
    dac_continuous_config_t dac_config = {
        .chan_mask = DAC_CHANNEL_MASK_CH0,  // Only DAC channel 0 (GPIO25)
        .desc_num = 4,                      // Number of DMA descriptors
        .buf_size = DAC_BUFFER_SIZE * 2,    // Buffer size (double buffering)
        .freq_hz = DAC_CONVERT_FREQ_HZ,     // Conversion frequency
        .offset = 0,                        // DC offset
        .clk_src = DAC_DIGI_CLK_SRC_DEFAULT,
        .chan_mode = DAC_CHANNEL_MODE_SIMUL,
    };
    
    // Allocate continuous channel
    ESP_ERROR_CHECK(dac_continuous_new_channels(&dac_config, &dac_handle));
    
    // Enable the DAC channel
    ESP_ERROR_CHECK(dac_continuous_enable(dac_handle));
    
    ESP_LOGI(TAG, "DAC continuous initialized with %d Hz sample rate, buffer size: %d", 
             DAC_CONVERT_FREQ_HZ, DAC_BUFFER_SIZE);
}

/**
 * @brief Switches the active waveform (simple version)
 */
void dac_output_set_waveform(waveform_type_t type) {
    waveform_config_t config = {
        .type = type,
        .amplitude = DEFAULT_AMPLITUDE,
        .dc_offset = DEFAULT_DC_OFFSET,
        .frequency_hz = 0
    };
    dac_output_set_waveform_config(&config);
}

/**
 * @brief Switches the active waveform with full configuration
 */
void dac_output_set_waveform_config(waveform_config_t *config) {
    if (!config) return;
    
    current_config = *config;
    
    if (dac_handle) {
        uint8_t output_buffer[DAC_BUFFER_SIZE];
        
        // Generate waveform with current configuration
        for (int i = 0; i < DAC_BUFFER_SIZE; i++) {
            output_buffer[i] = generate_sample(
                current_config.type, 
                i, 
                current_config.amplitude, 
                current_config.dc_offset
            );
        }
        
        // Increment phase for next cycle
        s_phase_offset = (s_phase_offset + s_phase_increment) % TABLE_SIZE;
        
        // Write the waveform to DAC (will play cyclically)
        ESP_ERROR_CHECK(dac_continuous_write_cyclically(dac_handle, 
                                                       output_buffer, 
                                                       DAC_BUFFER_SIZE, 
                                                       NULL));
        
        ESP_LOGI(TAG, "Waveform switched to %d (amp: %.2f, offset: %.2f)", 
                 current_config.type, current_config.amplitude, current_config.dc_offset);
    }
}

/**
 * @brief Starts DAC output
 */
void dac_output_start(void) {
    if (dac_handle) {
        // Start with current waveform
        uint8_t output_buffer[DAC_BUFFER_SIZE];
        
        for (int i = 0; i < DAC_BUFFER_SIZE; i++) {
            output_buffer[i] = generate_sample(
                current_config.type, 
                i, 
                current_config.amplitude, 
                current_config.dc_offset
            );
        }
        
        ESP_ERROR_CHECK(dac_continuous_write_cyclically(dac_handle, 
                                                       output_buffer, 
                                                       DAC_BUFFER_SIZE, 
                                                       NULL));
        ESP_LOGI(TAG, "DAC output started");
    }
}

/**
 * @brief Stops DAC output
 */
void dac_output_stop(void) {
    if (dac_handle) {
        ESP_ERROR_CHECK(dac_continuous_disable(dac_handle));
        ESP_LOGI(TAG, "DAC output stopped");
    }
}

/**
 * @brief Checks if DAC is running
 */
bool dac_output_is_running(void) {
    return (dac_handle != NULL);
}