#ifndef ADC_SAMPLING_H
#define ADC_SAMPLING_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_adc/adc_continuous.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    adc_continuous_handle_t handle;
    uint8_t channel;
    uint32_t sample_rate_hz;
    uint8_t adc_unit;
    uint8_t adc_bit_width;
} adc_config_t;

/**
 * @brief Initialize ADC continuous sampling
 * 
 * @return adc_continuous_handle_t Initialized ADC handle
 */
adc_continuous_handle_t adc_sampling_init(void);

/**
 * @brief Read samples from ADC
 * 
 * @param handle ADC handle
 * @param buffer Buffer to store samples
 * @param buffer_size Size of buffer
 * @param samples_read Number of samples actually read
 * @return esp_err_t ESP_OK on success
 */
esp_err_t adc_sampling_read(adc_continuous_handle_t handle, 
                            int16_t *buffer, 
                            size_t buffer_size, 
                            uint32_t *samples_read);

/**
 * @brief Deinitialize ADC
 * 
 * @param handle ADC handle to deinitialize
 */
void adc_sampling_deinit(adc_continuous_handle_t handle);

/**
 * @brief Get current ADC configuration
 * 
 * @param config Pointer to store configuration
 */
void adc_sampling_get_config(adc_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* ADC_SAMPLING_H */