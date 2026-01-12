#include "adc_sampling.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "soc/soc_caps.h"

static const char *TAG = "ADC_SAMPLING";

// ADC configuration
#define ADC_UNIT                    ADC_UNIT_1
#define ADC_CHANNEL                 ADC_CHANNEL_6  // GPIO34
#define ADC_ATTEN                   ADC_ATTEN_DB_12
#define ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH
#define SAMPLE_RATE_HZ              20000
#define READ_LEN                    256

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define ADC_GET_CHANNEL(p_data)     ((p_data)->type1.channel)
#define ADC_GET_DATA(p_data)        ((p_data)->type1.data)
#else
#define ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
#define ADC_GET_DATA(p_data)        ((p_data)->type2.data)
#endif

static adc_config_t s_adc_config;
static TaskHandle_t s_conversion_task_handle = NULL;

// ADC conversion done callback
static bool IRAM_ATTR conversion_done_callback(adc_continuous_handle_t handle, 
                                               const adc_continuous_evt_data_t *edata, 
                                               void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    vTaskNotifyGiveFromISR(s_conversion_task_handle, &mustYield);
    return (mustYield == pdTRUE);
}

// Initialize ADC continuous sampling
adc_continuous_handle_t adc_sampling_init(void)
{
    esp_err_t ret;
    adc_continuous_handle_t handle = NULL;
    
    // Store task handle for callback
    s_conversion_task_handle = xTaskGetCurrentTaskHandle();
    
    // ADC handle configuration
    adc_continuous_handle_cfg_t adc_handle_cfg = {
        .max_store_buf_size = 2048,
        .conv_frame_size = READ_LEN * SOC_ADC_DIGI_RESULT_BYTES,
    };
    
    ret = adc_continuous_new_handle(&adc_handle_cfg, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC handle: %s", esp_err_to_name(ret));
        return NULL;
    }
    
    // ADC continuous mode configuration
    adc_continuous_config_t adc_cont_cfg = {
        .sample_freq_hz = SAMPLE_RATE_HZ,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_OUTPUT_TYPE,
    };
    
    // Pattern configuration for single channel
    adc_digi_pattern_config_t adc_pattern = {
        .atten = ADC_ATTEN,
        .channel = ADC_CHANNEL & 0x7,
        .unit = ADC_UNIT,
        .bit_width = ADC_BIT_WIDTH,
    };
    
    adc_cont_cfg.pattern_num = 1;
    adc_cont_cfg.adc_pattern = &adc_pattern;
    
    ret = adc_continuous_config(handle, &adc_cont_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC: %s", esp_err_to_name(ret));
        adc_continuous_deinit(handle);
        return NULL;
    }
    
    // Register event callbacks
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = conversion_done_callback,
    };
    
    ret = adc_continuous_register_event_callbacks(handle, &cbs, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register callbacks: %s", esp_err_to_name(ret));
        adc_continuous_deinit(handle);
        return NULL;
    }
    
    // Start ADC
    ret = adc_continuous_start(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start ADC: %s", esp_err_to_name(ret));
        adc_continuous_deinit(handle);
        return NULL;
    }
    
    // Store configuration
    s_adc_config.handle = handle;
    s_adc_config.channel = ADC_CHANNEL;
    s_adc_config.sample_rate_hz = SAMPLE_RATE_HZ;
    s_adc_config.adc_unit = ADC_UNIT;
    s_adc_config.adc_bit_width = ADC_BIT_WIDTH;
    
    ESP_LOGI(TAG, "ADC initialized: channel=%d, sample_rate=%d Hz", 
             ADC_CHANNEL, SAMPLE_RATE_HZ);
    
    return handle;
}

// Read samples from ADC
esp_err_t adc_sampling_read(adc_continuous_handle_t handle, 
                            int16_t *buffer, 
                            size_t buffer_size, 
                            uint32_t *samples_read)
{
    uint8_t raw_buffer[READ_LEN * SOC_ADC_DIGI_RESULT_BYTES];
    uint32_t bytes_read;
    esp_err_t ret;
    
    // Wait for conversion complete
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    
    // Read ADC data
    ret = adc_continuous_read(handle, raw_buffer, 
                              READ_LEN * SOC_ADC_DIGI_RESULT_BYTES, 
                              &bytes_read, 0);
    
    if (ret != ESP_OK) {
        if (ret != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "ADC read error: %s", esp_err_to_name(ret));
        }
        return ret;
    }
    
    // Convert raw data to samples
    size_t num_samples = bytes_read / SOC_ADC_DIGI_RESULT_BYTES;
    *samples_read = num_samples;
    
    for (int i = 0; i < num_samples && i < buffer_size; i++) {
        adc_digi_output_data_t *p = (adc_digi_output_data_t*)&raw_buffer[i * SOC_ADC_DIGI_RESULT_BYTES];
        uint32_t chan = ADC_GET_CHANNEL(p);
        uint32_t data = ADC_GET_DATA(p);
        
        // Validate channel
        if (chan < SOC_ADC_CHANNEL_NUM(ADC_UNIT)) {
            // Convert to signed 16-bit
            buffer[i] = (int16_t)data;
        } else {
            buffer[i] = 0;
        }
    }
    
    return ESP_OK;
}

// Deinitialize ADC
void adc_sampling_deinit(adc_continuous_handle_t handle)
{
    if (handle) {
        adc_continuous_stop(handle);
        adc_continuous_deinit(handle);
        ESP_LOGI(TAG, "ADC deinitialized");
    }
}

// Get current ADC configuration
void adc_sampling_get_config(adc_config_t *config)
{
    if (config) {
        memcpy(config, &s_adc_config, sizeof(adc_config_t));
    }
}