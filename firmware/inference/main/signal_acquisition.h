/**
 * @file signal_acquisition.h
 * @brief Header for real-time ADC signal acquisition
 * @details Implements ML contract for window buffer format
 */

#ifndef SIGNAL_ACQUISITION_H
#define SIGNAL_ACQUISITION_H

#include "common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_adc/adc_continuous.h"

/**
 * @brief Window buffer structure - MUST match ML contract
 * 
 * @note Uses ml_input_t for samples as defined in ml_contract.h
 * @note Size is ML_WINDOW_SIZE as defined in ml_contract.h
 */
typedef struct {
    ml_input_t samples[ML_WINDOW_SIZE];  /**< Raw samples per ML contract */
    uint64_t timestamp_us;                /**< Microsecond timestamp */
    uint32_t window_id;                   /**< Sequential window ID */
    float sample_rate_hz;                 /**< Actual sampling rate */
    uint32_t sequence_number;             /**< For detecting drops */
    ml_class_t label;                     /**< Ground truth from generator */
    uint32_t checksum;                    /**< Data integrity check */
} window_buffer_t;

/**
 * @brief Acquisition statistics structure
 */
typedef struct {
    uint32_t samples_processed;
    uint32_t windows_captured;
    uint32_t buffer_overruns;
    uint32_t sampling_errors;
    uint32_t contract_violations;  /**< Count of ML contract violations */
} acquisition_stats_t;

// Function declarations
void signal_acquisition_init(void);
void signal_acquisition_init_uart(void);
QueueHandle_t signal_acquisition_get_window_queue(void);
acquisition_stats_t signal_acquisition_get_stats(void);
void signal_acquisition_update_label(ml_class_t label);  // Use ml_class_t
void signal_acquisition_start(void);
void signal_acquisition_stop(void);

/**
 * @brief Validate window buffer against ML contract
 * 
 * @param[in] window Pointer to window buffer
 * @return true if valid, false if contract violation
 */
bool signal_acquisition_validate_window(const window_buffer_t *window);

#endif /* SIGNAL_ACQUISITION_H */