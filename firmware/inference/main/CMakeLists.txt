idf_component_register(SRCS "app_main.c"
                              "adc_sampling.c"
                              "preprocessing.c"
                              "inference.c"
                              "metrics.c"
                              "signal_validation.c"
                              "clock_sync.c"
                              "system_health.c"
                              "data_collection.c"
                              "benchmark.c"
                              "../arrays/cnn_float32_model.c"
                              "../arrays/cnn_int8_model.c"
                              "../arrays/mlp_float32_model.c"
                              "../arrays/mlp_int8_model.c"
                              "../arrays/hybrid_float32_model.c"
                              "../arrays/hybrid_int8_model.c"
                       INCLUDE_DIRS "." "../arrays"
                       INCLUDE_DIRS "."
                       REQUIRES freertos esp_adc driver esp_timer esp_common esp_system)

if(CONFIG_TFLITE_ENABLE)
    idf_component_register(SRCS "inference.c"
                           REQUIRES esp-tflite-micro)
endif()