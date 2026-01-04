/**
 * @file statistics.h
 * @brief Statistical utilities and tracking structures
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Moving average structure
 */
typedef struct {
    float value;
    uint32_t count;
    float total;
} moving_average_t;

/**
 * @brief Min/max tracker structure
 */
typedef struct {
    float min;
    float max;
    bool initialized;
    uint32_t update_count;
} min_max_tracker_t;

/**
 * @brief Distribution statistics structure
 */
typedef struct {
    uint32_t count;
    float sum;
    float sum_sq;
    float min;
    float max;
    bool initialized;
} distribution_stats_t;

/**
 * @brief Rate calculator structure
 */
typedef struct {
    uint32_t count;
    uint32_t total_count;
    uint64_t start_time_us;
    uint64_t last_update_us;
    uint64_t time_window_us;
    float current_rate;
    float average_rate;
} rate_calculator_t;

/**
 * @brief Histogram structure
 */
typedef struct {
    float min_value;
    float max_value;
    uint32_t num_bins;
    float bin_width;
    uint32_t *bins;
    uint32_t total_count;
} histogram_t;

// Function declarations for all the new functions in statistics.c
void moving_average_init(moving_average_t *ma);
void moving_average_update(moving_average_t *ma, float new_value);
void moving_average_update_simple(moving_average_t *ma, float new_value);
void moving_average_reset(moving_average_t *ma);
float moving_average_get(const moving_average_t *ma);
uint32_t moving_average_count(const moving_average_t *ma);

void min_max_tracker_init(min_max_tracker_t *tracker);
void min_max_tracker_update(min_max_tracker_t *tracker, float value);
void min_max_tracker_reset(min_max_tracker_t *tracker);
float min_max_tracker_range(const min_max_tracker_t *tracker);
float min_max_tracker_midpoint(const min_max_tracker_t *tracker);
float min_max_tracker_normalize(const min_max_tracker_t *tracker, float value);

void distribution_stats_init(distribution_stats_t *stats);
void distribution_stats_add(distribution_stats_t *stats, float value);
float distribution_stats_mean(const distribution_stats_t *stats);
float distribution_stats_variance(const distribution_stats_t *stats);
float distribution_stats_std_dev(const distribution_stats_t *stats);
float distribution_stats_coefficient_of_variation(const distribution_stats_t *stats);
void distribution_stats_reset(distribution_stats_t *stats);

void rate_calculator_init(rate_calculator_t *calc, uint32_t time_window_ms);
void rate_calculator_update(rate_calculator_t *calc);
float rate_calculator_get_current(const rate_calculator_t *calc);
float rate_calculator_get_average(const rate_calculator_t *calc);
void rate_calculator_reset(rate_calculator_t *calc);

void histogram_init(histogram_t *hist, float min_value, float max_value, uint32_t num_bins);
void histogram_add(histogram_t *hist, float value);
uint32_t histogram_get_bin(const histogram_t *hist, float value);
uint32_t histogram_get_bin_count(const histogram_t *hist, uint32_t bin_idx);
float histogram_get_normalized(const histogram_t *hist, uint32_t bin_idx);
uint32_t histogram_get_mode(const histogram_t *hist);
void histogram_reset(histogram_t *hist);
void histogram_cleanup(histogram_t *hist);

float statistics_median_sorted(const float *values, uint32_t count);
float statistics_percentile_sorted(const float *values, uint32_t count, float percentile);
float statistics_iqr_sorted(const float *values, uint32_t count);
uint32_t statistics_detect_outliers_iqr(const float *values, uint32_t count, 
                                        uint32_t *outliers, uint32_t max_outliers,
                                        float multiplier);