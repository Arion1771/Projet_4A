#include "main.h"
#include "stm32wlxx_hal.h"
#include "platform_port.h"

#include <stdio.h>
#include <string.h>

#ifndef UART_ONE_BIT_SAMPLE_DISABLE
#define UART_ONE_BIT_SAMPLE_DISABLE UART_ONE_BIT_SAMPLE_DISABLED
#endif

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART_UART_Init(void);
static void OnRadioRx(link_direction_t from, const uint8_t *data, uint16_t len, int16_t rssi, int8_t snr);
static void BootBlink(void);

static UART_HandleTypeDef huart1;
static UART_HandleTypeDef huart2;
static uint8_t uart1_ready = 0U;
static uint8_t uart2_ready = 0U;

static void Uart_Log(const char *msg);
static void Uart_LogText(const uint8_t *data, uint16_t size);
static void Uart_TransmitAll(const uint8_t *buf, uint16_t len);

int main(void)
{
    uint32_t last_heartbeat_ms = 0U;
    uint32_t last_ping_ms = 0U;
    uint32_t ping_counter = 0U;
    char ping_msg[40];
    bool ping_ok;

    HAL_Init();
    MX_GPIO_Init();
    BootBlink();
    SystemClock_Config();
    MX_USART_UART_Init();

    Uart_Log("BOOT WYRESV2\r\n");

    if (!platform_radio_init(OnRadioRx)) {
        Uart_Log("RADIO INIT ERROR\r\n");
        Error_Handler();
    }

    Uart_Log("RADIO INIT OK\r\n");

    for (;;) {
        if ((HAL_GetTick() - last_heartbeat_ms) >= 500U) {
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
            last_heartbeat_ms = HAL_GetTick();
            Uart_Log("WYRES ALIVE\r\n");
        }

        if ((HAL_GetTick() - last_ping_ms) >= 2000U) {
            (void)snprintf(ping_msg, sizeof(ping_msg), "WYRES PING %lu", (unsigned long)ping_counter++);
            ping_ok = platform_radio_send(LINK_SUCCESSOR, (const uint8_t *)ping_msg, (uint16_t)strlen(ping_msg));
            if (ping_ok) {
                Uart_Log("PING TX: ");
                Uart_Log(ping_msg);
                Uart_Log("\r\n");
            } else {
                Uart_Log("PING TX FAIL\r\n");
            }
            last_ping_ms = HAL_GetTick();
        }

        platform_radio_process();
        HAL_Delay(2U);
    }
}

static void OnRadioRx(link_direction_t from, const uint8_t *data, uint16_t len, int16_t rssi, int8_t snr)
{
    bool relay_ok;
    uint8_t tries;
    char hdr[96];

    (void)from;

    if ((data == NULL) || (len == 0U)) {
        Uart_Log("RX EMPTY\r\n");
        return;
    }

    (void)snprintf(hdr, sizeof(hdr), "RX len=%u rssi=%d snr=%d: ", (unsigned)len, (int)rssi, (int)snr);
    Uart_Log(hdr);
    Uart_LogText(data, len);
    Uart_Log("\r\n");

    /* Visual confirmation: toggle LED on each received radio payload. */
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);

    /*
     * Give the sender a short guard time to switch back to RX after TX_DONE.
     * Without this, immediate relay can be transmitted too early and be missed.
     */
    HAL_Delay(80U);

    relay_ok = false;
    for (tries = 0U; tries < 3U; tries++) {
        relay_ok = platform_radio_send(LINK_SUCCESSOR, data, len);
        if (relay_ok) {
            break;
        }
        HAL_Delay(20U);
    }

    if (relay_ok) {
        Uart_Log("RELAY TX START\r\n");
    } else {
        Uart_Log("RELAY TX FAIL\r\n");
    }
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

static void BootBlink(void)
{
    uint8_t i;

    for (i = 0U; i < 3U; i++) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
        HAL_Delay(100U);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
        HAL_Delay(100U);
    }
}

static void MX_USART_UART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart2) == HAL_OK) {
        uart2_ready = 1U;
    }

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) == HAL_OK) {
        uart1_ready = 1U;
    }
}

static void Uart_Log(const char *msg)
{
    if (msg == NULL) {
        return;
    }

    Uart_TransmitAll((const uint8_t *)msg, (uint16_t)strlen(msg));
}

static void Uart_LogText(const uint8_t *data, uint16_t size)
{
    uint16_t i;
    uint8_t c;

    for (i = 0; i < size; i++) {
        c = data[i];
        if ((c < 32U) || (c > 126U)) {
            c = '.';
        }
        Uart_TransmitAll(&c, 1U);
    }
}

static void Uart_TransmitAll(const uint8_t *buf, uint16_t len)
{
    if ((buf == NULL) || (len == 0U)) {
        return;
    }

    if (uart1_ready != 0U) {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 100U);
    }

    if (uart2_ready != 0U) {
        (void)HAL_UART_Transmit(&huart2, (uint8_t *)buf, len, 100U);
    }
}

void Error_Handler(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    volatile uint32_t d;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    while (1) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
        for (d = 0U; d < 250000U; d++) {
            __NOP();
        }
    }
}
