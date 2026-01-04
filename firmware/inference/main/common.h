/**
 * @file common.h
 * @brief Common types and utilities shared across modules
 * 
 * @author Darkhan Zhanibekuly
 * @date 2025 December
 * @version 1.0.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <esp_timer.h>
#include "ml_contract.h"  // Include ML contract FIRST

/**
 * @brief Legacy signal type (deprecated - use ml_class_t from contract)
 * @deprecated Use ml_class_t from ml_contract.h instead
 */
typedef ml_class_t signal_type_t;

// Maintain backward compatibility for now
#define SIGNAL_UNKNOWN    ML_CLASS_NOISE
#define SIGNAL_SINE       ML_CLASS_SINE
#define SIGNAL_SQUARE     ML_CLASS_SQUARE
#define SIGNAL_TRIANGLE   ML_CLASS_TRIANGLE
#define SIGNAL_SAWTOOTH   ML_CLASS_SAWTOOTH
#define SIGNAL_NOISE      ML_CLASS_NOISE
#define SIGNAL_COUNT      ML_CLASS_COUNT

/**
 * @brief Error codes
 */
typedef enum {
    ERR_OK = 0,
    ERR_NO_MEM = -1,
    ERR_INVALID_ARG = -2,
    ERR_TIMEOUT = -3,
    ERR_NOT_INIT = -4,
    ERR_QUEUE_FULL = -5,
    ERR_CONTRACT_VIOLATION = -6  // New: ML contract violation
} error_code_t;

/**
 * @brief System configuration constants
 * 
 * @note SAMPLING_RATE_HZ and ML_WINDOW_SIZE are part of the ML contract
 * @note Changing these requires model retraining
 */
#define SAMPLING_RATE_HZ             20000
#define WINDOW_SIZE                  ML_WINDOW_SIZE  // Use contract definition
#define WINDOW_OVERLAP               128
#define CIRCULAR_BUFFER_SIZE         1024
#define FEATURE_VECTOR_SIZE          16

/**
 * @brief Logging macros with module tags
 */
#define LOG_ERROR(tag, fmt, ...)     ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...)      ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define LOG_INFO(tag, fmt, ...)      ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(tag, fmt, ...)     ESP_LOGD(tag, fmt, ##__VA_ARGS__)

/**
 * @brief Timing utilities
 */
static inline uint64_t get_time_us(void) {
    return esp_timer_get_time();
}

/**
 * @brief Math utilities
 */
#define DEG2RAD(deg) ((deg) * M_PI / 180.0f)
#define RAD2DEG(rad) ((rad) * 180.0f / M_PI)
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

/**
 * @brief Circular buffer index increment
 */
#define CIRCULAR_INCREMENT(idx, size) ((idx) = ((idx) + 1) % (size))