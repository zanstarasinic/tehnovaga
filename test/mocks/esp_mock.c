#include "esp_mock.h"

/* ---- Timer ---- */
static int64_t mock_time = 0;
int64_t esp_timer_get_time(void) { return mock_time; }
void mock_set_time(int64_t us) { mock_time = us; }

/* ---- GPIO ---- */
static int mock_gpio_levels[64] = {0};

esp_err_t gpio_config(const gpio_config_t *cfg) { (void)cfg; return ESP_OK; }
esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level) { (void)pin; (void)level; return ESP_OK; }
int gpio_get_level(gpio_num_t pin) { return mock_gpio_levels[pin]; }
void mock_gpio_set_read_value(int gpio, int level) { mock_gpio_levels[gpio] = level; }

/* ---- UART ---- */
static uint8_t uart_tx_buf[MOCK_UART_BUF_SIZE];
static int uart_tx_len = 0;
static uint8_t uart_rx_buf[256];
static int uart_rx_len = 0;

esp_err_t uart_param_config(int uart, const uart_config_t *cfg) { (void)uart; (void)cfg; return ESP_OK; }
esp_err_t uart_set_pin(int uart, int tx, int rx, int rts, int cts) { (void)uart; (void)tx; (void)rx; (void)rts; (void)cts; return ESP_OK; }
esp_err_t uart_driver_install(int uart, int rx_buf, int tx_buf, int q_size, void *q, int flags) { (void)uart; (void)rx_buf; (void)tx_buf; (void)q_size; (void)q; (void)flags; return ESP_OK; }

int uart_write_bytes(int uart, const void *data, int len)
{
    (void)uart;
    if (uart_tx_len + len <= MOCK_UART_BUF_SIZE) {
        memcpy(uart_tx_buf + uart_tx_len, data, len);
        uart_tx_len += len;
    }
    return len;
}

int uart_read_bytes(int uart, uint8_t *buf, int max, int timeout)
{
    (void)uart; (void)timeout;
    int n = (uart_rx_len < max) ? uart_rx_len : max;
    if (n > 0) {
        memcpy(buf, uart_rx_buf, n);
        uart_rx_len = 0;
    }
    return n;
}

const uint8_t *mock_uart_get_tx(int *len) { *len = uart_tx_len; return uart_tx_buf; }
void mock_uart_reset(void) { uart_tx_len = 0; uart_rx_len = 0; }
void mock_uart_inject_rx(const uint8_t *data, int len)
{
    int n = (len < (int)sizeof(uart_rx_buf)) ? len : (int)sizeof(uart_rx_buf);
    memcpy(uart_rx_buf, data, n);
    uart_rx_len = n;
}

/* ---- FreeRTOS ---- */
void vTaskDelay(int ticks) { (void)ticks; }
void vTaskDelayUntil(TickType_t *prev, TickType_t inc) { (void)prev; (void)inc; }
TickType_t xTaskGetTickCount(void) { return 0; }
BaseType_t xTaskCreate(void (*fn)(void*), const char *name, int stack, void *arg, int prio, TaskHandle_t *handle)
{
    (void)fn; (void)name; (void)stack; (void)arg; (void)prio; (void)handle;
    return pdPASS;
}
void ets_delay_us(int us) { (void)us; }

/* ---- NVS ---- */
esp_err_t nvs_flash_init(void) { return ESP_OK; }
esp_err_t nvs_flash_erase(void) { return ESP_OK; }
esp_err_t nvs_open(const char *ns, int mode, nvs_handle_t *h) { (void)ns; (void)mode; *h = 1; return ESP_OK; }
esp_err_t nvs_set_i32(nvs_handle_t h, const char *key, int32_t val) { (void)h; (void)key; (void)val; return ESP_OK; }
esp_err_t nvs_get_i32(nvs_handle_t h, const char *key, int32_t *val) { (void)h; (void)key; (void)val; return ESP_ERR_NVS_NOT_FOUND; }
esp_err_t nvs_commit(nvs_handle_t h) { (void)h; return ESP_OK; }
void nvs_close(nvs_handle_t h) { (void)h; }

/* ---- Global reset ---- */
void mock_reset_all(void)
{
    mock_time = 0;
    memset(mock_gpio_levels, 0, sizeof(mock_gpio_levels));
    mock_uart_reset();
}
