/**
 * @file signal_acquisition.c
 * @brief ADC-based real-time signal acquisition and windowing system
 * @details Implements continuous ADC sampling, circular buffering, and window extraction 
 *          for real-time signal processing. Designed for ESP32 with FreeRTOS.
 * 
 * @author Your Name
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Uses ESP32 ADC continuous mode and FreeRTOS queues for inter-task communication
 * @note Thread-safe statistics protected by mutex
 * 
 * @copyright (c) 2025 ESP32 Signal Processing Project
 */

#include "signal_acquisition.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

// Private constants
#define ADC_UNIT                    ADC_UNIT_1           /**< ADC unit to use (1 or 2) */
#define ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1 /**< ADC conversion mode */
#define ADC_ATTEN                   ADC_ATTEN_DB_12     /**< ADC attenuation level (12dB) */
#define ADC_BIT_WIDTH               ADC_BITWIDTH_12     /**< ADC resolution (12-bit) */
#define CAPTURE_CHANNEL             ADC_CHANNEL_2       /**< ADC channel for signal input */
#define ADC_FRAME_BYTES             512                 /**< Bytes per ADC read frame */

// Global variables
static const char *TAG = "SIGNAL_ACQ";                  /**< Logging tag for module */
static QueueHandle_t s_window_queue = NULL;             /**< Queue for extracted windows */
static TaskHandle_t s_capture_task_handle = NULL;       /**< Handle for capture task */
static SemaphoreHandle_t s_stats_mutex = NULL;          /**< Mutex for statistics protection */
static adc_continuous_handle_t s_adc_handle = NULL;     /**< ADC continuous mode handle */
static volatile bool s_capture_running = false;         /**< Capture state flag */
static signal_wave_t s_current_label = SIGNAL_WAVE_SINE;  /**< Current ground truth label from generator */

// Circular buffer
static uint16_t s_circular_buffer[CIRCULAR_BUFFER_SIZE]; /**< Circular buffer for raw samples */
static uint32_t s_circular_buffer_write_idx = 0;         /**< Write index for circular buffer */
static uint32_t s_circular_buffer_read_idx = 0;          /**< Read index for circular buffer */
static uint32_t s_window_counter = 0;                    /**< Sequential window ID counter */

// Statistics
static acquisition_stats_t s_stats = {0};                /**< Acquisition statistics */

// Private function prototypes
static void capture_task(void *arg);
static void extract_window(void);
static bool IRAM_ATTR adc_conversion_callback(adc_continuous_handle_t handle, 
                                              const adc_continuous_evt_data_t *edata, 
                                              void *user_data);
static void continuous_adc_init(void);

/**
 * @brief ADC conversion complete callback (ISR context)
 * 
 * This interrupt service routine is called when an ADC conversion completes.
 * It notifies the capture task to process the new data.
 * 
 * @param[in] handle ADC continuous handle (unused)
 * @param[in] edata Event data (unused)
 * @param[in] user_data User context data (unused)
 * 
 * @return bool Returns true if a context switch is required
 * 
 * @note This function must be in IRAM for interrupt handling
 * @note Uses task notifications for minimal ISR overhead
 */
static bool IRAM_ATTR adc_conversion_callback(adc_continuous_handle_t handle, 
                                              const adc_continuous_evt_data_t *edata, 
                                              void *user_data) {
    BaseType_t mustYield = pdFALSE;
    if (s_capture_task_handle) {
        vTaskNotifyGiveFromISR(s_capture_task_handle, &mustYield);
    }
    return (mustYield == pdTRUE);
}

/**
 * @brief Initialize continuous ADC hardware and driver
 * 
 * Configures ADC for continuous sampling mode with specified parameters.
 * Sets up sampling frequency, conversion mode, and channel pattern.
 * 
 * @pre None (initializes hardware)
 * @post ADC is configured but not started
 * 
 * @throws ESP_ERR_INVALID_ARG if configuration invalid
 * @throws ESP_ERR_NO_MEM if insufficient memory for ADC driver
 * 
 * @note Uses SOC_ADC_DIGI_RESULT_BYTES for buffer sizing
 * @note Channel mask is limited to 3 bits (0-7) for pattern config
 */
