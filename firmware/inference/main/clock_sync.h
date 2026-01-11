#ifndef CLOCK_SYNC_H
#define CLOCK_SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// UART packet structure (must match generator)
typedef struct __attribute__((packed)) {
    uint8_t sync_byte;     // 0xAA
    uint8_t packet_type;
    uint16_t sequence;
    uint32_t timestamp_ms;
    uint8_t payload_length;
    uint8_t payload[32];
    uint8_t crc8;
} uart_packet_t;

typedef enum {
    PKT_TYPE_LABEL = 0x01,
    PKT_TYPE_TIMESTAMP = 0x02,
    PKT_TYPE_HEARTBEAT = 0x03,
    PKT_TYPE_ACK = 0x04
} uart_packet_type_t;

typedef struct {
    uint32_t remote_timestamp;
    uint32_t local_timestamp;
    int32_t offset_ms;
    float drift_ppm;
    bool synchronized;
    uint32_t sync_count;
} clock_sync_t;

/**
 * @brief Initialize clock synchronization
 * 
 * @param sync Clock sync structure
 */
void sync_init(clock_sync_t *sync);

/**
 * @brief Process incoming packet for synchronization
 * 
 * @param sync Clock sync structure
 * @param packet Received packet
 */
void sync_process_packet(clock_sync_t *sync, uart_packet_t *packet);

/**
 * @brief Get synchronized timestamp
 * 
 * @param sync Clock sync structure
 * @return uint32_t Synchronized timestamp in ms
 */
uint32_t get_synchronized_timestamp(clock_sync_t *sync);

/**
 * @brief Check if clock is synchronized
 * 
 * @param sync Clock sync structure
 * @return true if synchronized
 */
bool is_clock_synchronized(clock_sync_t *sync);

/**
 * @brief Calculate CRC8 for packet verification
 * 
 * @param data Data buffer
 * @param length Data length
 * @return uint8_t CRC8 value
 */
uint8_t calculate_crc8(const uint8_t *data, size_t length);

/**
 * @brief Send ACK packet
 * 
 * @param sequence Sequence number to acknowledge
 */
void uart_send_ack(uint16_t sequence);

#ifdef __cplusplus
}
#endif

#endif /* CLOCK_SYNC_H */