#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "hx711.h"
#include "nextion.h"
#include "weight.h"

static const char *TAG = "main";

#define NVS_NAMESPACE "scale_cal"

/* ---------- Calibration wizard state ---------- */

typedef enum {
    CAL_IDLE,
    CAL_ZERO,   /* Step 1: remove weight, tare */
    CAL_LOAD,   /* Step 2: place known weight, capture raw */
    CAL_ENTER,  /* Step 3: dial in known kg, compute factor */
} cal_state_t;

static cal_state_t cal_state = CAL_IDLE;
static int32_t cal_raw[HX711_NUM_SENSORS];  /* raw readings with known weight */
static int cal_known_kg = 1000;             /* known weight, adjustable via +/- */
static int cal_step_kg = 100;              /* +/- step size */

/*
 * Nextion page 1 (calibration) expected components:
 *   tStep  — text: current step instructions
 *   tKg    — text: known weight value being entered
 *   tUnit  — text: "kg" label (static)
 *   bNext  — button: component ID 1 (NEXT / SAVE depending on step)
 *   bPlus  — button: component ID 2 (+)
 *   bMinus — button: component ID 3 (-)
 *   bStep  — button: component ID 4 (toggle step size: 100/10/1)
 *   bBack  — button: component ID 5 (cancel, return to main)
 */

/* ---------- NVS persistence ---------- */

static void save_calibration(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) return;

    for (int i = 0; i < HX711_NUM_SENSORS; i++) {
        hx711_sensor_t *s = hx711_get_sensor(i);
        if (!s) continue;

        char key_off[16], key_scl[16];
        snprintf(key_off, sizeof(key_off), "off_%d", i);
        snprintf(key_scl, sizeof(key_scl), "scl_%d", i);

        nvs_set_i32(nvs, key_off, s->offset);
        nvs_set_i32(nvs, key_scl, (int32_t)(s->scale_factor * 1000.0f));
    }
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG, "Calibration saved");
}

static void load_calibration(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        ESP_LOGW(TAG, "No saved calibration found");
        return;
    }

    for (int i = 0; i < HX711_NUM_SENSORS; i++) {
        hx711_sensor_t *s = hx711_get_sensor(i);
        if (!s) continue;

        char key_off[16], key_scl[16];
        snprintf(key_off, sizeof(key_off), "off_%d", i);
        snprintf(key_scl, sizeof(key_scl), "scl_%d", i);

        int32_t offset, scale_x1000;
        if (nvs_get_i32(nvs, key_off, &offset) == ESP_OK) {
            s->offset = offset;
        }
        if (nvs_get_i32(nvs, key_scl, &scale_x1000) == ESP_OK) {
            s->scale_factor = (float)scale_x1000 / 1000.0f;
        }
    }
    nvs_close(nvs);
    ESP_LOGI(TAG, "Calibration loaded");
}

/* ---------- Calibration wizard ---------- */

static void cal_enter_step(cal_state_t step)
{
    cal_state = step;

    switch (step) {
    case CAL_ZERO:
        nextion_set_text("tStep", "1/3  Remove all weight");
        nextion_set_text("tKg", "");
        nextion_send_cmd("bNext.txt=\"NEXT\"");
        break;

    case CAL_LOAD:
        nextion_set_text("tStep", "2/3  Place known weight");
        nextion_set_text("tKg", "");
        nextion_send_cmd("bNext.txt=\"NEXT\"");
        break;

    case CAL_ENTER: {
        nextion_set_text("tStep", "3/3  Enter known weight");
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", cal_known_kg);
        nextion_set_text("tKg", buf);
        nextion_send_cmd("bNext.txt=\"SAVE\"");
        break;
    }
    default:
        break;
    }
}

static void cal_start(void)
{
    cal_known_kg = 1000;
    cal_step_kg = 100;
    nextion_set_page(1);
    cal_enter_step(CAL_ZERO);
    ESP_LOGI(TAG, "Calibration wizard started");
}

static void cal_cancel(void)
{
    cal_state = CAL_IDLE;
    nextion_set_page(0);
    ESP_LOGI(TAG, "Calibration cancelled");
}

