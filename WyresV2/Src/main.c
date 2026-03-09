#include "main.h"
#include "stm32wlxx_hal.h"
#include "wyresv2_node.h"

#include <string.h>

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void OnRadioRx(link_direction_t from, const uint8_t *data, uint16_t len, int16_t rssi, int8_t snr);

static wyresv2_ctx_t *s_ctx = NULL;

int main(void)
{
    wyresv2_ctx_t ctx;
    uint32_t last_tx_ms = 0U;
    const uint8_t ping_msg[] = "WYRES->E5";

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    if (!platform_radio_init(OnRadioRx)) {
        Error_Handler();
    }

    wyresv2_init(&ctx, 0x0201);
    wyresv2_power_on(&ctx);
    s_ctx = &ctx;

    for (;;) {
        platform_radio_process();
        wyresv2_tick(&ctx, HAL_GetTick());

        if ((HAL_GetTick() - last_tx_ms) >= 7000U) {
            last_tx_ms = HAL_GetTick();
            (void)wyresv2_send_text(&ctx, 0xE501U, ping_msg, (uint8_t)(sizeof(ping_msg) - 1U));
        }

        HAL_Delay(10);
    }
}

static void OnRadioRx(link_direction_t from, const uint8_t *data, uint16_t len, int16_t rssi, int8_t snr)
{
    wyresv2_frame_t frame;

    if ((s_ctx == NULL) || (data == NULL) || (len == 0U)) {
        return;
    }

    memset(&frame, 0, sizeof(frame));

    /* Interop: accept both native framed packets and legacy raw text payloads. */
    if ((len >= 11U) && (data[0] == 1U)) {
        if (len > sizeof(frame)) {
            len = sizeof(frame);
        }
        memcpy(&frame, data, len);
    } else {
        frame.version = 1U;
        frame.type = MSG_TEXT;
        frame.src_id = 0xE501U;
        frame.dst_id = s_ctx->node_id;
        frame.seq = 0U;
        frame.ttl = 1U;
        frame.flags = 0U;
        if (len > WYRESV2_MAX_PAYLOAD) {
            len = WYRESV2_MAX_PAYLOAD;
        }
        frame.payload_len = (uint8_t)len;
        memcpy(frame.payload, data, len);
    }

    wyresv2_on_frame(s_ctx, from, &frame, rssi, snr);
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, LL_FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}
