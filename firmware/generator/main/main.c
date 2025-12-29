/**
 * @file main.c
 * @brief ESP32 Signal Generator Main Application
 * @details This is the entry point for the ESP32 signal generator application.
 * It initializes all system components and starts the command interface.
 * 
 * @mainpage ESP32 Signal Generator
 * 
 * @section intro_sec Introduction
 * The ESP32 Signal Generator is a flexible waveform generation tool that
 * produces analog signals via the ESP32's built-in DAC. It supports four
 * waveform types with configurable amplitude, frequency, noise, and DC offset.
 * 
 * @section features_sec Features
 * - Four waveform types: Sine, Square, Triangle, Sawtooth
 * - Configurable frequency (1Hz - 10kHz)
 * - Adjustable amplitude and DC offset
 * - Additive Gaussian noise
 * - Command-line interface via UART
 * - Real-time waveform updates
 * 
 * @section hardware_sec Hardware Requirements
 * - ESP32 Development Board
 * - DAC Channel 0 (GPIO25) for output
 * - UART for command interface (115200 baud)
 * 
 * @section usage_sec Usage
 * 1. Connect to ESP32 via serial terminal (115200 baud)
 * 2. Use commands: start, stop, config, status, help
 * 3. Connect oscilloscope to GPIO25 for signal output
 * 
 * @section api_sec API Documentation
 * See signal_gen.h for the public API documentation.
 * 
 * @author Darkhan Zhanibekuly
 * @date 2025 December
 * @version 1.0.0
 * 
 * @copyright (c) 2025 December ESP32 Signal Generator Project
 */

#include "signal_gen.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Forward declaration */
void control_init(void);

/**
 * @brief Application entry point
 * 
 * This function is called by the ESP-IDF framework after system initialization.
 * It performs the following tasks:
 * 1. Initializes the signal generator hardware
 * 2. Starts the command-line control interface
 * 3. Optionally auto-starts signal generation
 * 
 * @return This function does not return (infinite loop in control_init)
 * 
 * @note System logging is available via ESP_LOGI after this function starts
 * @note FreeRTOS scheduler starts before this function is called
 * 
 * @par Execution Flow:
 * @dot
 * digraph main_flow {
 *   rankdir=TB;
 *   node [shape=box];
 *   
 *   app_main -> "vTaskDelay(100ms)" -> signal_gen_init -> control_init;
 *   signal_gen_init -> "DAC Hardware Init" -> "Waveform Buffer Init";
 *   control_init -> "UART Console Init" -> "Command Registration" -> "REPL Start";
 *   "REPL Start" -> "Command Processing Loop";
 * }
 * @enddot
 */
void app_main(void)
{
    ESP_LOGI("app_main", "ESP32 Signal Generator");
    
    // Small delay to ensure system is stable
    vTaskDelay(pdMS_TO_TICKS(100));

    // Initialize signal generator hardware
    signal_gen_init();
    
    // Start command-line interface (does not return)
    control_init();

    /* optional: auto-start signal generation at boot */
    // signal_gen_start();
}