static void continuous_adc_init(void) {
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = CIRCULAR_BUFFER_SIZE * SOC_ADC_DIGI_RESULT_BYTES,
        .conv_frame_size = ADC_FRAME_BYTES,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &s_adc_handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = SAMPLING_RATE_HZ,
        .conv_mode = ADC_CONV_MODE,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };

    adc_digi_pattern_config_t adc_pattern = {
        .atten = ADC_ATTEN,
        .channel = CAPTURE_CHANNEL & 0x7,
        .unit = ADC_UNIT,
        .bit_width = ADC_BIT_WIDTH,
    };
    
    dig_cfg.pattern_num = 1;
    dig_cfg.adc_pattern = &adc_pattern;

    ESP_ERROR_CHECK(adc_continuous_config(s_adc_handle, &dig_cfg));
}
/**
 * @brief Update current ground truth label
 * 
 * Updates the current signal label used for window metadata.
 * This function is thread-safe and can be called from any context.
 * 
 * @param[in] label New ground truth label from signal generator
 * 
 * @post s_current_label is updated
 */
void signal_acquisition_update_label(signal_wave_t label) {
    s_current_label = label;
    ESP_LOGI(TAG, "Label updated to: %d", label);
}

/**
 * @brief Extract overlapping window from circular buffer
 * 
 * Extracts WINDOW_SIZE samples from the circular buffer with WINDOW_OVERLAP
 * advancement. If insufficient samples are available, returns immediately.
 * 
 * @pre Statistics mutex must be available (handled internally)
 * @post Read index advanced by WINDOW_OVERLAP
 * @post Window buffer enqueued for processing if successful
 * 
 * @note Window extraction is non-blocking (queue sends with 0 timeout)
 * @note Updates acquisition statistics on success/failure
 * @note Uses timestamp from esp_timer_get_time() for window time reference
 */
static void extract_window(void) {
    static uint16_t window[WINDOW_SIZE];
    
    // Check if we have enough samples
    uint32_t available_samples;
    if (s_circular_buffer_write_idx >= s_circular_buffer_read_idx) {
        available_samples = s_circular_buffer_write_idx - s_circular_buffer_read_idx;
    } else {
        available_samples = CIRCULAR_BUFFER_SIZE - s_circular_buffer_read_idx + 
                           s_circular_buffer_write_idx;
    }

    if (available_samples < WINDOW_SIZE) {
        return;
    }

    xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
    if (s_stats.buffer_overruns > 0) {
        // Reset read index after overrun to maintain synchronization
        s_circular_buffer_read_idx = 
            (s_circular_buffer_write_idx - WINDOW_SIZE + CIRCULAR_BUFFER_SIZE) 
            % CIRCULAR_BUFFER_SIZE;
    }
    xSemaphoreGive(s_stats_mutex);

    // Extract window
    for (int i = 0; i < WINDOW_SIZE; i++) {
        window[i] = s_circular_buffer[(s_circular_buffer_read_idx + i) % CIRCULAR_BUFFER_SIZE];
    }

    // Update read index
    s_circular_buffer_read_idx = (s_circular_buffer_read_idx + WINDOW_OVERLAP) % CIRCULAR_BUFFER_SIZE;

    // Prepare window buffer
    window_buffer_t window_buf = {
        .timestamp_us = esp_timer_get_time(),
        .window_id = s_window_counter++,
        .sample_rate_hz = SAMPLING_RATE_HZ,
        .sequence_number = s_window_counter,  // FIX: Sequence tracking
        .label = s_current_label,             // FIX: Ground truth label
        .checksum = 0                         // FIX: Will calculate below
    };
    memcpy(window_buf.samples, window, sizeof(window));

    // Calculate simple checksum for data integrity
    uint32_t checksum = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        checksum += window[i];
    }
    window_buf.checksum = checksum;

    // Send to queue
    if (s_window_queue && s_capture_running) {
        if (xQueueSend(s_window_queue, &window_buf, 0) != pdTRUE) {
            xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
            s_stats.buffer_overruns++;
            xSemaphoreGive(s_stats_mutex);
        } else {
            xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
            s_stats.windows_captured++;
            s_stats.samples_processed += WINDOW_SIZE;
            xSemaphoreGive(s_stats_mutex);
        }
    }
}

/**
 * @brief Main ADC capture task
 * 
 * FreeRTOS task that handles ADC data acquisition, circular buffer management,
 * and window extraction. Waits for ADC conversion notifications via task notify.
 * 
 * @param[in] arg Task argument (unused)
 * 
 * @note Allocates raw_buffer dynamically for ADC frames
 * @note Includes 1ms delay per loop iteration for watchdog compliance
 * @note Self-terminates when s_capture_running becomes false
 */
