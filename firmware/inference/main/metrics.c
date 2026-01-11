#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <math.h>
#include "system_monitor.h"

static const char *TAG = "METRICS";

static metrics_t s_metrics = {0};
static portMUX_TYPE s_metrics_mutex = portMUX_INITIALIZER_UNLOCKED;

void metrics_init(void)
{
    portENTER_CRITICAL(&s_metrics_mutex);
    memset(&s_metrics, 0, sizeof(metrics_t));
    s_metrics.min_inference_time_us = UINT64_MAX;
    portEXIT_CRITICAL(&s_metrics_mutex);
    
    ESP_LOGI(TAG, "Metrics system initialized");
}

void metrics_record_inference_time(uint64_t time_us)
{
    portENTER_CRITICAL(&s_metrics_mutex);
    
    s_metrics.total_inference_time_us += time_us;
    s_metrics.inference_count++;
    
    if (time_us < s_metrics.min_inference_time_us) {
        s_metrics.min_inference_time_us = time_us;
    }
    
    if (time_us > s_metrics.max_inference_time_us) {
        s_metrics.max_inference_time_us = time_us;
    }
    
    portEXIT_CRITICAL(&s_metrics_mutex);
}

void metrics_record_adc_time(uint64_t timestamp)
{
    static uint64_t last_timestamp = 0;
    
    portENTER_CRITICAL(&s_metrics_mutex);
    
    if (last_timestamp > 0) {
        uint64_t delta = timestamp - last_timestamp;
        s_metrics.total_adc_time_us += delta;
        s_metrics.adc_sample_count++;
    }
    
    last_timestamp = timestamp;
    
    portEXIT_CRITICAL(&s_metrics_mutex);
}

void metrics_record_correct_prediction(void)
{
    portENTER_CRITICAL(&s_metrics_mutex);
    s_metrics.correct_predictions++;
    s_metrics.total_predictions++;
    portEXIT_CRITICAL(&s_metrics_mutex);
}

void metrics_record_incorrect_prediction(void)
{
    portENTER_CRITICAL(&s_metrics_mutex);
    s_metrics.total_predictions++;
    portEXIT_CRITICAL(&s_metrics_mutex);
}

void metrics_record_memory_usage(void)
{
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    size_t used_heap = total_heap - free_heap;
    
    portENTER_CRITICAL(&s_metrics_mutex);
    
    s_metrics.current_heap_usage = used_heap;
    
    if (used_heap > s_metrics.peak_heap_usage) {
        s_metrics.peak_heap_usage = used_heap;
    }
    
    portEXIT_CRITICAL(&s_metrics_mutex);
}

void metrics_get_current(metrics_t *metrics)
{
    if (!metrics) return;
    
    portENTER_CRITICAL(&s_metrics_mutex);
    memcpy(metrics, &s_metrics, sizeof(metrics_t));
    portEXIT_CRITICAL(&s_metrics_mutex);
}

void metrics_log_statistics(void)
{
    metrics_t metrics;
    metrics_get_current(&metrics);
    
    if (metrics.inference_count > 0) {
        double avg_inference_time = (double)metrics.total_inference_time_us / metrics.inference_count;
        double inference_frequency = 1000000.0 / avg_inference_time;
        
        ESP_LOGI(TAG, "=== Inference Statistics ===");
        ESP_LOGI(TAG, "Total inferences: %u", metrics.inference_count);
        ESP_LOGI(TAG, "Avg inference time: %.2f us", avg_inference_time);
        ESP_LOGI(TAG, "Min inference time: %llu us", metrics.min_inference_time_us);
        ESP_LOGI(TAG, "Max inference time: %llu us", metrics.max_inference_time_us);
        ESP_LOGI(TAG, "Inference frequency: %.2f Hz", inference_frequency);
        
        if (metrics.total_predictions > 0) {
            double accuracy = 100.0 * metrics.correct_predictions / metrics.total_predictions;
            ESP_LOGI(TAG, "Accuracy: %.2f%% (%u/%u)", 
                    accuracy, metrics.correct_predictions, metrics.total_predictions);
        }
    }
    
    if (metrics.adc_sample_count > 0) {
        double avg_adc_time = (double)metrics.total_adc_time_us / metrics.adc_sample_count;
        double adc_frequency = 1000000.0 / avg_adc_time;
        
        ESP_LOGI(TAG, "=== ADC Statistics ===");
        ESP_LOGI(TAG, "Avg ADC interval: %.2f us", avg_adc_time);
        ESP_LOGI(TAG, "Effective ADC rate: %.2f Hz", adc_frequency);
    }
    
    ESP_LOGI(TAG, "=== Memory Statistics ===");
    ESP_LOGI(TAG, "Current heap usage: %.2f KB", metrics.current_heap_usage / 1024.0);
    ESP_LOGI(TAG, "Peak heap usage: %.2f KB", metrics.peak_heap_usage / 1024.0);
    
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    ESP_LOGI(TAG, "Free heap: %.2f KB", free_heap / 1024.0);
}

void metrics_reset(void)
{
    portENTER_CRITICAL(&s_metrics_mutex);
    memset(&s_metrics, 0, sizeof(metrics_t));
    s_metrics.min_inference_time_us = UINT64_MAX;
    portEXIT_CRITICAL(&s_metrics_mutex);
    
    ESP_LOGI(TAG, "Metrics reset");
}

void metrics_monitor_task(void *arg)
{
    uint32_t last_inference_count = 0;
    uint32_t last_adc_count = 0;
    
    while (1) {
        vTaskDelay(5000 / portTICK_PERIOD_MS); // Log every 5 seconds
        
        metrics_t metrics;
        metrics_get_current(&metrics);
        
        // Only log if there's new activity
        if (metrics.inference_count > last_inference_count || 
            metrics.adc_sample_count > last_adc_count) {
            metrics_log_statistics();
        }
        
        last_inference_count = metrics.inference_count;
        last_adc_count = metrics.adc_sample_count;
        
        // Record memory usage periodically
        metrics_record_memory_usage();
    }
}