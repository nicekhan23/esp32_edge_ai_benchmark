/**
 * @file control.c
 * @brief Command Line Interface Control Module
 * @details This module provides a REPL (Read-Eval-Print Loop) command interface
 * for controlling the signal generator. It implements command parsing, validation,
 * and dispatch using the ESP console framework.
 * 
 * @author Darkhan Zhanibekuly
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note Uses argtable3 for command-line argument parsing
 * @note Implements a UART-based console interface at 115200 baud
 * 
 * @copyright (c) 2025 ESP32 Signal Generator Project
 */

#include "signal_gen.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "control";    /**< Logging tag for control module */

/* ================== Command Handler Functions ================== */

/**
 * @brief Handle the 'start' command
 * 
 * Starts signal generation by calling signal_gen_start().
 * 
 * @param[in] argc Number of command arguments (unused)
 * @param[in] argv Array of argument strings (unused)
 * @return Always returns 0 (success)
 * 
 * @note This is a console command handler
 */
static int cmd_start_handler(int argc, char **argv)
{
    signal_gen_start();
    return 0;
}

/**
 * @brief Handle the 'stop' command
 * 
 * Stops signal generation by calling signal_gen_stop().
 * 
 * @param[in] argc Number of command arguments (unused)
 * @param[in] argv Array of argument strings (unused)
 * @return Always returns 0 (success)
 */
static int cmd_stop_handler(int argc, char **argv)
{
    signal_gen_stop();
    return 0;
}

/**
 * @brief Handle the 'config' command
 * 
 * Configures signal generator parameters. Accepts waveform type and frequency
 * as optional arguments. If arguments are omitted, current values are preserved.
 * 
 * @param[in] argc Number of command arguments
 * @param[in] argv Array of argument strings
 * @return 0 on success, 1 on parsing error
 * 
 * @note Uses argtable3 for robust argument parsing
 * @note Only modifies specified parameters (partial updates allowed)
 * 
 * Usage: config [<wave>] [<freq>]
 *   wave: 0=sine, 1=square, 2=triangle, 3=sawtooth
 *   freq: Frequency in Hertz (1-10000 recommended)
 */
static int cmd_config_handler(int argc, char **argv)
{
    signal_gen_config_t cfg = *signal_gen_get_config();
    
    // Parse arguments using argtable
    struct arg_int *wave = arg_int0(NULL, NULL, "<wave>", "Waveform type (0-3)");
    struct arg_int *freq = arg_int0(NULL, NULL, "<freq>", "Frequency in Hz");
    struct arg_end *end = arg_end(20);
    
    void *argtable[] = {wave, freq, end};
    
    // Parse arguments
    int nerrors = arg_parse(argc, argv, argtable);
    
    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        printf("Usage: %s <wave> <freq>\n", argv[0]);
        printf("  wave: 0=sine, 1=square, 2=triangle, 3=sawtooth\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return 1;
    }
    
    if (wave->count > 0) {
        cfg.wave = wave->ival[0];
    }
    
    if (freq->count > 0) {
        cfg.frequency_hz = freq->ival[0];
    }
    
    signal_gen_set_config(&cfg);
    printf("Configuration updated:\n");
    signal_gen_emit_label();
    
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 0;
}

/**
 * @brief Handle the 'status' command
 * 
 * Displays current signal generator configuration in human-readable format.
 * 
 * @param[in] argc Number of command arguments (unused)
 * @param[in] argv Array of argument strings (unused)
 * @return Always returns 0 (success)
 */
static int cmd_status_handler(int argc, char **argv)
{
    printf("Current configuration:\n");
    signal_gen_emit_label();
    return 0;
}

/**
 * @brief Handle the 'help' command
 * 
 * Displays available commands and their usage information.
 * 
 * @param[in] argc Number of command arguments (unused)
 * @param[in] argv Array of argument strings (unused)
 * @return Always returns 0 (success)
 * 
 * @note Provides detailed help for all registered commands
 */
static int cmd_help_handler(int argc, char **argv)
{
    printf("\nAvailable commands:\n");
    printf("  start                     - Start signal generation\n");
    printf("  stop                      - Stop signal generation\n");
    printf("  config <wave> <freq>      - Configure signal\n");
    printf("  status                    - Show current configuration\n");
    printf("  help                      - Show this help\n");
    printf("\n");
    return 0;
}

/* ================== Module Initialization ================== */

/**
 * @brief Initialize the control interface
 * 
 * Sets up the console REPL environment, registers all commands, and starts
 * the command processor. This function runs indefinitely, handling user input
 * via UART.
 * 
 * @pre UART driver should be initialized by ESP-IDF
 * @pre signal_gen_init() should be called before this function
 * 
 * @throws ESP_ERR_NO_MEM if insufficient memory for console
 * @throws ESP_ERR_INVALID_STATE if console already initialized
 * 
 * @note Default UART configuration: 115200 baud, 8N1
 * @note Command history is supported (up/down arrows)
 * @note Tab completion is available for commands
 */
void control_init(void)
{
    ESP_LOGI(TAG, "Initializing control interface");
    
    // Initialize console REPL (Read-Eval-Print Loop)
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "siggen> ";
    repl_config.max_cmdline_length = 256;
    
    // Initialize UART for console communication
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    
    // ========== Command Registration ==========
    
    /**
     * @brief 'start' command descriptor
     */
    esp_console_cmd_t start_cmd = {
        .command = "start",
        .help = "Start signal generation",
        .hint = NULL,
        .func = &cmd_start_handler,
        .argtable = NULL,
    };
    
    /**
     * @brief 'stop' command descriptor
     */
    esp_console_cmd_t stop_cmd = {
        .command = "stop",
        .help = "Stop signal generation",
        .hint = NULL,
        .func = &cmd_stop_handler,
        .argtable = NULL,
    };
    
    /**
     * @brief 'config' command descriptor
     */
    esp_console_cmd_t config_cmd = {
        .command = "config",
        .help = "Configure signal generator (config <wave> <freq>)",
        .hint = NULL,
        .func = &cmd_config_handler,
        .argtable = NULL,
    };
    
    /**
     * @brief 'status' command descriptor
     */
    esp_console_cmd_t status_cmd = {
        .command = "status",
        .help = "Show current configuration",
        .hint = NULL,
        .func = &cmd_status_handler,
        .argtable = NULL,
    };
    
    /**
     * @brief 'help' command descriptor
     */
    esp_console_cmd_t help_cmd = {
        .command = "help",
        .help = "Show help message",
        .hint = NULL,
        .func = &cmd_help_handler,
        .argtable = NULL,
    };
    
    // Register all commands with the console
    ESP_ERROR_CHECK(esp_console_cmd_register(&start_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&stop_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&config_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&status_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&help_cmd));
    
    // Print welcome message and command summary
    printf("\n========================================\n");
    printf("        ESP32 Signal Generator\n");
    printf("========================================\n");
    printf("Commands:\n");
    printf("  start                     - Start signal generation\n");
    printf("  stop                      - Stop signal generation\n");
    printf("  config <wave> <freq>      - Configure signal\n");
    printf("  wave: 0=sine, 1=square, 2=triangle, 3=sawtooth\n");
    printf("  status                    - Show current configuration\n");
    printf("  help                      - Show this help\n");
    printf("========================================\n\n");
    
    // Start the REPL (this function does not return)
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}