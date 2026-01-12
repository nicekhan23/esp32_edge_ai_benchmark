// tflite_wrapper.cpp
#include <cstring>
#include <cstdio>

// TensorFlow Lite includes
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_allocator.h"

extern "C" {
    #include "esp_log.h"
    #include "esp_heap_caps.h"
    #include "esp_timer.h"
}

static const char* TAG = "TFLITE_WRAPPER";

// Model includes
extern "C" {
    #include "model_config.h"

    #ifdef USE_CNN_INT8_MODEL
    #include "cnn_int8_model.h"
    #endif

    #ifdef USE_CNN_FLOAT32_MODEL
    #include "cnn_float32_model.h"
    #endif

    #ifdef USE_MLP_FLOAT32_MODEL
    #include "mlp_float32_model.h"
    #endif

    #ifdef USE_MLP_INT8_MODEL
    #include "mlp_int8_model.h"
    #endif

    #ifdef USE_HYBRID_FLOAT32_MODEL
    #include "hybrid_float32_model.h"
    #endif

    #ifdef USE_HYBRID_INT8_MODEL
    #include "hybrid_int8_model.h"
    #endif
}

// Class names - must match those in inference.c
static const char* s_class_names[] = {"SINE", "SQUARE", "TRIANGLE", "SAWTOOTH"};
#define NUM_CLASSES 4

// C++ implementation
extern "C" bool tflite_inference_cpp(void* model_data, 
                                     size_t model_size, 
                                     float* samples, 
                                     int num_samples, 
                                     char* predicted_class, 
                                     size_t class_len, 
                                     float* confidence) {
    
    if (!model_data || model_size == 0 || !samples || !predicted_class || !confidence) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }
    
    const tflite::Model* model = tflite::GetModel(model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "Model schema version mismatch");
        return false;
    }
    
    // Create op resolver
    static tflite::MicroMutableOpResolver<6> resolver;
    resolver.AddFullyConnected();
    resolver.AddSoftmax();
    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddReshape();
    resolver.AddQuantize();
    
    // Allocate arena - start with 32KB and adjust based on your model
    constexpr size_t kTensorArenaSize = 32 * 1024;
    uint8_t* arena = nullptr;
    
    // Try SPIRAM first, then internal RAM
    arena = (uint8_t*)heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!arena) {
        arena = (uint8_t*)malloc(kTensorArenaSize);
        ESP_LOGW(TAG, "Using internal RAM for TFLite arena");
    }
    
    if (!arena) {
        ESP_LOGE(TAG, "Failed to allocate %lu bytes for TFLite arena", kTensorArenaSize);
        return false;
    }
    
    // Create interpreter
    tflite::MicroInterpreter interpreter(model, resolver, arena, kTensorArenaSize);
    
    // Allocate tensors
    TfLiteStatus allocate_status = interpreter.AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        ESP_LOGE(TAG, "Failed to allocate tensors");
        free(arena);
        return false;
    }
    
    // Get input tensor
    TfLiteTensor* input = interpreter.input(0);
    if (!input) {
        ESP_LOGE(TAG, "Failed to get input tensor");
        free(arena);
        return false;
    }
    
    // Copy data to input tensor based on type
    bool input_processed = false;
    if (input->type == kTfLiteFloat32) {
        if (input->bytes >= (size_t)(num_samples * sizeof(float))) {
            float* input_data = input->data.f;
            memcpy(input_data, samples, num_samples * sizeof(float));
            input_processed = true;
        }
    } else if (input->type == kTfLiteInt8) {
        if (input->bytes >= (size_t)num_samples) {
            int8_t* input_data = input->data.int8;
            float scale = input->params.scale;
            int zero_point = input->params.zero_point;
            
            // Clamp values to int8 range
            const int32_t min_val = -128;
            const int32_t max_val = 127;
            
            for (int i = 0; i < num_samples; i++) {
                float scaled_value = samples[i] / scale + zero_point;
                int32_t quantized = static_cast<int32_t>(scaled_value);
                
                // Clamp to int8 range
                if (quantized < min_val) quantized = min_val;
                if (quantized > max_val) quantized = max_val;
                
                input_data[i] = static_cast<int8_t>(quantized);
            }
            input_processed = true;
        }
    }
    
    if (!input_processed) {
        ESP_LOGE(TAG, "Failed to process input tensor");
        free(arena);
        return false;
    }
    
    // Run inference
    uint64_t start_time = esp_timer_get_time();
    TfLiteStatus invoke_status = interpreter.Invoke();
    
    #ifdef CONFIG_DETAILED_LOGGING
    uint64_t inference_time = esp_timer_get_time() - start_time;
    ESP_LOGI(TAG, "TFLite inference time: %llu us", inference_time);
    #endif
    
    if (invoke_status != kTfLiteOk) {
        ESP_LOGE(TAG, "Failed to invoke interpreter");
        free(arena);
        return false;
    }
    
    // Get output tensor
    TfLiteTensor* output = interpreter.output(0);
    if (!output) {
        ESP_LOGE(TAG, "Failed to get output tensor");
        free(arena);
        return false;
    }
    
    // Process output
    float max_prob = 0.0f;
    int max_index = 0;
    
    if (output->type == kTfLiteFloat32) {
        float* output_data = output->data.f;
        int num_classes = output->dims->size > 1 ? output->dims->data[1] : 1;
        
        for (int i = 0; i < num_classes; i++) {
            float prob = output_data[i];
            if (prob > max_prob) {
                max_prob = prob;
                max_index = i;
            }
        }
    } else if (output->type == kTfLiteInt8) {
        int8_t* output_data = output->data.int8;
        float scale = output->params.scale;
        int zero_point = output->params.zero_point;
        int num_classes = output->dims->size > 1 ? output->dims->data[1] : 1;
        
        for (int i = 0; i < num_classes; i++) {
            float prob = (output_data[i] - zero_point) * scale;
            if (prob > max_prob) {
                max_prob = prob;
                max_index = i;
            }
        }
    } else {
        ESP_LOGE(TAG, "Unsupported output tensor type: %d", output->type);
        free(arena);
        return false;
    }
    
    // Set output values
    *confidence = max_prob;
    
    if (max_index < NUM_CLASSES) {
        strncpy(predicted_class, s_class_names[max_index], class_len - 1);
        predicted_class[class_len - 1] = '\0';
    } else {
        snprintf(predicted_class, class_len, "CLASS_%d", max_index);
    }
    
    // Clean up
    free(arena);
    
    return true;
}

extern "C" size_t tflite_get_arena_size(void* model_data, size_t model_size) {
    // This function is no longer needed with static arena size
    // For ESP32 with TFLite Micro, we use a fixed arena size
    return 32 * 1024; // 32KB default
}