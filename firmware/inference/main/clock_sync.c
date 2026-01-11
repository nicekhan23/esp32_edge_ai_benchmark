#include "clock_sync.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "CLOCK_SYNC";

// CRC8 table
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

void sync_init(clock_sync_t *sync) {
    if (!sync) return;
    
    memset(sync, 0, sizeof(clock_sync_t));
    sync->offset_ms = 0;
    sync->drift_ppm = 0.0f;
    sync->synchronized = false;
    sync->sync_count = 0;
    
    ESP_LOGI(TAG, "Clock synchronization initialized");
}

void sync_process_packet(clock_sync_t *sync, uart_packet_t *packet) {
    if (!sync || !packet) return;
    
    uint32_t local_receive_time = (uint32_t)(esp_timer_get_time() / 1000);
    uint32_t remote_send_time = packet->timestamp_ms;
    
    // Simple offset calculation (assumes symmetric latency)
    int32_t new_offset = (int32_t)(local_receive_time - remote_send_time);
    
    ESP_LOGD(TAG, "New offset: %d ms (local=%u, remote=%u)", 
             new_offset, local_receive_time, remote_send_time);
    
    // Update with moving average if we already have a previous value
    if (sync->sync_count > 0) {
        // Simple low-pass filter for offset
        sync->offset_ms = (sync->offset_ms * 7 + new_offset) / 8;
        
        // Calculate drift properly
        static uint32_t last_local_time = 0;
        static int32_t last_offset = 0;
        
        if (last_local_time > 0) {
            uint32_t time_change = local_receive_time - last_local_time;
            int32_t offset_change = new_offset - last_offset;
            
            if (time_change > 1000) {  // Only calculate drift after significant time
                sync->drift_ppm = (float)offset_change * 1000000.0f / (float)time_change;
                ESP_LOGD(TAG, "Drift: %.1f ppm (offset_change=%d, time_change=%u)", 
                         sync->drift_ppm, offset_change, time_change);
            }
        }
        
        last_local_time = local_receive_time;
        last_offset = new_offset;
    } else {
        sync->offset_ms = new_offset;
    }
    
    sync->remote_timestamp = remote_send_time;
    sync->local_timestamp = local_receive_time;
    sync->sync_count++;
    
    // Consider synchronized if offset is reasonable
    if (abs(sync->offset_ms) < 100) {  // Reasonable threshold
        if (!sync->synchronized) {
            sync->synchronized = true;
            ESP_LOGI(TAG, "Clock synchronized! Offset: %d ms, Drift: %.1f ppm", 
                     sync->offset_ms, sync->drift_ppm);
        }
    } else {
        if (sync->synchronized) {
            sync->synchronized = false;
            ESP_LOGW(TAG, "Clock lost synchronization! Offset: %d ms", sync->offset_ms);
        }
    }
    
    ESP_LOGD(TAG, "Clock sync: local=%u, remote=%u, offset=%d, count=%u, synced=%s",
             local_receive_time, remote_send_time, sync->offset_ms, sync->sync_count,
             sync->synchronized ? "yes" : "no");
}

uint32_t get_synchronized_timestamp(clock_sync_t *sync) {
    uint32_t local = (uint32_t)(esp_timer_get_time() / 1000);
    
    if (sync && sync->synchronized) {
        // Apply offset correction
        return local - sync->offset_ms;
    }
    
    return local;
}

bool is_clock_synchronized(clock_sync_t *sync) {
    return sync ? sync->synchronized : false;
}

uint8_t calculate_crc8(const uint8_t *data, size_t length) {
    uint8_t crc = 0;
    for (size_t i = 0; i < length; i++) {
        crc = crc8_table[crc ^ data[i]];
    }
    return crc;
}

void uart_send_ack(uint16_t sequence) {
    uart_packet_t packet = {
        .sync_byte = 0xAA,
        .packet_type = PKT_TYPE_ACK,
        .sequence = sequence,
        .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000),
        .payload_length = 0
    };
    
    packet.crc8 = calculate_crc8((uint8_t*)&packet, sizeof(packet) - 1);
    
    // Send via UART (assuming UART_NUM_1)
    uart_write_bytes(UART_NUM_1, (const char*)&packet, sizeof(packet));
    
    ESP_LOGD(TAG, "Sent ACK for sequence %d", sequence);
}