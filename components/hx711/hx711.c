#include "hx711.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <rom/ets_sys.h>

static const char *TAG = "hx711";

/* Pin assignments: DOUT, SCK for each of 4 sensors */
static hx711_sensor_t sensors[HX711_NUM_SENSORS] = {
    { .pins = { .dout = GPIO_NUM_32, .sck = GPIO_NUM_33 }, .offset = 0, .scale_factor = 1.0f },
    { .pins = { .dout = GPIO_NUM_25, .sck = GPIO_NUM_26 }, .offset = 0, .scale_factor = 1.0f },
    { .pins = { .dout = GPIO_NUM_27, .sck = GPIO_NUM_14 }, .offset = 0, .scale_factor = 1.0f },
    { .pins = { .dout = GPIO_NUM_12, .sck = GPIO_NUM_13 }, .offset = 0, .scale_factor = 1.0f },
};

esp_err_t hx711_init(void)
{
    for (int i = 0; i < HX711_NUM_SENSORS; i++) {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << sensors[i].pins.sck),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK) return err;

        io_conf.pin_bit_mask = (1ULL << sensors[i].pins.dout);
        io_conf.mode = GPIO_MODE_INPUT;
        err = gpio_config(&io_conf);
        if (err != ESP_OK) return err;

        gpio_set_level(sensors[i].pins.sck, 0);
        ESP_LOGI(TAG, "Sensor %d initialized (DOUT=%d, SCK=%d)", i,
                 sensors[i].pins.dout, sensors[i].pins.sck);
    }
    return ESP_OK;
}

esp_err_t hx711_read_raw(int sensor_idx, int32_t *raw_value)
{
    if (sensor_idx < 0 || sensor_idx >= HX711_NUM_SENSORS || !raw_value) {
        return ESP_ERR_INVALID_ARG;
    }

    hx711_pins_t *pins = &sensors[sensor_idx].pins;

    /* Wait for DOUT to go low (data ready), timeout 200ms */
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(pins->dout)) {
        if ((esp_timer_get_time() - start) > 200000) {
            ESP_LOGW(TAG, "Sensor %d timeout", sensor_idx);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }

    /* Read 24 bits via bit-bang */
    int32_t value = 0;
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    taskENTER_CRITICAL(&mux);

    for (int i = 0; i < 24; i++) {
        gpio_set_level(pins->sck, 1);
        ets_delay_us(1);
        value = (value << 1) | gpio_get_level(pins->dout);
        gpio_set_level(pins->sck, 0);
        ets_delay_us(1);
    }

    /* 25th pulse: Channel A, gain 128 */
    gpio_set_level(pins->sck, 1);
    ets_delay_us(1);
    gpio_set_level(pins->sck, 0);
    ets_delay_us(1);

    taskEXIT_CRITICAL(&mux);

    /* Sign extend 24-bit to 32-bit */
    if (value & 0x800000) {
        value |= (int32_t)0xFF000000;
    }

    *raw_value = value;
    return ESP_OK;
}

esp_err_t hx711_tare(int sensor_idx, int num_samples)
{
    if (sensor_idx < 0 || sensor_idx >= HX711_NUM_SENSORS) {
        return ESP_ERR_INVALID_ARG;
    }

    int64_t sum = 0;
    int valid = 0;

    for (int i = 0; i < num_samples; i++) {
        int32_t raw;
        if (hx711_read_raw(sensor_idx, &raw) == ESP_OK) {
            sum += raw;
            valid++;
        }
    }

    if (valid == 0) return ESP_FAIL;

    sensors[sensor_idx].offset = (int32_t)(sum / valid);
    ESP_LOGI(TAG, "Sensor %d tared: offset=%ld", sensor_idx, (long)sensors[sensor_idx].offset);
    return ESP_OK;
}

esp_err_t hx711_tare_all(int num_samples)
{
    for (int i = 0; i < HX711_NUM_SENSORS; i++) {
        esp_err_t err = hx711_tare(i, num_samples);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Tare failed for sensor %d", i);
        }
    }
    return ESP_OK;
}

void hx711_set_scale(int sensor_idx, float scale_factor)
{
    if (sensor_idx >= 0 && sensor_idx < HX711_NUM_SENSORS) {
        sensors[sensor_idx].scale_factor = scale_factor;
    }
}

esp_err_t hx711_get_weight(int sensor_idx, float *weight_kg)
{
    if (sensor_idx < 0 || sensor_idx >= HX711_NUM_SENSORS || !weight_kg) {
        return ESP_ERR_INVALID_ARG;
    }

    int32_t raw;
    esp_err_t err = hx711_read_raw(sensor_idx, &raw);
    if (err != ESP_OK) return err;

    *weight_kg = (float)(raw - sensors[sensor_idx].offset) / sensors[sensor_idx].scale_factor;
    return ESP_OK;
}

hx711_sensor_t *hx711_get_sensor(int sensor_idx)
{
    if (sensor_idx >= 0 && sensor_idx < HX711_NUM_SENSORS) {
        return &sensors[sensor_idx];
    }
    return NULL;
}
