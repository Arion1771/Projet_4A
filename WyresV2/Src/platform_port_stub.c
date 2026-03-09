#include "platform_port.h"
#include "main.h"
#include "radio.h"
#include "subghz.h"
#include "timer.h"
#include "stm32wlxx_hal.h"

#include <string.h>

/*
 * HAL-backed radio implementation using STM32WL SubGHz_Phy middleware.
 */

#define WYRESV2_RF_FREQUENCY_HZ 868000000U
#define WYRESV2_TX_POWER_DBM 14
#define WYRESV2_LORA_BW 0
#define WYRESV2_LORA_SF 7
#define WYRESV2_LORA_CR 1
#define WYRESV2_LORA_PREAMBLE 8
#define WYRESV2_LORA_SYMBOL_TIMEOUT 5
#define WYRESV2_TX_TIMEOUT_MS 3000

static RadioEvents_t s_radio_events;
static platform_radio_rx_cb_t s_rx_cb = NULL;
static volatile uint8_t s_radio_rx_done = 0;
static volatile uint8_t s_radio_tx_done = 0;
static volatile uint8_t s_radio_rx_timeout = 0;
static volatile uint8_t s_radio_rx_error = 0;
static uint8_t s_rx_buffer[255];
static uint16_t s_rx_len = 0;
static int16_t s_rx_rssi = 0;
static int8_t s_rx_snr = 0;
static uint8_t s_radio_inited = 0;

static void OnTxDone(void)
{
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    s_radio_tx_done = 1U;
}

static void OnTxTimeout(void)
{
    s_radio_tx_done = 1U;
}

static void OnRxTimeout(void)
{
    s_radio_rx_timeout = 1U;
}

static void OnRxError(void)
{
    s_radio_rx_error = 1U;
}

static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    if (size > sizeof(s_rx_buffer)) {
        size = sizeof(s_rx_buffer);
    }

    memcpy(s_rx_buffer, payload, size);
    s_rx_len = size;
    s_rx_rssi = rssi;
    s_rx_snr = snr;
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    s_radio_rx_done = 1U;
}

bool platform_radio_init(platform_radio_rx_cb_t cb)
{
    s_rx_cb = cb;

    if (s_radio_inited != 0U) {
        return true;
    }

    MX_SUBGHZ_Init();

    memset(&s_radio_events, 0, sizeof(s_radio_events));
    s_radio_events.TxDone = OnTxDone;
    s_radio_events.RxDone = OnRxDone;
    s_radio_events.TxTimeout = OnTxTimeout;
    s_radio_events.RxTimeout = OnRxTimeout;
    s_radio_events.RxError = OnRxError;

    Radio.Init(&s_radio_events);
    Radio.SetChannel(WYRESV2_RF_FREQUENCY_HZ);

    Radio.SetTxConfig(MODEM_LORA,
                      WYRESV2_TX_POWER_DBM,
                      0,
                      WYRESV2_LORA_BW,
                      WYRESV2_LORA_SF,
                      WYRESV2_LORA_CR,
                      WYRESV2_LORA_PREAMBLE,
                      false,
                      true,
                      0,
                      0,
                      false,
                      WYRESV2_TX_TIMEOUT_MS);

    Radio.SetRxConfig(MODEM_LORA,
                      WYRESV2_LORA_BW,
                      WYRESV2_LORA_SF,
                      WYRESV2_LORA_CR,
                      0,
                      WYRESV2_LORA_PREAMBLE,
                      WYRESV2_LORA_SYMBOL_TIMEOUT,
                      false,
                      0,
                      true,
                      0,
                      0,
                      false,
                      true);

    Radio.Rx(0);
    s_radio_inited = 1U;
    return true;
}

void platform_radio_process(void)
{
    TimerProcess();

    if (s_radio_rx_done != 0U) {
        s_radio_rx_done = 0U;
        if (s_rx_cb != NULL) {
            /* Direction resolution requires network context; default for now. */
            s_rx_cb(LINK_PREDECESSOR, s_rx_buffer, s_rx_len, s_rx_rssi, s_rx_snr);
        }
        Radio.Rx(0);
    }

    if ((s_radio_tx_done != 0U) || (s_radio_rx_timeout != 0U) || (s_radio_rx_error != 0U)) {
        s_radio_tx_done = 0U;
        s_radio_rx_timeout = 0U;
        s_radio_rx_error = 0U;
        Radio.Rx(0);
    }
}

uint32_t platform_millis(void)
{
    return HAL_GetTick();
}

bool platform_radio_send(link_direction_t dir, const uint8_t *data, uint16_t len)
{
    RadioState_t state;

    (void)dir;
    if ((s_radio_inited == 0U) || (data == NULL) || (len == 0U)) {
        return false;
    }

    state = Radio.GetStatus();
    if (state == RF_TX_RUNNING) {
        return false;
    }

    if (state == RF_RX_RUNNING) {
        Radio.Standby();
    }

    Radio.Send((uint8_t *)data, (uint8_t)len);
    return true;
}

void platform_led_set(led_state_t state)
{
    switch (state) {
        case LED_OFF:
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
            break;
        case LED_NEUTRAL:
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
            break;
        case LED_INSERTED:
            /* Single LED fallback: ON indicates active insertion. */
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
            break;
        case LED_ALERT:
            /* Single LED fallback: keep ON for alert state. */
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
            break;
        default:
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
            break;
    }
}

uint16_t platform_battery_mv(void)
{
    return 3900;
}
