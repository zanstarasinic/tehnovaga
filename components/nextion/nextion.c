#include "nextion.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "nextion";

#define NX_UART        UART_NUM_2
#define NX_TX_PIN      GPIO_NUM_17
#define NX_RX_PIN      GPIO_NUM_16
#define NX_BAUD        9600
#define NX_BUF_SIZE    256
#define NX_TERM        "\xFF\xFF\xFF"
#define NX_TERM_LEN    3

static nextion_touch_cb_t touch_cb = NULL;

esp_err_t nextion_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = NX_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    esp_err_t err = uart_param_config(NX_UART, &uart_config);
    if (err != ESP_OK) return err;

    err = uart_set_pin(NX_UART, NX_TX_PIN, NX_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;

    err = uart_driver_install(NX_UART, NX_BUF_SIZE * 2, NX_BUF_SIZE * 2, 0, NULL, 0);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "Nextion UART initialized (TX=%d, RX=%d)", NX_TX_PIN, NX_RX_PIN);
    return ESP_OK;
}

esp_err_t nextion_send_cmd(const char *cmd)
{
    int len = strlen(cmd);
    int written = uart_write_bytes(NX_UART, cmd, len);
    written += uart_write_bytes(NX_UART, NX_TERM, NX_TERM_LEN);

    if (written != len + NX_TERM_LEN) {
        ESP_LOGE(TAG, "UART write failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t nextion_set_value(const char *component, int32_t value)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s.val=%ld", component, (long)value);
    return nextion_send_cmd(buf);
}

esp_err_t nextion_set_text(const char *component, const char *text)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "%s.txt=\"%s\"", component, text);
    return nextion_send_cmd(buf);
}

esp_err_t nextion_set_page(uint8_t page)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "page %d", page);
    return nextion_send_cmd(buf);
}

void nextion_set_touch_callback(nextion_touch_cb_t cb)
{
    touch_cb = cb;
}

static void nextion_listener_task(void *arg)
{
    uint8_t buf[NX_BUF_SIZE];

    while (1) {
        int len = uart_read_bytes(NX_UART, buf, sizeof(buf), pdMS_TO_TICKS(100));
        if (len <= 0) continue;

        /* Parse touch events: 0x65 page comp event 0xFF 0xFF 0xFF */
        for (int i = 0; i < len - 6; i++) {
            if (buf[i] == 0x65 &&
                buf[i + 4] == 0xFF &&
                buf[i + 5] == 0xFF &&
                buf[i + 6] == 0xFF) {
                uint8_t page = buf[i + 1];
                uint8_t comp = buf[i + 2];
                uint8_t event = buf[i + 3];
                ESP_LOGI(TAG, "Touch: page=%d comp=%d event=%d", page, comp, event);
                if (touch_cb) {
                    touch_cb(page, comp, event);
                }
                i += 6;
            }
        }
    }
}

esp_err_t nextion_start_listener(void)
{
    BaseType_t ret = xTaskCreate(nextion_listener_task, "nextion_rx", 2048, NULL, 5, NULL);
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}
