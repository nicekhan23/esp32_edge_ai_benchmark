/**
 * @file inference.h
 * @brief Header for signal classification inference engine
 * @details Defines signal types, inference results, and API for ML-based
 *          signal classification using feature vectors.
 * 
 * @author Your Name
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Currently implements rule-based classifier; placeholder for TensorFlow Lite
 * @note Supports both neural network and heuristic classification
 * 
 * @copyright (c) 2025 ESP32 Signal Processing Project
 */

#pragma once

#include "feature_extraction.h"
#include "common.h"

/**
 * @brief Inference result structure
 * 
 * Contains the classification result with confidence score,
 * timing information, and window reference.
 */
typedef struct {
    signal_type_t type;            /**< Classified signal type */
    float confidence;              /**< Confidence score (0.0 to 1.0) */
    uint64_t inference_time_us;    /**< Inference execution time in microseconds */
    uint32_t window_id;            /**< Reference to source window ID */
} inference_result_t;

/**
 * @brief Initialize inference engine
 * 
 * Allocates memory for model (TensorFlow Lite) and initializes
 * classification structures. Must be called before running inference.
 * 
 * @throws ERR_NO_MEM if tensor arena allocation fails
 * 
 * @note Currently only allocates memory; model loading is TODO
 */
void inference_init(void);

/**
 * @brief Run inference on feature vector
 * 
 * Classifies signal type from extracted features using either
 * neural network model or rule-based heuristics.
 * 
 * @param[in] features Pointer to feature vector structure
 * @return inference_result_t Classification result with metadata
 * 
 * @note Updates internal statistics automatically
 * @note Execution time is measured and included in result
 */
inference_result_t run_inference(const feature_vector_t *features);

/**
 * @brief Convert signal type enum to human-readable string
 * 
 * @param[in] type Signal type enumeration value
 * @return const char* Corresponding string representation
 * 
 * @note Returns "UNKNOWN" for invalid type values
 */
const char* signal_type_to_string(signal_type_t type);

/**
 * @brief Cleanup inference engine resources
 * 
 * Frees allocated memory and releases model resources.
 * Should be called before system shutdown.
 */
void inference_cleanup(void);

/**
 * @brief Inference statistics structure
 * 
 * Tracks performance and accuracy metrics for the inference subsystem.
 */
typedef struct {
    uint32_t total_inferences;               /**< Total number of inferences performed */
    uint32_t per_type_counts[SIGNAL_COUNT];  /**< Counts per signal type */
    float avg_inference_time_us;             /**< Average inference execution time */
    float accuracy;                          /**< Classification accuracy (0.0 to 1.0) */
} inference_stats_t;

/**
 * @brief Get current inference statistics
 * 
 * Returns cumulative statistics including counts, timing, and accuracy.
 * 
 * @return inference_stats_t Current inference statistics
 * 
 * @note Accuracy calculation requires ground truth data (currently placeholder)
 */
inference_stats_t get_inference_stats(void);