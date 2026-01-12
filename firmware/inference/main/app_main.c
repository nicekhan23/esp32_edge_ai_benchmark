/*
 * Signal Inference Pipeline
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
#include "data_collection.h" // For data collection functions
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
                        
                        // Parse label - CHANGED TO MATCH GENERATOR FORMAT
                        if (strstr(label_buffer, "LBL:")) {  // Was: "LABEL:"
                            char *label = label_buffer + 4;  // Was: +6 (skip "LBL:" instead of "LABEL:")
                            
                            // Clean up trailing whitespace if any
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
                                ESP_LOGI(TAG, "Received label: %s", label);
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
            // DEBUG: Log raw ADC values occasionally
            static int debug_counter = 0;
            if (debug_counter++ % 100 == 0) {
                ESP_LOGI(TAG, "Raw ADC: [0]=%d, [127]=%d, [255]=%d", 
                         raw_samples[0], raw_samples[127], raw_samples[255]);
            }
            
            // Convert to float for processing with proper scaling
            // ADC reads 12-bit (0-4095), convert to -1 to 1
            for (int i = 0; i < SAMPLE_WINDOW_SIZE; i++) {
                g_samples[i] = ((float)raw_samples[i] / 2048.0f) - 1.0f;
            }
            
            // Send to inference task
            xQueueSend(s_samples_queue, g_samples, portMAX_DELAY);
            
            // Log timing
            metrics_record_adc_time(esp_timer_get_time());
        }
        
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

// Inference task
static void inference_task(void *arg)
{
    // Initialize benchmark system
    model_benchmark_init();
    
    // Track when to run benchmarks
    static uint32_t inference_count = 0;
    static const uint32_t BENCHMARK_INTERVAL = 50; // Run benchmark every 50 inferences

    // Initialize inference engine
    inference_engine_t engine;
    inference_config_t config = {
        .mode = INFERENCE_MODE_HEURISTIC,  // Use heuristic mode for now
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
    // Add data collection modes
    bool collect_data_mode = false;  // Set via UART command
    
    while (1) {
        // Wait for new samples
        if (xQueueReceive(s_samples_queue, g_samples, portMAX_DELAY) == pdTRUE) {
            uint64_t start_time = esp_timer_get_time();
            
            // DATA COLLECTION MODE
            if (collect_data_mode && current_label) {
                data_collection_start("esp32", current_label);
                for (int i = 0; i < SAMPLE_WINDOW_SIZE; i++) {
                    data_collection_add_sample(g_samples[i]);
                }
                data_collection_finish();
                vTaskDelay(100 / portTICK_PERIOD_MS);  // Space between collections
                continue;  // Skip inference for this sample
            }
            
            // Check for new label
            char *new_label = NULL;
            if (xQueueReceive(s_labels_queue, &new_label, 0) == pdTRUE) {
                if (current_label) {
                    free(current_label);
                }
                current_label = new_label;
                ESP_LOGI(TAG, "Active ground truth label set to: %s", current_label);
            }
            
            // Preprocessing
            memcpy(g_processed_samples, g_samples, SAMPLE_WINDOW_SIZE * sizeof(float));
            preprocess_samples_fixed(g_processed_samples, SAMPLE_WINDOW_SIZE, PREPROCESS_ALL);

            // ✅ ADD BENCHMARK TRIGGER HERE
            inference_count++;
            if (inference_count % BENCHMARK_INTERVAL == 0) {
                // Run benchmark with current samples
                ESP_LOGI(TAG, "Running periodic benchmark...");
                run_benchmark_suite(g_processed_samples, SAMPLE_WINDOW_SIZE, current_label);
                
                // Get and display recommendations
                model_benchmark_t results[10];
                int num_results = model_get_benchmark_results(results, 10);
                
                for (int i = 0; i < num_results; i++) {
                    ESP_LOGI(TAG, "Model: %s, Acc: %.2f%%, Time: %uus, Flash: %uKB, RAM: %uKB",
                             results[i].name,
                             results[i].accuracy * 100,
                             results[i].inference_time_us,
                             (unsigned int)results[i].flash_size_kb,
                             (unsigned int)results[i].ram_usage_kb);
                }
                
                // Get recommendation for constraints
                model_type_t recommended = model_get_recommended(
                    512,    // Max 512KB flash
                    128,    // Max 128KB RAM
                    0.85f   // Min 85% accuracy
                );
                
                ESP_LOGI(TAG, "Recommended model: %d", (int)recommended);
            }
            
            // Perform inference
            inference_result_t result;
            if (inference_run(&engine, g_processed_samples, SAMPLE_WINDOW_SIZE, &result)) {
                uint64_t end_time = esp_timer_get_time();
                uint64_t inference_time = end_time - start_time;
                
                // Log results
                ESP_LOGI(TAG, "Inference completed in %llu us", inference_time);
                ESP_LOGI(TAG, "Predicted: %s (confidence: %.2f)", 
                         result.predicted_class, result.confidence);
                
                if (current_label) {
                    ESP_LOGI(TAG, "Ground truth: %s", current_label);
                    
                    // Calculate accuracy if we have ground truth
                    if (strcmp(result.predicted_class, current_label) == 0) {
                        metrics_record_correct_prediction();
                        ESP_LOGI(TAG, "✓ CORRECT prediction!");
                    } else {
                        metrics_record_incorrect_prediction();
                        ESP_LOGW(TAG, "✗ INCORRECT prediction");
                    }
                }
                
                // Record metrics
                metrics_record_inference_time(inference_time);
                metrics_record_memory_usage();
                
                // Update system health
                update_system_health(&s_system_health, s_samples_queue, s_labels_queue);
                check_system_state(&s_system_health);
                
                // Log statistics periodically (controlled by metrics_monitor_task)
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Signal Inference Pipeline");
    ESP_LOGI(TAG, "Expecting labels with format: LBL:<WAVEFORM>");
    ESP_LOGI(TAG, "ADC sampling at 20kHz, expecting signal on GPIO34");
    
    // Initialize metrics system
    metrics_init();
    
    // Initialize system health
    health_init(&s_system_health);
    
    // Create queues
    s_samples_queue = xQueueCreate(2, sizeof(float) * SAMPLE_WINDOW_SIZE);
    s_labels_queue = xQueueCreate(5, sizeof(char *));
    
    if (!s_samples_queue || !s_labels_queue) {
        ESP_LOGE(TAG, "Failed to create queues");
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
    
    // Create tasks
    xTaskCreate(uart_receive_task, "uart_rx", 4096, NULL, 5, NULL);
    xTaskCreate(adc_sampling_task, "adc_sampling", 4096, NULL, 6, &s_adc_task_handle);
    xTaskCreate(inference_task, "inference", 8192, NULL, 4, &s_inference_task_handle);
    
    // Create monitoring task
    xTaskCreate(metrics_monitor_task, "metrics", 4096, NULL, 3, NULL);
    
    ESP_LOGI(TAG, "System initialized. Waiting for signal and labels...");
    ESP_LOGI(TAG, "Connect Generator GPIO25 -> Inference GPIO34");
    ESP_LOGI(TAG, "Connect Generator GPIO17 -> Inference GPIO16");
    ESP_LOGI(TAG, "Connect GND to GND");
}