#pragma once

#include <stdint.h>
#include "esp_err.h"

/* Nextion touch event callback */
typedef void (*nextion_touch_cb_t)(uint8_t page, uint8_t component, uint8_t event);

/**
 * Initialize UART2 for Nextion display communication.
 * TX=GPIO17, RX=GPIO16, 9600 baud.
 */
esp_err_t nextion_init(void);

/**
 * Send a raw command string to the Nextion display.
 * Automatically appends the 0xFF 0xFF 0xFF terminator.
 */
esp_err_t nextion_send_cmd(const char *cmd);

/**
 * Update a numeric value on the display.
 * Example: nextion_set_value("t0", 1234) sends "t0.val=1234"
 */
esp_err_t nextion_set_value(const char *component, int32_t value);

/**
 * Update a text field on the display.
 * Example: nextion_set_text("t0", "Hello") sends "t0.txt=\"Hello\""
 */
esp_err_t nextion_set_text(const char *component, const char *text);

/**
 * Change the active page on the display.
 */
esp_err_t nextion_set_page(uint8_t page);

/**
 * Register a callback for touch events from the display.
 */
void nextion_set_touch_callback(nextion_touch_cb_t cb);

/**
 * Start the background task that listens for events from Nextion.
 * Must be called after nextion_init().
 */
esp_err_t nextion_start_listener(void);
