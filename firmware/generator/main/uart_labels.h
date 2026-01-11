#ifndef UART_LABELS_H
#define UART_LABELS_H

#include "dac_output.h" // To share the waveform_type_t enum
#include "driver/gpio.h" // For GPIO_NUM_xx definitions

// Hardware Configuration
#define LABEL_UART_PORT      UART_NUM_1
#define LABEL_UART_TX_PIN    GPIO_NUM_17
#define LABEL_UART_RX_PIN    UART_PIN_NO_CHANGE // Not receiving on this node
#define LABEL_BAUD_RATE      115200

// UART Protocol Definitions
typedef enum {
    PKT_TYPE_LABEL = 0x01,
    PKT_TYPE_TIMESTAMP = 0x02,
    PKT_TYPE_HEARTBEAT = 0x03,
    PKT_TYPE_ACK = 0x04,
    PKT_TYPE_WAVEFORM_CONFIG = 0x05
} uart_packet_type_t;

typedef struct __attribute__((packed)) {
    uint8_t sync_byte;     // 0xAA
    uint8_t packet_type;
    uint16_t sequence;
    uint32_t timestamp_ms;
    uint8_t payload_length;
    uint8_t payload[32];
    uint8_t crc8;
} uart_packet_t;

// Calculate CRC8 for packet
uint8_t calculate_crc8(const uint8_t *data, size_t length);

// Prototypes
void uart_labels_init(void);
void uart_send_label(waveform_type_t type);
bool uart_send_label_with_ack(waveform_type_t type);
void uart_send_heartbeat(void);
bool uart_wait_for_ack(uint16_t sequence, uint32_t timeout_ms);

#endif