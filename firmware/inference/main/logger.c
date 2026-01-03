/**
 * @file wifi_logger.c
 * @brief WiFi TCP server for ML dataset streaming
 * @details Based on ESP-IDF WiFi station example
 *         Provides functions to initialize WiFi, manage client connections,
 *         and stream ML dataset rows in CSV format to connected clients.
 * * @author Your Name
 * @date 2025 December
 * @version 1.0.0
 */

#include "logger.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>

static const char *TAG = "WIFI_LOGGER";

// WiFi Configuration
#define WIFI_SSID      "qurt 2.4"
#define WIFI_PASS      "dilyadarkh"
#define WIFI_MAXIMUM_RETRY  10

// TCP Server Configuration
#define TCP_PORT       3333
#define MAX_CLIENTS    2

// WiFi event group
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Client socket management
static int s_client_sockets[MAX_CLIENTS] = {-1, -1};
static SemaphoreHandle_t s_socket_mutex = NULL;
static int s_retry_num = 0;
static TaskHandle_t s_tcp_server_task = NULL;
static bool s_header_sent[MAX_CLIENTS] = {false, false};

/**
 * @brief WiFi event handler (based on ESP-IDF station example)
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi station started, connecting...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP (%d/%d)", s_retry_num, WIFI_MAXIMUM_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Failed to connect to WiFi");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief Initialize WiFi in station mode (based on ESP-IDF example)
 */
static bool wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi station initialization finished. Connecting to SSID:%s", WIFI_SSID);

    /* Wait for connection */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi SSID:%s", WIFI_SSID);
        return true;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
        return false;
    } else {
        ESP_LOGE(TAG, "Unexpected WiFi event");
        return false;
    }
}

/**
 * @brief TCP server task - accepts client connections
 */
static void tcp_server_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    int keepAlive = 1;
    int keepIdle = 5;
    int keepInterval = 5;
    int keepCount = 3;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(TCP_PORT);

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket bind failed: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Socket bound to port %d", TCP_PORT);

    err = listen(listen_sock, 2);
    if (err != 0) {
        ESP_LOGE(TAG, "Socket listen failed: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "TCP server listening on port %d", TCP_PORT);
    ESP_LOGI(TAG, "Connect with: nc <ESP32_IP> %d > dataset.csv", TCP_PORT);

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);

        ESP_LOGI(TAG, "Waiting for client connection...");
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Accept failed: errno %d", errno);
            continue;
        }

        // Set socket options
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

        // Set non-blocking mode for send operations
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        // Get client IP
        inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
        ESP_LOGI(TAG, "Client connected from %s", addr_str);

        // Find slot for client
        xSemaphoreTake(s_socket_mutex, portMAX_DELAY);
        bool slot_found = false;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (s_client_sockets[i] == -1) {
                s_client_sockets[i] = sock;
                s_header_sent[i] = false;  // Mark that header needs to be sent
                slot_found = true;
                ESP_LOGI(TAG, "Client stored in slot %d", i);
                break;
            }
        }
        xSemaphoreGive(s_socket_mutex);

        if (!slot_found) {
            ESP_LOGW(TAG, "Max clients reached, closing connection");
            close(sock);
        } else {
            // Send CSV header immediately to new client
            wifi_logger_send_header();
        }
    }

    close(listen_sock);
    vTaskDelete(NULL);
}

/**
 * @brief Initialize WiFi logger (WiFi + TCP server)
 */
bool wifi_logger_init(void)
{
    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create socket mutex
    s_socket_mutex = xSemaphoreCreateMutex();
    if (s_socket_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create socket mutex");
        return false;
    }

    // Initialize WiFi
    if (!wifi_init_sta()) {
        ESP_LOGE(TAG, "WiFi initialization failed");
        return false;
    }

    // Start TCP server task
    BaseType_t task_created = xTaskCreate(tcp_server_task, "tcp_server", 
                                          4096, NULL, 5, &s_tcp_server_task);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create TCP server task");
        return false;
    }

    return true;
}

/**
 * @brief Check if WiFi is connected
 */
bool wifi_logger_is_connected(void)
{
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

/**
 * @brief Check if any clients are connected
 */
bool wifi_logger_has_clients(void)
{
    if (!wifi_logger_is_connected()) {
        return false;
    }

    xSemaphoreTake(s_socket_mutex, portMAX_DELAY);
    bool has_client = false;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (s_client_sockets[i] != -1) {
            has_client = true;
            break;
        }
    }
    xSemaphoreGive(s_socket_mutex);

    return has_client;
}

/**
 * @brief Send data to all connected clients
 */
void wifi_logger_send_data(const char *data, size_t len)
{
    if (!wifi_logger_is_connected()) {
        return;
    }

    xSemaphoreTake(s_socket_mutex, portMAX_DELAY);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (s_client_sockets[i] != -1) {
            int sent = send(s_client_sockets[i], data, len, 0);
            if (sent < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    ESP_LOGW(TAG, "Send failed on slot %d, closing socket", i);
                    close(s_client_sockets[i]);
                    s_client_sockets[i] = -1;
                    s_header_sent[i] = false;
                }
            }
        }
    }

    xSemaphoreGive(s_socket_mutex);
}

/**
 * @brief Send CSV header to all connected clients
 */
void wifi_logger_send_header(void)
{
    char header[2048];
    int len = snprintf(header, sizeof(header),
                      "timestamp_us,window_id,label,sample_rate");

    for (int i = 0; i < 16; i++) {
        len += snprintf(header + len, sizeof(header) - len, ",feature_%d", i);
    }

    len += snprintf(header + len, sizeof(header) - len,
                   ",predicted_type,confidence");

    for (int i = 0; i < 256; i++) {
        len += snprintf(header + len, sizeof(header) - len, ",sample_%d", i);
    }

    len += snprintf(header + len, sizeof(header) - len, "\n");

    xSemaphoreTake(s_socket_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (s_client_sockets[i] != -1 && !s_header_sent[i]) {
            send(s_client_sockets[i], header, len, 0);
            s_header_sent[i] = true;
            ESP_LOGI(TAG, "CSV header sent to client slot %d", i);
        }
    }
    xSemaphoreGive(s_socket_mutex);
}

/**
 * @brief Write ML dataset row to all connected TCP clients
 */
bool wifi_logger_write(const window_buffer_t *window,
                       const feature_vector_t *features,
                       const inference_result_t *result)
{
    if (!wifi_logger_has_clients()) {
        return false;
    }

    // Build CSV row
    static char buffer[8192];  // Static to reduce stack usage
    int len = snprintf(buffer, sizeof(buffer),
                       "%llu,%lu,%d,%.2f",
                       window->timestamp_us,
                       window->window_id,
                       window->label,
                       window->sample_rate_hz);

    // Add features
    for (int i = 0; i < 16; i++) {
        len += snprintf(buffer + len, sizeof(buffer) - len, ",%.6f",
                       features->features[i]);
    }

    // Add inference result
    len += snprintf(buffer + len, sizeof(buffer) - len, ",%d,%.4f",
                   result->type, result->confidence);

    // Add raw samples
    for (int i = 0; i < 256; i++) {
        len += snprintf(buffer + len, sizeof(buffer) - len, ",%u",
                       window->samples[i]);
    }

    len += snprintf(buffer + len, sizeof(buffer) - len, "\n");

    // Send to all clients
    wifi_logger_send_data(buffer, len);

    return true;
}