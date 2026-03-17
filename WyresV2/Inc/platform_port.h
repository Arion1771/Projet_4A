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
uint8_t platform_radio_version(void);
uint8_t platform_radio_profile(void);
uint8_t platform_radio_probe_count(void);
uint8_t platform_radio_probe_profile(uint8_t idx);
uint8_t platform_radio_probe_version(uint8_t idx);
uint8_t platform_radio_probe_af(uint8_t idx);
uint32_t platform_radio_dbg_rx_done_count(void);
uint32_t platform_radio_dbg_crc_err_count(void);
uint32_t platform_radio_dbg_rx_timeout_count(void);
uint32_t platform_radio_dbg_rearm_count(void);
uint32_t platform_radio_dbg_hard_reset_count(void);
uint8_t platform_radio_dbg_last_irq_flags(void);
uint8_t platform_radio_dbg_last_opmode(void);

/* LED and local indicators */
void platform_led_set(led_state_t state);

/* Battery in millivolts */
uint16_t platform_battery_mv(void);

#endif
