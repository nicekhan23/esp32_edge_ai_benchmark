/**
 * @file statistics.c
 * @brief Statistical utilities and tracking structures implementation
 * @details Provides common statistical calculations used across multiple modules
 *          including moving averages, min/max tracking, and distribution analysis.
 * 
 * @author Darkhan Zhanibekuly
 * @date 2025 December
 * @version 1.0.0
 * 
 * @note All functions are thread-safe when used with separate instances
 * @note Uses floating-point math; ensure FPU is enabled
 */

#include "statistics.h"
#include "common.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/**
 * @brief Initialize moving average
 */
void moving_average_init(moving_average_t *ma) {
    if (!ma) return;
    
    ma->value = 0.0f;
    ma->count = 0;
    ma->total = 0.0f;
}

/**
 * @brief Update moving average with new value
 */
void moving_average_update(moving_average_t *ma, float new_value) {
    if (!ma) return;
    
    if (ma->count == 0) {
        ma->value = new_value;
        ma->total = new_value;
    } else {
        // Exponential moving average (EMA) with adaptive weighting
        float alpha = 2.0f / (ma->count + 1);
        alpha = CLAMP(alpha, 0.01f, 0.3f); // Limit weighting
        
        ma->value = alpha * new_value + (1 - alpha) * ma->value;
        ma->total += new_value;
    }
    ma->count++;
}

/**
 * @brief Update moving average with simple cumulative average
 */
void moving_average_update_simple(moving_average_t *ma, float new_value) {
    if (!ma) return;
    
    if (ma->count == 0) {
        ma->value = new_value;
        ma->total = new_value;
    } else {
        ma->total += new_value;
        ma->value = ma->total / (ma->count + 1);
    }
    ma->count++;
}

/**
 * @brief Reset moving average
 */
void moving_average_reset(moving_average_t *ma) {
    if (!ma) return;
    
    ma->value = 0.0f;
    ma->count = 0;
    ma->total = 0.0f;
}

/**
 * @brief Get current moving average value
 */
float moving_average_get(const moving_average_t *ma) {
    if (!ma) return 0.0f;
    return ma->value;
}

/**
 * @brief Get total number of samples in moving average
 */
uint32_t moving_average_count(const moving_average_t *ma) {
    if (!ma) return 0;
    return ma->count;
}

/**
 * @brief Initialize min/max tracker
 */
void min_max_tracker_init(min_max_tracker_t *tracker) {
    if (!tracker) return;
    
    tracker->min = 0.0f;
    tracker->max = 0.0f;
    tracker->initialized = false;
    tracker->update_count = 0;
}

/**
 * @brief Update min/max tracker with new value
 */
void min_max_tracker_update(min_max_tracker_t *tracker, float value) {
    if (!tracker) return;
    
    if (!tracker->initialized) {
        tracker->min = value;
        tracker->max = value;
        tracker->initialized = true;
    } else {
        if (value < tracker->min) tracker->min = value;
        if (value > tracker->max) tracker->max = value;
    }
    tracker->update_count++;
}

/**
 * @brief Reset min/max tracker
 */
void min_max_tracker_reset(min_max_tracker_t *tracker) {
    if (!tracker) return;
    
    tracker->min = 0.0f;
    tracker->max = 0.0f;
    tracker->initialized = false;
    tracker->update_count = 0;
}

/**
 * @brief Get range (max - min) from tracker
 */
float min_max_tracker_range(const min_max_tracker_t *tracker) {
    if (!tracker || !tracker->initialized) return 0.0f;
    return tracker->max - tracker->min;
}

/**
 * @brief Get midpoint (average of min and max) from tracker
 */
float min_max_tracker_midpoint(const min_max_tracker_t *tracker) {
    if (!tracker || !tracker->initialized) return 0.0f;
    return (tracker->min + tracker->max) / 2.0f;
}

