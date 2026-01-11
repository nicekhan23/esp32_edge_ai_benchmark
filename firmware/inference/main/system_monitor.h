#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// System states
typedef enum {
    SYSTEM_STATE_NORMAL,
    SYSTEM_STATE_DEGRADED,
    SYSTEM_STATE_CRITICAL,
    SYSTEM_STATE_FAILED
} system_state_t;

// Compact health structure
typedef struct {
    uint32_t state : 2;           // 2 bits for 4 states
    uint32_t uart_connected : 1;  // 1 bit for boolean
    uint32_t queue_utilization : 7; // 0-127%
    uint32_t task_count : 6;      // Up to 63 tasks
    uint32_t health_counter : 16; // 0-65535
    
    size_t free_heap;
    size_t min_free_heap;
    uint32_t inference_time_avg;
    float recent_accuracy;
} system_health_t;

// Metrics structure
typedef struct {
    uint64_t total_inference_time_us;
    uint64_t min_inference_time_us;
    uint64_t max_inference_time_us;
    uint32_t inference_count;
    
    uint64_t total_adc_time_us;
    uint32_t adc_sample_count;
    
    uint32_t correct_predictions;
    uint32_t total_predictions;
    
    size_t peak_heap_usage;
    size_t current_heap_usage;
} metrics_t;

/**
 * @brief Initialize system monitoring
 */
void system_monitor_init(void);

/**
 * @brief Initialize health monitoring
 */
void health_init(system_health_t *health);

/**
 * @brief Update system health status
 */
void update_system_health(system_health_t *health, 
                          QueueHandle_t samples_queue, 
                          QueueHandle_t labels_queue);

/**
 * @brief Check current system state
 */
system_state_t check_system_state(system_health_t *health);

/**
 * @brief Record inference time
 */
void metrics_record_inference_time(uint64_t time_us);

/**
 * @brief Record ADC sampling time
 */
void metrics_record_adc_time(uint64_t timestamp);

/**
 * @brief Record correct prediction
 */
void metrics_record_correct_prediction(void);

/**
 * @brief Record incorrect prediction
 */
void metrics_record_incorrect_prediction(void);

/**
 * @brief Record memory usage
 */
void metrics_record_memory_usage(void);

/**
 * @brief Get current metrics
 */
void metrics_get_current(metrics_t *metrics);

/**
 * @brief Log current statistics
 */
void metrics_log_statistics(void);

/**
 * @brief Metrics monitoring task
 */
void metrics_monitor_task(void *arg);

void metrics_init(void);
void metrics_record_inference_time(uint64_t time_us);
void metrics_record_correct_prediction(void);
void metrics_record_incorrect_prediction(void);
void metrics_record_memory_usage(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_MONITOR_H */