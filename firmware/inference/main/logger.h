/**
 * @file logger.h
 * @brief WiFi TCP server for ML dataset streaming
 * @details Provides functions to initialize WiFi, manage client connections,
 *          and stream ML dataset rows in CSV format to connected clients.
 * @author Your Name
 * @date 2025 December
 * @version 1.0.0
 * @note Designed for real-time data streaming over WiFi
 * @note Thread-safe for multi-task environments
 */

#pragma once

#include <stdbool.h>
#include "signal_acquisition.h"
#include "feature_extraction.h"
#include "inference.h"

/**
 * @brief Initialize WiFi and TCP server
 * @return true if successful, false otherwise
 */
bool wifi_logger_init(void);

/**
 * @brief Check if any clients are connected
 * @return true if at least one client connected
 */
bool wifi_logger_has_clients(void);

/**
 * @brief Send CSV header to all connected clients
 */
void wifi_logger_send_header(void);

/**
 * @brief Write ML dataset row to all connected TCP clients
 * @param[in] window Pointer to window buffer
 * @param[in] features Pointer to feature vector
 * @param[in] result Pointer to inference result
 * @return true if data sent successfully
 */
bool wifi_logger_write(const window_buffer_t *window, 
                       const feature_vector_t *features,
                       const inference_result_t *result);

/**
 * @brief Get WiFi connection status
 * @return true if WiFi connected
 */
bool wifi_logger_is_connected(void);

/**
 * @brief Send raw data to all clients
 * @param[in] data Data buffer to send
 * @param[in] len Length of data
 */
void wifi_logger_send_data(const char *data, size_t len);