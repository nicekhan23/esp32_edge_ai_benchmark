#ifndef WAVEFORM_TABLES_H
#define WAVEFORM_TABLES_H

#include <stdint.h>

#define TABLE_SIZE 256
#define M_PI 3.14159265358979323846f

// Pre-computed 8-bit values for one full cycle (0-255)
extern const uint8_t sine_lut[TABLE_SIZE];
extern const uint8_t square_lut[TABLE_SIZE];
extern const uint8_t triangle_lut[TABLE_SIZE];
extern const uint8_t sawtooth_lut[TABLE_SIZE];

// Initialize all waveform tables (call once at startup)
void waveform_tables_init(void);

#endif