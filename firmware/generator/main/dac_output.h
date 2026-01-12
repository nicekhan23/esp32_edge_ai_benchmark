#ifndef DAC_OUTPUT_H
#define DAC_OUTPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/dac_continuous.h"

// Define the available waveforms for the system
typedef enum {
    WAVEFORM_SINE,
    WAVEFORM_SQUARE,
    WAVEFORM_TRIANGLE,
    WAVEFORM_SAWTOOTH,
    WAVEFORM_COUNT
} waveform_type_t;

// Waveform configuration structure
typedef struct {
    waveform_type_t type;
    float amplitude;        // 0.0 to 1.0
    float dc_offset;        // -0.5 to 0.5
    uint32_t frequency_hz;  // Not used in current implementation but available
} waveform_config_t;

// Configuration
#define DAC_CONVERT_FREQ_HZ     20000    // 20 kHz sample rate
#define DAC_BUFFER_SIZE         257      // PRIME number to avoid periodicity
#define DEFAULT_AMPLITUDE       1.0f
#define DEFAULT_DC_OFFSET       0.0f

// Function Prototypes
void dac_output_init(void);
void dac_output_set_waveform(waveform_type_t type);
void dac_output_set_waveform_config(waveform_config_t *config);
void dac_output_start(void);
void dac_output_stop(void);
bool dac_output_is_running(void);

#endif