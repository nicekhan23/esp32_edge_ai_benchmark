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
                       INCLUDE_DIRS "."
                       REQUIRES freertos esp_adc driver esp_timer esp_common esp_system)

if(CONFIG_TFLITE_ENABLE)
    idf_component_register(SRCS "inference.c"
                           REQUIRES esp-tflite-micro)
endif()