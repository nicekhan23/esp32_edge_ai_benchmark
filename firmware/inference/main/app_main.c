/*
 * Signal Inference Pipeline - Thesis Implementation
 * Acquisition and inference subsystem for ESP32
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "adc_sampling.h"
#include "preprocessing.h"
#include "inference.h"
#include "system_monitor.h"
#include "data_collection.h"
#include "signal_processing.h"
#include "benchmark.h"

static const char *TAG = "SIGNAL_INFERENCE";

// Signal buffer
#define SAMPLE_WINDOW_SIZE 256
static float g_samples[SAMPLE_WINDOW_SIZE];
static float g_processed_samples[SAMPLE_WINDOW_SIZE];

// UART configuration for receiving labels
#define UART_PORT_NUM      UART_NUM_1
#define UART_BAUD_RATE     115200
#define UART_RX_PIN        16
#define UART_TX_PIN        17
#define UART_BUF_SIZE      1024

// Task handles
static TaskHandle_t s_adc_task_handle = NULL;
static TaskHandle_t s_inference_task_handle = NULL;

// Queues for inter-task communication
static QueueHandle_t s_samples_queue = NULL;
static QueueHandle_t s_labels_queue = NULL;

// System health monitoring
static system_health_t s_system_health;

// UART label reception task
static void uart_receive_task(void *arg)
{
    uint8_t data[UART_BUF_SIZE];
    char label_buffer[64];
    int label_index = 0;
    
    while (1) {
        int len = uart_read_bytes(UART_PORT_NUM, data, UART_BUF_SIZE - 1, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            data[len] = 0;
            
            for (int i = 0; i < len; i++) {
                if (data[i] == '\n' || data[i] == '\r') {
                    if (label_index > 0) {
                        label_buffer[label_index] = 0;
                        
                        // Parse label
                        if (strstr(label_buffer, "LBL:")) {
                            char *label = label_buffer + 4;
                            
                            // Clean up trailing whitespace
                            char *end = label + strlen(label) - 1;
                            while (end > label && (*end == '\n' || *end == '\r' || *end == ' ')) {
                                *end = 0;
                                end--;
                            }
                            
                            // Send to inference task
                            char *queue_label = malloc(strlen(label) + 1);
                            if (queue_label) {
                                strcpy(queue_label, label);
                                xQueueSend(s_labels_queue, &queue_label, portMAX_DELAY);
                            }
                        }
                        label_index = 0;
                    }
                } else if (label_index < sizeof(label_buffer) - 1) {
                    label_buffer[label_index++] = data[i];
                }
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// ADC sampling task
static void adc_sampling_task(void *arg)
{
    adc_continuous_handle_t handle = adc_sampling_init();
    
    while (1) {
        int16_t raw_samples[SAMPLE_WINDOW_SIZE];
        uint32_t sample_count;
        
        // Acquire samples
        esp_err_t ret = adc_sampling_read(handle, raw_samples, SAMPLE_WINDOW_SIZE, &sample_count);
        
        if (ret == ESP_OK && sample_count == SAMPLE_WINDOW_SIZE) {
            // Convert to float for processing with proper scaling
            for (int i = 0; i < SAMPLE_WINDOW_SIZE; i++) {
                g_samples[i] = ((float)raw_samples[i] / 2048.0f) - 1.0f;
            }
            
            // Send to inference task
            xQueueSend(s_samples_queue, g_samples, portMAX_DELAY);
            
            // Record timing metrics
            metrics_record_adc_time(esp_timer_get_time());
        }
        
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

// Determine model type based on Kconfig selection
static model_type_t get_selected_model_type(void)
{
    // Match the same logic as in inference.c
    #ifdef CONFIG_MODEL_CNN_INT8
        ESP_LOGI(TAG, "Selected model: CNN_INT8");
        return MODEL_CNN_INT8;
    #elif defined(CONFIG_MODEL_CNN_FLOAT32)
        ESP_LOGI(TAG, "Selected model: CNN_FLOAT32");
        return MODEL_CNN_FLOAT32;
    #elif defined(CONFIG_MODEL_MLP_FLOAT32)
        ESP_LOGI(TAG, "Selected model: MLP_FLOAT32");
        return MODEL_MLP_FLOAT32;
    #elif defined(CONFIG_MODEL_MLP_INT8)
        ESP_LOGI(TAG, "Selected model: MLP_INT8");
        return MODEL_MLP_INT8;
    #elif defined(CONFIG_MODEL_HYBRID_FLOAT32)
        ESP_LOGI(TAG, "Selected model: HYBRID_FLOAT32");
        return MODEL_HYBRID_FLOAT32;
    #elif defined(CONFIG_MODEL_HYBRID_INT8)
        ESP_LOGI(TAG, "Selected model: HYBRID_INT8");
        return MODEL_HYBRID_INT8;
    #elif defined(CONFIG_MODEL_HEURISTIC_ONLY)
        ESP_LOGI(TAG, "Selected model: HEURISTIC_ONLY");
        return MODEL_NONE;
    #else
        ESP_LOGW(TAG, "No model selected in Kconfig, defaulting to heuristic");
        return MODEL_NONE;
    #endif
}

// Determine inference mode based on Kconfig
static inference_mode_t get_inference_mode(void)
{
    // Check if any TFLite model is enabled
    #if defined(CONFIG_MODEL_CNN_INT8) || defined(CONFIG_MODEL_CNN_FLOAT32) || \
        defined(CONFIG_MODEL_MLP_FLOAT32) || defined(CONFIG_MODEL_MLP_INT8) || \
        defined(CONFIG_MODEL_HYBRID_FLOAT32) || defined(CONFIG_MODEL_HYBRID_INT8)
        ESP_LOGI(TAG, "Using TFLite inference mode");
        return INFERENCE_MODE_TFLITE;
    #else
        ESP_LOGI(TAG, "Using heuristic inference mode");
        return INFERENCE_MODE_HEURISTIC;
    #endif
}

// Inference task
static void inference_task(void *arg)
{
    // Initialize benchmark system
    model_benchmark_init();
    
    // Track when to run benchmarks
    static uint32_t inference_count = 0;
    static const uint32_t BENCHMARK_INTERVAL = 50;

    // Initialize inference engine based on Kconfig selection
    inference_engine_t engine;
    inference_config_t config = {
        .mode = get_inference_mode(),
        .model_type = get_selected_model_type(),
        .confidence_threshold = 0.5f,
        .voting_window = 3,
        .enable_voting = false,
        .enable_fft = true
    };
    
    if (!inference_init(&engine, &config)) {
        ESP_LOGE(TAG, "Failed to initialize inference engine");
        vTaskDelete(NULL);
    }
    
    // Buffer for current label
    char *current_label = NULL;
    
    while (1) {
        // Wait for new samples
        if (xQueueReceive(s_samples_queue, g_samples, portMAX_DELAY) == pdTRUE) {
            uint64_t start_time = esp_timer_get_time();
            
            // Check for new label
            char *new_label = NULL;
            if (xQueueReceive(s_labels_queue, &new_label, 0) == pdTRUE) {
                if (current_label) {
                    free(current_label);
                }
                current_label = new_label;
            }
            
            // Preprocessing
            memcpy(g_processed_samples, g_samples, SAMPLE_WINDOW_SIZE * sizeof(float));
            preprocess_samples_fixed(g_processed_samples, SAMPLE_WINDOW_SIZE, PREPROCESS_ALL);

            // Periodic benchmark execution
            inference_count++;
            if (inference_count % BENCHMARK_INTERVAL == 0) {
                run_benchmark_suite(g_processed_samples, SAMPLE_WINDOW_SIZE, current_label);
            }
            
            // Perform inference
            inference_result_t result;
            if (inference_run(&engine, g_processed_samples, SAMPLE_WINDOW_SIZE, &result)) {
                uint64_t end_time = esp_timer_get_time();
                uint64_t inference_time = end_time - start_time;
                
                // Log inference results
                ESP_LOGI(TAG, "Inference: %s (%.2f) in %llu us", 
                         result.predicted_class, result.confidence, inference_time);
                
                if (current_label) {
                    // Calculate accuracy
                    if (strcmp(result.predicted_class, current_label) == 0) {
                        metrics_record_correct_prediction();
                    } else {
                        metrics_record_incorrect_prediction();
                    }
                }
                
                // Record metrics
                metrics_record_inference_time(inference_time);
                
                // Update system health
                update_system_health(&s_system_health, s_samples_queue, s_labels_queue);
                check_system_state(&s_system_health);
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Signal Inference Pipeline - Thesis Implementation");
    ESP_LOGI(TAG, "Sampling Rate: 20kHz, Window Size: %d samples", SAMPLE_WINDOW_SIZE);
    
    // Log selected model configuration
    model_type_t selected_model = get_selected_model_type();
    inference_mode_t inference_mode = get_inference_mode();
    
    ESP_LOGI(TAG, "Inference Mode: %d, Model Type: %d", inference_mode, selected_model);
    
    // Initialize metrics system
    metrics_init();
    
    // Initialize system health
    health_init(&s_system_health);
    
    // Create queues
    s_samples_queue = xQueueCreate(2, sizeof(float) * SAMPLE_WINDOW_SIZE);
    s_labels_queue = xQueueCreate(5, sizeof(char *));
    
    if (!s_samples_queue || !s_labels_queue) {
        ESP_LOGE(TAG, "Failed to create communication queues");
        return;
    }
    
    // Initialize UART for label reception
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, 
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 
                                        UART_BUF_SIZE * 2, 0, NULL, 0));
    
    // Create tasks with proper priorities
    xTaskCreate(uart_receive_task, "uart_rx", 4096, NULL, 5, NULL);
    xTaskCreate(adc_sampling_task, "adc_sampling", 4096, NULL, 6, &s_adc_task_handle);
    xTaskCreate(inference_task, "inference", 12288, NULL, 4, &s_inference_task_handle);
    
    // Create monitoring task
    xTaskCreate(metrics_monitor_task, "metrics", 4096, NULL, 3, NULL);
    
    ESP_LOGI(TAG, "System initialized and ready");
}