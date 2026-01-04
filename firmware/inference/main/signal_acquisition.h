/**
 * @file signal_acquisition.h
 * @brief Header for real-time ADC signal acquisition
 */

#ifndef SIGNAL_ACQUISITION_H
#define SIGNAL_ACQUISITION_H

#include "common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_adc/adc_continuous.h"

/**
 * @brief Window buffer structure
 */
typedef struct {
    uint16_t samples[WINDOW_SIZE];
    uint64_t timestamp_us;
    uint32_t window_id;
    float sample_rate_hz;
    uint32_t sequence_number;
    signal_type_t label;            // Using common signal_type_t
    uint32_t checksum;
} window_buffer_t;

/**
 * @brief Acquisition statistics structure
 */
typedef struct {
    uint32_t samples_processed;
    uint32_t windows_captured;
    uint32_t buffer_overruns;
    uint32_t sampling_errors;
} acquisition_stats_t;

// Function declarations
void signal_acquisition_init(void);
void signal_acquisition_init_uart(void);
QueueHandle_t signal_acquisition_get_window_queue(void);
acquisition_stats_t signal_acquisition_get_stats(void);
void signal_acquisition_update_label(signal_type_t label);  // Using common type
void signal_acquisition_start(void);
void signal_acquisition_stop(void);

#endif /* SIGNAL_ACQUISITION_H */