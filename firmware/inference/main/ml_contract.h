/**
 * @file ml_contract.h
 * @brief Machine Learning I/O Contract - Single Source of Truth
 * @details Defines the exact interface between data acquisition, preprocessing,
 *          and inference. This contract MUST be respected by all modules.
 * 
 * @author Darkhan Zhanibekuly
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Changing this contract requires retraining the ML model
 * @note All timing and synchronization depends on these constants
 */

#pragma once

#include <stdint.h>

// ===== ML INPUT CONTRACT =====

/**
 * @brief Number of samples per inference window
 * 
 * This is the fundamental unit of ML inference. Changing this requires
 * retraining the model and updating preprocessing.
 */
#define ML_WINDOW_SIZE        256

/**
 * @brief Type of raw input samples
 * 
 * Raw ADC readings directly from hardware. Must match ADC configuration.
 */
typedef uint16_t ml_input_t;

/**
 * @brief ADC dynamic range
 * 
 * 12-bit ADC range (0-4095). Important for normalization.
 */
#define ML_ADC_MIN            0
#define ML_ADC_MAX            4095

/**
 * @brief Sample ordering specification
 * 
 * Temporal ordering of samples within the window:
 * - samples[0] = oldest sample (t - (ML_WINDOW_SIZE-1) * Δt)
 * - samples[ML_WINDOW_SIZE-1] = newest sample (t)
 * 
 * Where Δt = 1 / SAMPLING_RATE_HZ
 */

// ===== ML OUTPUT CONTRACT =====

/**
 * @brief ML classification output classes
 * 
 * Enumeration of signal types the ML model can identify.
 * The integer values must match training labels.
 */
typedef enum {
    ML_CLASS_SINE = 0,      /**< Sine wave */
    ML_CLASS_SQUARE = 1,    /**< Square wave */
    ML_CLASS_TRIANGLE = 2,  /**< Triangle wave */
    ML_CLASS_SAWTOOTH = 3,  /**< Sawtooth wave */
    ML_CLASS_NOISE = 4,     /**< Noise or unknown signal */
    ML_CLASS_COUNT          /**< Total number of classes */
} ml_class_t;

/**
 * @brief ML inference result structure
 * 
 * Contains classification result with confidence score.
 */
typedef struct {
    ml_class_t predicted_class;  /**< Class predicted by ML model */
    float confidence;            /**< Confidence score (0.0 to 1.0) */
    uint64_t inference_time_us;  /**< Inference execution time */
    uint32_t window_id;          /**< Source window ID for correlation */
} ml_output_t;

// ===== VALIDATION MACROS =====

/**
 * @brief Validate ADC sample is within expected range
 */
#define ML_VALIDATE_ADC_SAMPLE(sample) \
    ((sample) >= ML_ADC_MIN && (sample) <= ML_ADC_MAX)

/**
 * @brief Validate ML class is within valid range
 */
#define ML_VALIDATE_CLASS(class) \
    ((class) >= 0 && (class) < ML_CLASS_COUNT)

/**
 * @brief Convert ML class to string for logging
 */
static inline const char* ml_class_to_string(ml_class_t class) {
    static const char* strings[] = {
        "SINE", "SQUARE", "TRIANGLE", "SAWTOOTH", "NOISE"
    };
    return (class >= 0 && class < ML_CLASS_COUNT) ? strings[class] : "INVALID";
}