static void capture_task(void *arg) {
    uint8_t *raw_buffer = malloc(ADC_FRAME_BYTES);
    uint32_t bytes_read = 0;
    
    ESP_LOGI(TAG, "Capture task started");
    
    while (s_capture_running) {
        // Wait for ADC conversion notification
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        if (!s_capture_running) {
            break;
        }
        
        esp_err_t ret = adc_continuous_read(s_adc_handle, raw_buffer, 
                                           ADC_FRAME_BYTES, &bytes_read, 0);

        
        if (ret == ESP_OK && bytes_read > 0) {
            for (int i = 0; i < bytes_read; i += SOC_ADC_DIGI_RESULT_BYTES) {
                adc_digi_output_data_t *p = (adc_digi_output_data_t*)&raw_buffer[i];
                uint32_t data = p->type1.data;
                
                // Store in circular buffer
                s_circular_buffer[s_circular_buffer_write_idx] = (uint16_t)data;
                s_circular_buffer_write_idx = (s_circular_buffer_write_idx + 1) % CIRCULAR_BUFFER_SIZE;
                
                // Check for buffer overrun
                if (s_circular_buffer_write_idx == s_circular_buffer_read_idx) {
                    xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
                    s_stats.buffer_overruns++;
                    xSemaphoreGive(s_stats_mutex);
                    s_circular_buffer_read_idx = (s_circular_buffer_read_idx + WINDOW_OVERLAP) % CIRCULAR_BUFFER_SIZE;
                }
                
                // Try to extract window
                extract_window();
            }
        } else if (ret != ESP_OK) {
            xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
            s_stats.sampling_errors++;
            xSemaphoreGive(s_stats_mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    free(raw_buffer);
    vTaskDelete(NULL);
}

/**
 * @brief Initialize signal acquisition subsystem
 * 
 * Initializes mutex, creates window queue, configures ADC hardware,
 * and registers ADC callback. Must be called before starting acquisition.
 * 
 * @pre FreeRTOS scheduler should be running
 * @post ADC is configured, queue and mutex are created
 * 
 * @throws ESP_ERR_INVALID_STATE if ADC already initialized
 * @throws ERR_NO_MEM if queue or mutex creation fails
 * 
 * @note Window queue size is fixed at 20 entries
 * @note Callback is registered without user context data
 */
void signal_acquisition_init(void) {
    s_stats_mutex = xSemaphoreCreateMutex();
    s_window_queue = xQueueCreate(20, sizeof(window_buffer_t));
    
    continuous_adc_init();
    
    // Register ADC callback
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = adc_conversion_callback,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(s_adc_handle, &cbs, NULL));
    
    ESP_LOGI(TAG, "Signal acquisition initialized");
}

/**
 * @brief Get the window queue handle
 * 
 * Returns the FreeRTOS queue handle used for window data transfer.
 * Other tasks can receive windows from this queue.
 * 
 * @return QueueHandle_t Handle to window queue (NULL if not initialized)
 */
QueueHandle_t signal_acquisition_get_window_queue(void) {
    return s_window_queue;
}

/**
 * @brief Get current acquisition statistics
 * 
 * Returns a thread-safe copy of the acquisition statistics structure.
 * 
 * @return acquisition_stats_t Current statistics (samples, windows, errors, etc.)
 * 
 * @note Uses mutex protection during copy
 * @note Statistics are reset only by system restart
 */
acquisition_stats_t signal_acquisition_get_stats(void) {
    acquisition_stats_t stats;
    xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
    stats = s_stats;
    xSemaphoreGive(s_stats_mutex);
    return stats;
}

/**
 * @brief Start signal acquisition
 * 
 * Starts ADC continuous conversion and creates the capture task.
 * If already running, does nothing.
 * 
 * @pre signal_acquisition_init() must be called first
 * @post ADC is sampling, capture task is running
 * 
 * @throws ESP_ERR_INVALID_STATE if ADC cannot be started
 * @throws ERR_NO_MEM if task creation fails
 */
void signal_acquisition_start(void) {
    if (!s_capture_running) {
        s_capture_running = true;
        ESP_ERROR_CHECK(adc_continuous_start(s_adc_handle));
        
        // FIX: Create a proper FreeRTOS task instead of calling capture_task directly
        xTaskCreate(
            capture_task,
            "capture_task",
            8192,  // Increased stack size for safety
            NULL,
            5,  // Priority
            &s_capture_task_handle
        );
        ESP_LOGI(TAG, "Signal acquisition started");
    }
}

/**
 * @brief Stop signal acquisition
 * 
 * Stops ADC conversion and signals capture task to terminate.
 * If already stopped, does nothing.
 * 
 * @post ADC is stopped, capture task will self-delete
 * @note Task termination is asynchronous (uses flag-based graceful shutdown)
 */
void signal_acquisition_stop(void) {
    if (s_capture_running) {
        s_capture_running = false;
        ESP_ERROR_CHECK(adc_continuous_stop(s_adc_handle));
        // Task will self-terminate
        ESP_LOGI(TAG, "Signal acquisition stopped");
    }
}