/**
 * @brief Normalize value to [0, 1] range based on tracker min/max
 */
float min_max_tracker_normalize(const min_max_tracker_t *tracker, float value) {
    if (!tracker || !tracker->initialized) return 0.0f;
    
    float range = tracker->max - tracker->min;
    if (range < 1e-6f) return 0.5f; // All values are the same
    
    return (value - tracker->min) / range;
}

/**
 * @brief Initialize distribution statistics
 */
void distribution_stats_init(distribution_stats_t *stats) {
    if (!stats) return;
    
    stats->count = 0;
    stats->sum = 0.0f;
    stats->sum_sq = 0.0f;
    stats->min = 0.0f;
    stats->max = 0.0f;
    stats->initialized = false;
}

/**
 * @brief Add sample to distribution statistics
 */
void distribution_stats_add(distribution_stats_t *stats, float value) {
    if (!stats) return;
    
    if (!stats->initialized) {
        stats->min = value;
        stats->max = value;
        stats->initialized = true;
    } else {
        if (value < stats->min) stats->min = value;
        if (value > stats->max) stats->max = value;
    }
    
    stats->sum += value;
    stats->sum_sq += value * value;
    stats->count++;
}

/**
 * @brief Calculate mean from distribution statistics
 */
float distribution_stats_mean(const distribution_stats_t *stats) {
    if (!stats || stats->count == 0) return 0.0f;
    return stats->sum / stats->count;
}

/**
 * @brief Calculate variance from distribution statistics
 */
float distribution_stats_variance(const distribution_stats_t *stats) {
    if (!stats || stats->count < 2) return 0.0f;
    
    float mean = distribution_stats_mean(stats);
    return (stats->sum_sq / stats->count) - (mean * mean);
}

/**
 * @brief Calculate standard deviation from distribution statistics
 */
float distribution_stats_std_dev(const distribution_stats_t *stats) {
    float variance = distribution_stats_variance(stats);
    return sqrtf(fabsf(variance)); // Use fabs to handle possible negative due to floating errors
}

/**
 * @brief Calculate coefficient of variation from distribution statistics
 */
float distribution_stats_coefficient_of_variation(const distribution_stats_t *stats) {
    if (!stats || stats->count == 0) return 0.0f;
    
    float mean = distribution_stats_mean(stats);
    if (fabsf(mean) < 1e-6f) return 0.0f;
    
    float std_dev = distribution_stats_std_dev(stats);
    return (std_dev / mean) * 100.0f; // Return as percentage
}

/**
 * @brief Reset distribution statistics
 */
void distribution_stats_reset(distribution_stats_t *stats) {
    if (!stats) return;
    
    stats->count = 0;
    stats->sum = 0.0f;
    stats->sum_sq = 0.0f;
    stats->min = 0.0f;
    stats->max = 0.0f;
    stats->initialized = false;
}

/**
 * @brief Initialize rate calculator
 */
void rate_calculator_init(rate_calculator_t *calc, uint32_t time_window_ms) {
    if (!calc) return;
    
    calc->count = 0;
    calc->total_count = 0;
    calc->start_time_us = 0;
    calc->last_update_us = 0;
    calc->time_window_us = time_window_ms * 1000;
    calc->current_rate = 0.0f;
    calc->average_rate = 0.0f;
}

/**
 * @brief Update rate calculator with new event
 */
void rate_calculator_update(rate_calculator_t *calc) {
    if (!calc) return;
    
    uint64_t now = get_time_us();
    
    if (calc->start_time_us == 0) {
        calc->start_time_us = now;
    }
    
    calc->count++;
    calc->total_count++;
    calc->last_update_us = now;
    
    // Calculate current rate (events per second)
    uint64_t elapsed = now - calc->start_time_us;
    if (elapsed > 0) {
        calc->current_rate = (float)calc->count * 1000000.0f / elapsed;
        
        // Reset if time window exceeded
        if (elapsed > calc->time_window_us) {
            calc->count = 1; // Keep current event
            calc->start_time_us = now;
        }
    }
    
    // Calculate average rate
    elapsed = now - (calc->start_time_us ? calc->start_time_us : now);
    if (elapsed > 0) {
        calc->average_rate = (float)calc->total_count * 1000000.0f / elapsed;
    }
}

