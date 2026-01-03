idf_component_register(SRCS "logger.c" "main.c"
                            "signal_acquisition.c"
                            "inference.c"
                            "feature_extraction.c"
                            "output.c"
                            "benchmark.c"
                            "logger.c"
                      PRIV_REQUIRES esp_adc
                      INCLUDE_DIRS "."
                      REQUIRES esp_timer freertos nvs_flash esp_system driver esp_wifi lwip esp_event log)