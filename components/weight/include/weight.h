#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define WEIGHT_MOVING_AVG_SIZE 10

typedef struct {
    float total_kg;
    float sensor_kg[4];
    bool stable;
    float capacity_pct;
} weight_result_t;

/**
 * Initialize the weight processing module.
 */
esp_err_t weight_init(void);

/**
 * Read all sensors, apply filtering, and compute total weight.
 * Call this at your desired sample rate (e.g., 20Hz).
 */
esp_err_t weight_update(void);

/**
 * Get the latest processed weight result.
 */
weight_result_t weight_get_result(void);

/**
 * Set the maximum capacity for percentage calculation (default: 40000 kg).
 */
void weight_set_capacity(float max_kg);

/**
 * Set the stability threshold in kg (default: 5.0 kg).
 * Weight is "stable" when std deviation < threshold for stable_count updates.
 */
void weight_set_stability(float threshold_kg, int stable_count);
