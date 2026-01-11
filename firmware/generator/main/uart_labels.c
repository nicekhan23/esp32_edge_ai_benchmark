#include "uart_labels.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "uart_labels";
static uint16_t s_sequence_number = 0;
static bool s_uart_initialized = false;

// CRC8 table for calculation
static const uint8_t crc8_table[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31,
    0x24, 0x23, 0x2A, 0x2D, 0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D, 0xE0, 0xE7, 0xEE, 0xE9,
    0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1,
    0xB4, 0xB3, 0xBA, 0xBD, 0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA, 0xB7, 0xB0, 0xB9, 0xBE,
    0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16,
    0x03, 0x04, 0x0D, 0x0A, 0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A, 0x89, 0x8E, 0x87, 0x80,
    0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8,
    0xDD, 0xDA, 0xD3, 0xD4, 0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44, 0x19, 0x1E, 0x17, 0x10,
    0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F,
    0x6A, 0x6D, 0x64, 0x63, 0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13, 0xAE, 0xA9, 0xA0, 0xA7,
    0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF,
    0xFA, 0xFD, 0xF4, 0xF3
};

uint8_t calculate_crc8(const uint8_t *data, size_t length) {
    uint8_t crc = 0;
    for (size_t i = 0; i < length; i++) {
        crc = crc8_table[crc ^ data[i]];
    }
    return crc;
}

void uart_labels_init(void) {
    const uart_config_t uart_config = {
        .baud_rate = LABEL_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(LABEL_UART_PORT, &uart_config));
    
    // Set UART pins
    ESP_ERROR_CHECK(uart_set_pin(LABEL_UART_PORT, 
                                 LABEL_UART_TX_PIN, 
                                 LABEL_UART_RX_PIN, 
                                 UART_PIN_NO_CHANGE, 
                                 UART_PIN_NO_CHANGE));
    
    // Install UART driver with RX buffer for ACKs
    ESP_ERROR_CHECK(uart_driver_install(LABEL_UART_PORT, 
                                        512,  // RX buffer size for ACKs
                                        256,  // TX buffer size
                                        0,    // Queue size
                                        NULL, // Queue handle
                                        0));  // Interrupt allocation flags
    
    s_uart_initialized = true;
    ESP_LOGI(TAG, "UART labels initialized on GPIO %d", LABEL_UART_TX_PIN);
}

bool uart_wait_for_ack(uint16_t sequence, uint32_t timeout_ms) {
    uint8_t buffer[sizeof(uart_packet_t)];
    uint32_t start_time = esp_timer_get_time() / 1000;
    
    while ((esp_timer_get_time() / 1000 - start_time) < timeout_ms) {
        int len = uart_read_bytes(LABEL_UART_PORT, buffer, sizeof(uart_packet_t), 10 / portTICK_PERIOD_MS);
        
        if (len == sizeof(uart_packet_t)) {
            uart_packet_t *packet = (uart_packet_t *)buffer;
            
            // Verify packet
            if (packet->sync_byte == 0xAA && 
                packet->packet_type == PKT_TYPE_ACK &&
                packet->crc8 == calculate_crc8(buffer, sizeof(uart_packet_t) - 1)) {
                
                // Check if this is the ACK for our sequence
                if (packet->sequence == sequence) {
                    ESP_LOGD(TAG, "Received ACK for sequence %d", sequence);
                    return true;
                }
            }
        }
    }
    
    return false;
}

void uart_send_packet(uart_packet_t *packet) {
    if (!s_uart_initialized || !packet) return;
    
    // Calculate CRC
    packet->crc8 = calculate_crc8((uint8_t*)packet, sizeof(uart_packet_t) - 1);
    
    // Send packet
    int bytes_written = uart_write_bytes(LABEL_UART_PORT, (const char*)packet, sizeof(uart_packet_t));
    
    if (bytes_written > 0) {
        ESP_LOGD(TAG, "Sent packet type %d, seq %d", packet->packet_type, packet->sequence);
    } else {
        ESP_LOGE(TAG, "Failed to send packet");
    }
}

void uart_send_label(waveform_type_t type) {
    if (!s_uart_initialized) return;
    
    const char* labels[] = {"SINE", "SQUARE", "TRIANGLE", "SAWTOOTH"};
    const char* label = (type < WAVEFORM_COUNT) ? labels[type] : "UNKNOWN";
    
    uart_packet_t packet = {
        .sync_byte = 0xAA,
        .packet_type = PKT_TYPE_LABEL,
        .sequence = s_sequence_number++,
        .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000),
        .payload_length = strlen(label)
    };
    
    memcpy(packet.payload, label, packet.payload_length);
    uart_send_packet(&packet);
    
    ESP_LOGI(TAG, "Sent label: %s", label);
}

bool uart_send_label_with_ack(waveform_type_t type) {
    if (!s_uart_initialized) return false;
    
    const char* labels[] = {"SINE", "SQUARE", "TRIANGLE", "SAWTOOTH"};
    const char* label = (type < WAVEFORM_COUNT) ? labels[type] : "UNKNOWN";
    
    uart_packet_t packet = {
        .sync_byte = 0xAA,
        .packet_type = PKT_TYPE_LABEL,
        .sequence = s_sequence_number,
        .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000),
        .payload_length = strlen(label)
    };
    
    memcpy(packet.payload, label, packet.payload_length);
    
    for (int retry = 0; retry < 3; retry++) {
        uart_send_packet(&packet);
        
        // Wait for ACK with 100ms timeout
        if (uart_wait_for_ack(packet.sequence, 100)) {
            s_sequence_number++;
            return true;
        }
        
        ESP_LOGW(TAG, "Retry %d for packet %d", retry+1, packet.sequence);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    
    return false;
}

void uart_send_heartbeat(void) {
    if (!s_uart_initialized) return;
    
    uart_packet_t packet = {
        .sync_byte = 0xAA,
        .packet_type = PKT_TYPE_HEARTBEAT,
        .sequence = s_sequence_number++,
        .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000),
        .payload_length = 0
    };
    
    uart_send_packet(&packet);
}