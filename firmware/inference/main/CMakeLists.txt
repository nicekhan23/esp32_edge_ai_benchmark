idf_component_register(SRCS "main.c"
                            "signal_acquisition.c"
                            "inference.c"
                            "feature_extraction.c"
                            "output.c"
                            "benchmark.c"
                      PRIV_REQUIRES esp_adc
                      INCLUDE_DIRS "."
                      REQUIRES esp_timer freertos nvs_flash esp_system driver)