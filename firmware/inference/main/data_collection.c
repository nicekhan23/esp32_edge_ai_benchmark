// data_collection.c - Optimized with binary format
#include "data_collection.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "DATA_COLLECT";
static FILE *s_data_file = NULL;
static bool s_collecting = false;
static uint32_t s_sample_count = 0;
static char s_current_label[32] = {0};

// Binary format header
typedef struct __attribute__((packed)) {
    uint64_t timestamp;
    uint8_t source_id;
    uint8_t label_len;
    uint16_t sample_count;
    // Followed by: label string (null-terminated), then samples
} data_header_t;

void data_collection_init(void) {
    // Open file for binary writing
    s_data_file = fopen("/sdcard/waveform_data.bin", "wb");
    if (s_data_file) {
        ESP_LOGI(TAG, "Binary data collection initialized");
    }
}

void data_collection_start(const char *source, const char *label) {
    if (!s_data_file || s_collecting) return;
    
    s_collecting = true;
    s_sample_count = 0;
    
    // Store label for later
    strncpy(s_current_label, label, sizeof(s_current_label) - 1);
    
    #ifdef CONFIG_DETAILED_LOGGING
    ESP_LOGI(TAG, "Starting collection: source=%s, label=%s", source, label);
    #endif
}

void data_collection_add_sample(float sample) {
    if (!s_collecting || !s_data_file) return;
    
    // Buffer sample for later writing
    static float sample_buffer[256];
    if (s_sample_count < 256) {
        sample_buffer[s_sample_count] = sample;
        s_sample_count++;
    }
    
    // Write when buffer is full
    if (s_sample_count >= 256) {
        data_collection_finish_binary(sample_buffer, s_sample_count);
    }
}

// Optimized binary write
void data_collection_finish_binary(float *samples, int count) {
    if (!s_collecting || !s_data_file || count == 0) return;
    
    data_header_t header = {
        .timestamp = esp_timer_get_time(),
        .source_id = 1,  // ESP32 source
        .label_len = (uint8_t)strlen(s_current_label),
        .sample_count = (uint16_t)count
    };
    
    // Write header
    fwrite(&header, sizeof(header), 1, s_data_file);
    
    // Write label
    fwrite(s_current_label, 1, header.label_len, s_data_file);
    
    // Write samples
    fwrite(samples, sizeof(float), count, s_data_file);
    
    fflush(s_data_file);
    s_collecting = false;
    
    #ifdef CONFIG_DETAILED_LOGGING
    ESP_LOGI(TAG, "Collection finished: %d samples", count);
    #endif
}

void data_collection_finish(void) {
    // Legacy function for compatibility
    s_collecting = false;
}