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
#define DEBUG_LED1_PORT GPIOB
#define DEBUG_LED1_PIN  GPIO_PIN_5
#define DEBUG_LED2_PORT GPIOA
#define DEBUG_LED2_PIN  GPIO_PIN_5
#define LED_ACTIVE_LOW  1U

static RadioEvents_t s_radio_events;
static platform_radio_rx_cb_t s_rx_cb = NULL;
static volatile uint8_t s_radio_rx_done = 0;
static volatile uint8_t s_radio_tx_done = 0;
static volatile uint8_t s_radio_tx_timeout = 0;
static volatile uint8_t s_radio_rx_timeout = 0;
static volatile uint8_t s_radio_rx_error = 0;
static uint8_t s_rx_buffer[255];
static uint16_t s_rx_len = 0;
static int16_t s_rx_rssi = 0;
static int8_t s_rx_snr = 0;
static uint8_t s_radio_inited = 0;

static void DebugLedOn(void)
{
    HAL_GPIO_WritePin(DEBUG_LED1_PORT, DEBUG_LED1_PIN, (LED_ACTIVE_LOW != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(DEBUG_LED2_PORT, DEBUG_LED2_PIN, (LED_ACTIVE_LOW != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void DebugLedOff(void)
{
    HAL_GPIO_WritePin(DEBUG_LED1_PORT, DEBUG_LED1_PIN, (LED_ACTIVE_LOW != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DEBUG_LED2_PORT, DEBUG_LED2_PIN, (LED_ACTIVE_LOW != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void LedFlashPattern(uint8_t flashes)
{
    uint8_t i;
    for (i = 0U; i < flashes; i++) {
        DebugLedOn();
        HAL_Delay(60U);
        DebugLedOff();
        HAL_Delay(90U);
    }
}

static void OnTxDone(void)
{
    s_radio_tx_done = 1U;
}

static void OnTxTimeout(void)
{
    s_radio_tx_timeout = 1U;
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
    RadioState_t state;

    TimerProcess();
    Radio.IrqProcess();

    if (s_radio_rx_done != 0U) {
        s_radio_rx_done = 0U;
        if (s_rx_cb != NULL) {
            /* Direction resolution requires network context; default for now. */
            s_rx_cb(LINK_PREDECESSOR, s_rx_buffer, s_rx_len, s_rx_rssi, s_rx_snr);
        }

        /* If callback triggered relay TX, do not interrupt it by forcing RX. */
        state = Radio.GetStatus();
        if (state != RF_TX_RUNNING) {
            Radio.Rx(0);
        }
    }

    if (s_radio_tx_done != 0U) {
        s_radio_tx_done = 0U;
        Radio.Rx(0);
    }

    if ((s_radio_tx_timeout != 0U) || (s_radio_rx_timeout != 0U) || (s_radio_rx_error != 0U)) {
        s_radio_tx_timeout = 0U;
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
    (void)state;
    /* Debug mode: keep LED off at idle; only flash patterns indicate events. */
    DebugLedOff();
}

uint16_t platform_battery_mv(void)
{
    return 3900;
}
