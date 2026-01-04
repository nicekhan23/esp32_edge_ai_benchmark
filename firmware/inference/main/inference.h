/**
 * @file inference.h
 * @brief Header for signal classification inference engine
 * @details Consumes ML contract via window_buffer_t
 * 
 * @author Darkhan Zhanibekuly
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Uses ml_class_t and ml_output_t from ml_contract.h
 * @note run_inference() expects window_buffer_t format per contract
 */

#ifndef INFERENCE_H
#define INFERENCE_H

#include "signal_acquisition.h"  // Includes window_buffer_t with ML contract

/**
 * @brief Initialize inference engine
 * 
 * @throws ERR_NO_MEM if tensor arena allocation fails
 */
void inference_init(void);

/**
 * @brief Run inference on window buffer (ML contract entry point)
 * 
 * This is the main ML inference function that consumes data
 * in the exact format defined by the ML contract.
 * 
 * @param[in] window Pointer to window buffer (must match ML contract)
 * @return ml_output_t Classification result with metadata
 * 
 * @note Updates internal statistics automatically
 * @note Will validate window against contract before processing
 */
ml_output_t run_inference(const window_buffer_t *window);

/**
 * @brief Convert ML class to human-readable string
 * 
 * @param[in] class ML class enumeration value
 * @return const char* Corresponding string representation
 * 
 * @note Uses ml_class_to_string() from ml_contract.h
 */
const char* inference_class_to_string(ml_class_t class);

/**
 * @brief Cleanup inference engine resources
 */
void inference_cleanup(void);

/**
 * @brief Inference statistics structure
 */
typedef struct {
    uint32_t total_inferences;               /**< Total inferences performed */
    uint32_t per_class_counts[ML_CLASS_COUNT]; /**< Counts per ML class */
    float avg_inference_time_us;             /**< Average inference time */
    float accuracy;                          /**< Classification accuracy */
    uint32_t contract_violations;            /**< Count of contract violations */
} inference_stats_t;

/**
 * @brief Get current inference statistics
 * 
 * @return inference_stats_t Current inference statistics
 */
inference_stats_t get_inference_stats(void);

#endif /* INFERENCE_H */