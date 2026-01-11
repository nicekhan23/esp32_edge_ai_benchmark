set(srcs "app_main.c"
        
        
         )

idf_component_register(SRCS "waveform_tables.c" "uart_labels.c" "dac_output.c" "${srcs}"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_dac esp_driver_gpio esp_timer driver)
