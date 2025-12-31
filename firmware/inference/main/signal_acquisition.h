/**
 * @file signal_acquisition.h
 * @brief Header for real-time ADC signal acquisition and windowing system
 * @details Defines data structures, constants, and API for continuous ADC sampling,
 *          circular buffering, and window extraction for signal processing.
 * 
 * @author Your Name
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Requires FreeRTOS and ESP32 ADC continuous mode driver
 * @note Designed for inter-task communication via FreeRTOS queues
 * 
 * @copyright (c) 2025 ESP32 Signal Processing Project
 */

#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_adc/adc_continuous.h"

// Configuration
#define SAMPLING_RATE_HZ             20000  /**< ADC sampling frequency in Hertz */
#define WINDOW_SIZE                  256    /**< Number of samples per processing window */
#define WINDOW_OVERLAP               128    /**< Overlap between consecutive windows */
#define CIRCULAR_BUFFER_SIZE         1024   /**< Size of circular buffer for raw samples */

/**
 * @brief Window buffer structure containing processed signal data
 * 
 * This structure holds a complete window of ADC samples along with metadata
 * for timestamping, identification, and sample rate information.
 */
typedef struct {
    uint16_t samples[WINDOW_SIZE];    /**< Array of ADC samples */
    uint64_t timestamp_us;            /**< Microsecond timestamp */
    uint32_t window_id;               /**< Sequential window identifier */
    float sample_rate_hz;             /**< Actual sampling rate */
    uint32_t sequence_number;         /**< FIX: Sequence number for dataset integrity */
    signal_wave_t label;              /**< FIX: Ground truth label from generator */
    uint32_t checksum;                /**< FIX: Data integrity check */
} window_buffer_t;

/**
 * @brief Acquisition statistics structure
 * 
 * Tracks performance and error metrics for the signal acquisition subsystem.
 */
typedef struct {
    uint32_t samples_processed;      /**< Total number of ADC samples processed */
    uint32_t windows_captured;       /**< Total number of windows extracted */
    uint32_t buffer_overruns;        /**< Number of circular buffer overrun events */
    uint32_t sampling_errors;        /**< Number of ADC read errors */
} acquisition_stats_t;

/**
 * @brief Initialize signal acquisition subsystem
 * 
 * Configures ADC hardware, creates mutexes and queues, and sets up callback.
 * Must be called before starting acquisition.
 * 
 * @pre FreeRTOS scheduler must be running
 * @post ADC is configured but not started; queue and mutex created
 * 
 * @throws ESP_ERR_INVALID_STATE if ADC already initialized
 * @throws ERR_NO_MEM if memory allocation fails
 */
void signal_acquisition_init(void);

/**
 * @brief Get window queue handle for inter-task communication
 * 
 * Returns the FreeRTOS queue handle used to transfer window buffers
 * from the acquisition task to processing tasks.
 * 
 * @return QueueHandle_t Handle to window queue (NULL if not initialized)
 * 
 * @note Queue items are of type window_buffer_t
 * @note Queue size is fixed at 20 entries
 */
QueueHandle_t signal_acquisition_get_window_queue(void);

/**
 * @brief Get current acquisition statistics
 * 
 * Returns a thread-safe copy of acquisition statistics including
 * sample counts, window counts, and error metrics.
 * 
 * @return acquisition_stats_t Current statistics structure
 * 
 * @note Uses mutex protection during copy operation
 * @note Statistics are cumulative since system start
 */
acquisition_stats_t signal_acquisition_get_stats(void);

/**
 * @brief Start signal acquisition
 * 
 * Starts ADC continuous conversion and creates the capture task.
 * If acquisition is already running, does nothing.
 * 
 * @pre signal_acquisition_init() must be called first
 * @post ADC is sampling, capture task is running
 * 
 * @throws ESP_ERR_INVALID_STATE if ADC cannot be started
 * @throws ERR_NO_MEM if task creation fails
 */
void signal_acquisition_start(void);

/**
 * @brief Stop signal acquisition
 * 
 * Stops ADC conversion and signals capture task to terminate gracefully.
 * If acquisition is already stopped, does nothing.
 * 
 * @post ADC is stopped, capture task will self-delete
 * @note Uses flag-based graceful shutdown (non-blocking)
 */
void signal_acquisition_stop(void);