/**
 * @brief Get current rate (events per second)
 */
float rate_calculator_get_current(const rate_calculator_t *calc) {
    if (!calc) return 0.0f;
    return calc->current_rate;
}

/**
 * @brief Get average rate (events per second)
 */
float rate_calculator_get_average(const rate_calculator_t *calc) {
    if (!calc) return 0.0f;
    return calc->average_rate;
}

/**
 * @brief Reset rate calculator
 */
void rate_calculator_reset(rate_calculator_t *calc) {
    if (!calc) return;
    
    calc->count = 0;
    calc->total_count = 0;
    calc->start_time_us = 0;
    calc->last_update_us = 0;
    calc->current_rate = 0.0f;
    calc->average_rate = 0.0f;
}

/**
 * @brief Initialize histogram
 */
void histogram_init(histogram_t *hist, float min_value, float max_value, uint32_t num_bins) {
    if (!hist || num_bins == 0 || max_value <= min_value) return;
    
    hist->min_value = min_value;
    hist->max_value = max_value;
    hist->num_bins = num_bins;
    hist->total_count = 0;
    
    // Calculate bin width
    hist->bin_width = (max_value - min_value) / num_bins;
    
    // Allocate bins
    hist->bins = (uint32_t*)calloc(num_bins, sizeof(uint32_t));
    if (!hist->bins) {
        hist->num_bins = 0;
        return;
    }
}

/**
 * @brief Add value to histogram
 */
void histogram_add(histogram_t *hist, float value) {
    if (!hist || !hist->bins || hist->num_bins == 0) return;
    
    // Clamp value to histogram range
    value = CLAMP(value, hist->min_value, hist->max_value - 0.0001f);
    
    // Calculate bin index
    uint32_t bin_idx = (uint32_t)((value - hist->min_value) / hist->bin_width);
    
    // Ensure index is within bounds
    if (bin_idx >= hist->num_bins) {
        bin_idx = hist->num_bins - 1;
    }
    
    hist->bins[bin_idx]++;
    hist->total_count++;
}

/**
 * @brief Get bin index for a value
 */
uint32_t histogram_get_bin(const histogram_t *hist, float value) {
    if (!hist || !hist->bins || hist->num_bins == 0) return 0;
    
    value = CLAMP(value, hist->min_value, hist->max_value - 0.0001f);
    uint32_t bin_idx = (uint32_t)((value - hist->min_value) / hist->bin_width);
    
    if (bin_idx >= hist->num_bins) {
        bin_idx = hist->num_bins - 1;
    }
    
    return bin_idx;
}

/**
 * @brief Get count for specific bin
 */
uint32_t histogram_get_bin_count(const histogram_t *hist, uint32_t bin_idx) {
    if (!hist || !hist->bins || bin_idx >= hist->num_bins) return 0;
    return hist->bins[bin_idx];
}

/**
 * @brief Get normalized histogram (probabilities)
 */
float histogram_get_normalized(const histogram_t *hist, uint32_t bin_idx) {
    if (!hist || !hist->bins || bin_idx >= hist->num_bins || hist->total_count == 0) 
        return 0.0f;
    
    return (float)hist->bins[bin_idx] / hist->total_count;
}

/**
 * @brief Get histogram mode (most frequent bin)
 */
uint32_t histogram_get_mode(const histogram_t *hist) {
    if (!hist || !hist->bins || hist->num_bins == 0) return 0;
    
    uint32_t max_count = 0;
    uint32_t mode_bin = 0;
    
    for (uint32_t i = 0; i < hist->num_bins; i++) {
        if (hist->bins[i] > max_count) {
            max_count = hist->bins[i];
            mode_bin = i;
        }
    }
    
    return mode_bin;
}

