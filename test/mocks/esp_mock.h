#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* ---- esp_err ---- */
typedef int esp_err_t;
#define ESP_OK                    0
#define ESP_FAIL                  -1
#define ESP_ERR_INVALID_ARG       0x102
#define ESP_ERR_TIMEOUT           0x107
#define ESP_ERR_NVS_NO_FREE_PAGES 0x1100
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x1110
#define ESP_ERR_NVS_NOT_FOUND     0x1102

#define ESP_ERROR_CHECK(x) (void)(x)

/* ---- esp_log (no-op) ---- */
#define ESP_LOGI(tag, fmt, ...)
#define ESP_LOGW(tag, fmt, ...)
#define ESP_LOGE(tag, fmt, ...)

/* ---- esp_timer ---- */
int64_t esp_timer_get_time(void);
void mock_set_time(int64_t us);

/* ---- GPIO ---- */
typedef int gpio_num_t;

#define GPIO_NUM_12 12
#define GPIO_NUM_13 13
#define GPIO_NUM_14 14
#define GPIO_NUM_16 16
#define GPIO_NUM_17 17
#define GPIO_NUM_25 25
#define GPIO_NUM_26 26
#define GPIO_NUM_27 27
#define GPIO_NUM_32 32
#define GPIO_NUM_33 33

#define GPIO_MODE_INPUT   1
#define GPIO_MODE_OUTPUT  2
#define GPIO_PULLUP_DISABLE   0
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE     0

typedef struct {
    uint64_t pin_bit_mask;
    int mode;
    int pull_up_en;
    int pull_down_en;
    int intr_type;
} gpio_config_t;

esp_err_t gpio_config(const gpio_config_t *cfg);
esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level);
int gpio_get_level(gpio_num_t pin);

void mock_gpio_set_read_value(int gpio, int level);

/* ---- UART ---- */
#define UART_NUM_2          2
#define UART_DATA_8_BITS    3
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1    1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_PIN_NO_CHANGE  (-1)

typedef struct {
    int baud_rate;
    int data_bits;
    int parity;
    int stop_bits;
    int flow_ctrl;
} uart_config_t;

esp_err_t uart_param_config(int uart, const uart_config_t *cfg);
esp_err_t uart_set_pin(int uart, int tx, int rx, int rts, int cts);
esp_err_t uart_driver_install(int uart, int rx_buf, int tx_buf, int q_size, void *q, int flags);
int uart_write_bytes(int uart, const void *data, int len);
int uart_read_bytes(int uart, uint8_t *buf, int max, int timeout);

#define MOCK_UART_BUF_SIZE 4096
const uint8_t *mock_uart_get_tx(int *len);
void mock_uart_reset(void);
void mock_uart_inject_rx(const uint8_t *data, int len);

/* ---- FreeRTOS ---- */
typedef int BaseType_t;
typedef uint32_t TickType_t;
typedef void *TaskHandle_t;

#define pdPASS       1
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))

typedef struct { int dummy; } portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED {0}

void vTaskDelay(int ticks);
void vTaskDelayUntil(TickType_t *prev, TickType_t inc);
TickType_t xTaskGetTickCount(void);
BaseType_t xTaskCreate(void (*fn)(void*), const char *name, int stack, void *arg, int prio, TaskHandle_t *handle);

#define taskENTER_CRITICAL(mux)
#define taskEXIT_CRITICAL(mux)

/* ---- rom/ets_sys ---- */
void ets_delay_us(int us);

/* ---- NVS ---- */
typedef uint32_t nvs_handle_t;
#define NVS_READWRITE 1
#define NVS_READONLY  0

esp_err_t nvs_flash_init(void);
esp_err_t nvs_flash_erase(void);
esp_err_t nvs_open(const char *ns, int mode, nvs_handle_t *h);
esp_err_t nvs_set_i32(nvs_handle_t h, const char *key, int32_t val);
esp_err_t nvs_get_i32(nvs_handle_t h, const char *key, int32_t *val);
esp_err_t nvs_commit(nvs_handle_t h);
void nvs_close(nvs_handle_t h);

/* ---- Global mock reset ---- */
void mock_reset_all(void);
