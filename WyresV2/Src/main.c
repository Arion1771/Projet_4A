#include "main.h"
#include "stm32wlxx_hal.h"
#include "platform_port.h"

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void OnRadioRx(link_direction_t from, const uint8_t *data, uint16_t len, int16_t rssi, int8_t snr);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    if (!platform_radio_init(OnRadioRx)) {
        Error_Handler();
    }

    for (;;) {
        platform_radio_process();
        HAL_Delay(2U);
    }
}

static void OnRadioRx(link_direction_t from, const uint8_t *data, uint16_t len, int16_t rssi, int8_t snr)
{
    (void)from;
    (void)rssi;
    (void)snr;

    if ((data == NULL) || (len == 0U)) {
        return;
    }

    /* Echo relay: retransmit exactly what was received. */
    (void)platform_radio_send(LINK_SUCCESSOR, data, len);
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

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}
