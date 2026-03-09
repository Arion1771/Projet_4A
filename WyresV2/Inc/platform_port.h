#ifndef PLATFORM_PORT_H
#define PLATFORM_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Directions in the linear chain */
typedef enum {
    LINK_PREDECESSOR = 0,
    LINK_SUCCESSOR = 1
} link_direction_t;

typedef enum {
    LED_OFF = 0,
    LED_NEUTRAL,
    LED_INSERTED,
    LED_ALERT
} led_state_t;

typedef void (*platform_radio_rx_cb_t)(link_direction_t from, const uint8_t *data, uint16_t len, int16_t rssi, int8_t snr);

/* Time base in milliseconds */
uint32_t platform_millis(void);

/* Radio primitives (to implement with WyresV2 LoRa driver) */
bool platform_radio_init(platform_radio_rx_cb_t cb);
void platform_radio_process(void);
bool platform_radio_send(link_direction_t dir, const uint8_t *data, uint16_t len);

/* LED and local indicators */
void platform_led_set(led_state_t state);

/* Battery in millivolts */
uint16_t platform_battery_mv(void);

#endif