/**
 * @brief Reset histogram counts
 */
void histogram_reset(histogram_t *hist) {
    if (!hist || !hist->bins) return;
    
    memset(hist->bins, 0, hist->num_bins * sizeof(uint32_t));
    hist->total_count = 0;
}

/**
 * @brief Cleanup histogram memory
 */
void histogram_cleanup(histogram_t *hist) {
    if (!hist) return;
    
    if (hist->bins) {
        free(hist->bins);
        hist->bins = NULL;
    }
    
    hist->num_bins = 0;
    hist->total_count = 0;
}

/**
 * @brief Calculate median from sorted array
 * 
 * @param[in] values Array of sorted values
 * @param[in] count Number of values
 * @return float Median value
 */
float statistics_median_sorted(const float *values, uint32_t count) {
    if (!values || count == 0) return 0.0f;
    
    if (count % 2 == 0) {
        // Even number of elements: average of two middle values
        uint32_t idx1 = count / 2 - 1;
        uint32_t idx2 = count / 2;
        return (values[idx1] + values[idx2]) / 2.0f;
    } else {
        // Odd number of elements: middle value
        return values[count / 2];
    }
}

/**
 * @brief Calculate percentile from sorted array
 * 
 * @param[in] values Array of sorted values
 * @param[in] count Number of values
 * @param[in] percentile Percentile to calculate (0-100)
 * @return float Percentile value
 */
float statistics_percentile_sorted(const float *values, uint32_t count, float percentile) {
    if (!values || count == 0 || percentile < 0.0f || percentile > 100.0f) 
        return 0.0f;
    
    // Calculate index
    float index = (percentile / 100.0f) * (count - 1);
    uint32_t lower_idx = (uint32_t)index;
    float fraction = index - lower_idx;
    
    if (lower_idx >= count - 1) {
        return values[count - 1];
    }
    
    // Linear interpolation between values
    return values[lower_idx] + fraction * (values[lower_idx + 1] - values[lower_idx]);
}

/**
 * @brief Calculate interquartile range (IQR) from sorted array
 * 
 * @param[in] values Array of sorted values
 * @param[in] count Number of values
 * @return float IQR value (Q3 - Q1)
 */
float statistics_iqr_sorted(const float *values, uint32_t count) {
    float q1 = statistics_percentile_sorted(values, count, 25.0f);
    float q3 = statistics_percentile_sorted(values, count, 75.0f);
    return q3 - q1;
}

/**
 * @brief Detect outliers using IQR method
 * 
 * @param[in] values Array of sorted values
 * @param[in] count Number of values
 * @param[out] outliers Array to store outlier indices (must be pre-allocated)
 * @param[in] max_outliers Maximum number of outliers to detect
 * @return uint32_t Number of outliers detected
 */
uint32_t statistics_detect_outliers_iqr(const float *values, uint32_t count, 
                                        uint32_t *outliers, uint32_t max_outliers,
                                        float multiplier) {
    if (!values || count < 3 || !outliers || max_outliers == 0) return 0;
    
    float q1 = statistics_percentile_sorted(values, count, 25.0f);
    float q3 = statistics_percentile_sorted(values, count, 75.0f);
    float iqr = q3 - q1;
    
    if (iqr < 1e-6f) return 0; // No variation
    
    float lower_bound = q1 - multiplier * iqr;
    float upper_bound = q3 + multiplier * iqr;
    
    uint32_t outlier_count = 0;
    for (uint32_t i = 0; i < count && outlier_count < max_outliers; i++) {
        if (values[i] < lower_bound || values[i] > upper_bound) {
            outliers[outlier_count++] = i;
        }
    }
    
    return outlier_count;
}