/**
 * @file common.h
 * @brief Common types and utilities shared across modules
 * 
 * @author Your Name
 * @date 2025 December
 * @version 1.0.0
 * 
 * @copyright (c) 2025 ESP32 Signal Processing Project
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <esp_timer.h>

/**
 * @brief Signal type enumeration (shared between acquisition and inference)
 */
typedef enum {
    SIGNAL_UNKNOWN = -1,
    SIGNAL_SINE = 0,
    SIGNAL_SQUARE = 1,
    SIGNAL_TRIANGLE = 2,
    SIGNAL_SAWTOOTH = 3,
    SIGNAL_NOISE = 4,
    SIGNAL_COUNT
} signal_type_t;

/**
 * @brief Error codes
 */
typedef enum {
    ERR_OK = 0,
    ERR_NO_MEM = -1,
    ERR_INVALID_ARG = -2,
    ERR_TIMEOUT = -3,
    ERR_NOT_INIT = -4,
    ERR_QUEUE_FULL = -5
} error_code_t;

/**
 * @brief System configuration constants
 */
#define SAMPLING_RATE_HZ             20000
#define WINDOW_SIZE                  256
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