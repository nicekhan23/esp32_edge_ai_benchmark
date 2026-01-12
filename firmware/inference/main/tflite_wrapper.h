// tflite_wrapper.h
#ifndef TFLITE_WRAPPER_H
#define TFLITE_WRAPPER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run TFLite inference (C wrapper for C++ implementation)
 * 
 * @param model_data Pointer to model data
 * @param model_size Size of model data in bytes
 * @param samples Input samples
 * @param num_samples Number of samples
 * @param predicted_class Output buffer for predicted class name
 * @param class_len Length of predicted_class buffer
 * @param confidence Output confidence score
 * @return true if inference successful
 */
bool tflite_inference_cpp(void* model_data, 
                         size_t model_size, 
                         float* samples, 
                         int num_samples, 
                         char* predicted_class, 
                         size_t class_len, 
                         float* confidence);

/**
 * @brief Get required arena size for TFLite
 * 
 * @param model_data Pointer to model data
 * @param model_size Size of model data in bytes
 * @return size_t Required arena size in bytes (0 on error)
 */
size_t tflite_get_arena_size(void* model_data, size_t model_size);

#ifdef __cplusplus
}
#endif

#endif /* TFLITE_WRAPPER_H */