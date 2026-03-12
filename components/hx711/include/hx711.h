#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#define HX711_NUM_SENSORS 4

typedef struct {
    gpio_num_t dout;
    gpio_num_t sck;
} hx711_pins_t;

typedef struct {
    hx711_pins_t pins;
    int32_t offset;
    float scale_factor;
} hx711_sensor_t;

/**
 * Initialize all HX711 sensors with their GPIO pin assignments.
 */
esp_err_t hx711_init(void);

/**
 * Read raw 24-bit value from a specific sensor.
 */
esp_err_t hx711_read_raw(int sensor_idx, int32_t *raw_value);

/**
 * Tare (zero) a specific sensor by averaging current readings.
 */
esp_err_t hx711_tare(int sensor_idx, int num_samples);

/**
 * Tare all sensors.
 */
esp_err_t hx711_tare_all(int num_samples);

/**
 * Set calibration scale factor for a sensor.
 */
void hx711_set_scale(int sensor_idx, float scale_factor);

/**
 * Get weight in kg from a specific sensor (raw - offset) / scale.
 */
esp_err_t hx711_get_weight(int sensor_idx, float *weight_kg);

/**
 * Get access to sensor struct for NVS save/restore.
 */
hx711_sensor_t *hx711_get_sensor(int sensor_idx);