static void cal_handle_next(void)
{
    switch (cal_state) {
    case CAL_ZERO:
        /* Tare all sensors (zero with no load) */
        ESP_LOGI(TAG, "Cal: taring sensors...");
        nextion_set_text("tStep", "Zeroing...");
        hx711_tare_all(20);
        cal_enter_step(CAL_LOAD);
        break;

    case CAL_LOAD:
        /* Capture raw readings with known weight on platform */
        ESP_LOGI(TAG, "Cal: capturing raw readings...");
        nextion_set_text("tStep", "Reading...");
        for (int i = 0; i < HX711_NUM_SENSORS; i++) {
            int64_t sum = 0;
            int valid = 0;
            for (int s = 0; s < 20; s++) {
                int32_t raw;
                if (hx711_read_raw(i, &raw) == ESP_OK) {
                    sum += (raw - hx711_get_sensor(i)->offset);
                    valid++;
                }
            }
            cal_raw[i] = valid > 0 ? (int32_t)(sum / valid) : 0;
            ESP_LOGI(TAG, "Cal: sensor %d raw=%ld", i, (long)cal_raw[i]);
        }
        cal_enter_step(CAL_ENTER);
        break;

    case CAL_ENTER: {
        /* Compute and apply scale factors */
        /* Total raw across all sensors maps to cal_known_kg */
        int64_t total_raw = 0;
        for (int i = 0; i < HX711_NUM_SENSORS; i++) {
            total_raw += cal_raw[i];
        }

        if (total_raw == 0 || cal_known_kg <= 0) {
            ESP_LOGE(TAG, "Cal: invalid readings or weight");
            nextion_set_text("tStep", "ERROR: bad reading");
            vTaskDelay(pdMS_TO_TICKS(2000));
            cal_cancel();
            return;
        }

        /* Each sensor gets proportional scale factor */
        for (int i = 0; i < HX711_NUM_SENSORS; i++) {
            if (cal_raw[i] != 0) {
                float factor = (float)cal_raw[i] / ((float)cal_known_kg / HX711_NUM_SENSORS);
                hx711_set_scale(i, factor);
                ESP_LOGI(TAG, "Cal: sensor %d scale=%.3f", i, factor);
            }
        }

        save_calibration();
        nextion_set_text("tStep", "Calibration saved!");
        vTaskDelay(pdMS_TO_TICKS(1500));
        cal_state = CAL_IDLE;
        nextion_set_page(0);
        ESP_LOGI(TAG, "Calibration complete");
        break;
    }
    default:
        break;
    }
}

static void cal_adjust_kg(int direction)
{
    if (cal_state != CAL_ENTER) return;

    cal_known_kg += direction * cal_step_kg;
    if (cal_known_kg < 0) cal_known_kg = 0;
    if (cal_known_kg > 40000) cal_known_kg = 40000;

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", cal_known_kg);
    nextion_set_text("tKg", buf);
}

static void cal_toggle_step(void)
{
    if (cal_state != CAL_ENTER) return;

    if (cal_step_kg == 100) cal_step_kg = 10;
    else if (cal_step_kg == 10) cal_step_kg = 1;
    else cal_step_kg = 100;

    char buf[16];
    snprintf(buf, sizeof(buf), "+/- %d", cal_step_kg);
    nextion_send_cmd("bStep.txt=\"\"");
    snprintf(buf, sizeof(buf), "bStep.txt=\"+/-%d\"", cal_step_kg);
    nextion_send_cmd(buf);
}

/* ---------- Display update ---------- */

static void update_display(const weight_result_t *r)
{
    char buf[32];
    bool any_error = false;

    for (int i = 0; i < 4; i++) {
        char comp[16];
        snprintf(comp, sizeof(comp), "tS%d", i);
        if (r->sensor_ok[i]) {
            snprintf(buf, sizeof(buf), "%.0f", r->sensor_kg[i]);
        } else {
            snprintf(buf, sizeof(buf), "ERR");
            any_error = true;
        }
        nextion_set_text(comp, buf);
    }

    snprintf(buf, sizeof(buf), "%s%.0f", any_error ? "! " : "", r->total_kg);
    nextion_set_text("tTotal", buf);

    nextion_set_value("jBar", (int32_t)r->capacity_pct);
    nextion_set_value("pStable", r->stable ? 1 : 0);
}

/* ---------- Touch handler ---------- */

static void on_touch(uint8_t page, uint8_t component, uint8_t event)
{
    if (page == 0) {
        switch (component) {
        case 1: /* TARA */
            ESP_LOGI(TAG, "Tare requested");
            hx711_tare_all(20);
            save_calibration();
            break;
        case 2: /* SAVE */
            ESP_LOGI(TAG, "Save weight log");
            break;
        case 3: /* CAL */
            cal_start();
            break;
        }
    } else if (page == 1) {
        switch (component) {
        case 1: /* NEXT / SAVE */
            cal_handle_next();
            break;
        case 2: /* + */
            cal_adjust_kg(1);
            break;
        case 3: /* - */
            cal_adjust_kg(-1);
            break;
        case 4: /* toggle step size */
            cal_toggle_step();
            break;
        case 5: /* BACK / cancel */
            cal_cancel();
            break;
        }
    }
}

/* ---------- Main task ---------- */

static void weight_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();
    int display_div = 0;

    while (1) {
        weight_update();

        if (cal_state == CAL_IDLE && ++display_div >= 4) {
            display_div = 0;
            weight_result_t r = weight_get_result();
            update_display(&r);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Tehnovaga starting...");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_ERROR_CHECK(hx711_init());
    ESP_ERROR_CHECK(nextion_init());
    ESP_ERROR_CHECK(weight_init());

    load_calibration();

    ESP_LOGI(TAG, "Initial tare...");
    hx711_tare_all(20);

    nextion_set_page(0);
    nextion_set_touch_callback(on_touch);
    ESP_ERROR_CHECK(nextion_start_listener());

    xTaskCreate(weight_task, "weight", 4096, NULL, 10, NULL);

    ESP_LOGI(TAG, "System ready");
}
