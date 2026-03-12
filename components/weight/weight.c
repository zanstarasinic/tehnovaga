#include "weight.h"
#include "hx711.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "weight";

static float sensor_buf[HX711_NUM_SENSORS][WEIGHT_MOVING_AVG_SIZE];
static int buf_idx = 0;
static bool buf_full = false;

static weight_result_t last_result;
static float max_capacity_kg = 40000.0f;
static float stability_threshold = 5.0f;
static int stability_count_needed = 40; /* 2s at 20Hz */
static int stability_counter = 0;

esp_err_t weight_init(void)
{
    memset(sensor_buf, 0, sizeof(sensor_buf));
    memset(&last_result, 0, sizeof(last_result));
    buf_idx = 0;
    buf_full = false;
    stability_counter = 0;

    ESP_LOGI(TAG, "Weight processor initialized (capacity=%.0f kg)", max_capacity_kg);
    return ESP_OK;
}

static float compute_avg(float *buf, int count)
{
    float sum = 0;
    for (int i = 0; i < count; i++) {
        sum += buf[i];
    }
    return sum / count;
}

static float compute_stddev(float *buf, int count, float mean)
{
    float sum_sq = 0;
    for (int i = 0; i < count; i++) {
        float diff = buf[i] - mean;
        sum_sq += diff * diff;
    }
    return sqrtf(sum_sq / count);
}

esp_err_t weight_update(void)
{
    float total = 0;

    for (int i = 0; i < HX711_NUM_SENSORS; i++) {
        float w;
        esp_err_t err = hx711_get_weight(i, &w);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Sensor %d read failed", i);
            w = last_result.sensor_kg[i]; /* hold last value */
        }

        sensor_buf[i][buf_idx] = w;

        int count = buf_full ? WEIGHT_MOVING_AVG_SIZE : (buf_idx + 1);
        float avg = compute_avg(sensor_buf[i], count);
        last_result.sensor_kg[i] = avg;
        total += avg;
    }

    buf_idx = (buf_idx + 1) % WEIGHT_MOVING_AVG_SIZE;
    if (buf_idx == 0) buf_full = true;

    /* Stability detection on total weight history */
    float prev_total = last_result.total_kg;
    last_result.total_kg = total;

    if (fabsf(total - prev_total) < stability_threshold) {
        stability_counter++;
    } else {
        stability_counter = 0;
    }
    last_result.stable = (stability_counter >= stability_count_needed);

    /* Capacity percentage */
    last_result.capacity_pct = (max_capacity_kg > 0) ? (total / max_capacity_kg) * 100.0f : 0;
    if (last_result.capacity_pct > 100.0f) last_result.capacity_pct = 100.0f;
    if (last_result.capacity_pct < 0.0f) last_result.capacity_pct = 0.0f;

    return ESP_OK;
}

weight_result_t weight_get_result(void)
{
    return last_result;
}

void weight_set_capacity(float max_kg)
{
    max_capacity_kg = max_kg;
}

void weight_set_stability(float threshold_kg, int stable_count)
{
    stability_threshold = threshold_kg;
    stability_count_needed = stable_count;
}
