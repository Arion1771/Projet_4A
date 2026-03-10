#include "main.h"
#include "radio.h"
#include "subghz.h"
#include "timer.h"

#include <stdio.h>
#include <string.h>

#define RF_FREQUENCY_HZ        868000000U
#define TX_OUTPUT_POWER_DBM    14
#define LORA_BANDWIDTH         0
#define LORA_SPREADING_FACTOR  7
#define LORA_CODINGRATE        1
#define LORA_PREAMBLE_LENGTH   8
#define LORA_SYMBOL_TIMEOUT    5
#define TX_TIMEOUT_MS          3000U
#define APP_TX_PERIOD_MS       2000U

static UART_HandleTypeDef huart1;
static UART_HandleTypeDef huart2;
static uint8_t uart1_ready = 0U;
static uint8_t uart2_ready = 0U;

static RadioEvents_t radio_events;

static volatile uint8_t radio_tx_done = 0;
static volatile uint8_t radio_rx_done = 0;
static volatile uint8_t radio_rx_timeout = 0;
static volatile uint8_t radio_rx_error = 0;

static uint8_t rx_buffer[255];
static uint16_t rx_size = 0;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART_UART_Init(void);
static void Radio_Init(void);

static void OnTxDone(void);
static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
static void OnTxTimeout(void);
static void OnRxTimeout(void);
static void OnRxError(void);

static void Uart_Log(const char *msg);
static void Uart_LogText(const uint8_t *data, uint16_t size);
static void Uart_TransmitAll(const uint8_t *buf, uint16_t len);

int main(void)
{
    uint32_t loop_counter = 0U;
    uint32_t last_tx_loop = 0U;
    uint32_t tx_counter = 0U;
    uint32_t alive_counter = 0U;
    char tx_msg[40];

    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART_UART_Init();
    MX_SUBGHZ_Init();

    Uart_Log("BOOT LORAPROJET\r\n");

    Radio_Init();
    Radio.Rx(0);

    while (1)
    {
        TimerProcess();

        if (radio_rx_done != 0U)
        {
            radio_rx_done = 0U;

            Uart_Log("RX: ");
            Uart_LogText(rx_buffer, rx_size);
            Uart_Log("\r\n");

            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            Radio.Rx(0);
        }

        if (radio_tx_done != 0U)
        {
            radio_tx_done = 0U;
            Uart_Log("TX DONE\r\n");
            Radio.Rx(0);
        }

        if (radio_rx_timeout != 0U)
        {
            radio_rx_timeout = 0U;
            Uart_Log("RX TIMEOUT\r\n");
            Radio.Rx(0);
        }

        if (radio_rx_error != 0U)
        {
            radio_rx_error = 0U;
            Uart_Log("RX ERROR\r\n");
            Radio.Rx(0);
        }

        loop_counter++;

        if ((loop_counter - last_tx_loop) >= 20000U)
        {
            /* Force a fresh TX cycle without relying on pending IRQ processing. */
            Radio.Standby();
            (void)snprintf(tx_msg, sizeof(tx_msg), "MSG %lu", (unsigned long)tx_counter++);
            Radio.Send((uint8_t *)tx_msg, (uint8_t)strlen(tx_msg));
            Radio.Rx(0);

            Uart_Log("TX: ");
            Uart_Log(tx_msg);
            Uart_Log("\r\n");

            last_tx_loop = loop_counter;
        }

        alive_counter++;
        if (alive_counter >= 80000U)
        {
            alive_counter = 0U;
            Uart_Log("ALIVE\r\n");
        }
    }
}

static void Radio_Init(void)
{
    radio_events.TxDone = OnTxDone;
    radio_events.RxDone = OnRxDone;
    radio_events.TxTimeout = OnTxTimeout;
    radio_events.RxTimeout = OnRxTimeout;
    radio_events.RxError = OnRxError;

    Radio.Init(&radio_events);
    Radio.SetChannel(RF_FREQUENCY_HZ);

    Radio.SetTxConfig(MODEM_LORA,
                      TX_OUTPUT_POWER_DBM,
                      0,
                      LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR,
                      LORA_CODINGRATE,
                      LORA_PREAMBLE_LENGTH,
                      false,
                      true,
                      0,
                      0,
                      false,
                      TX_TIMEOUT_MS);

    Radio.SetRxConfig(MODEM_LORA,
                      LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR,
                      LORA_CODINGRATE,
                      0,
                      LORA_PREAMBLE_LENGTH,
                      LORA_SYMBOL_TIMEOUT,
                      false,
                      0,
                      true,
                      0,
                      0,
                      false,
                      true);
}

static void OnTxDone(void)
{
    radio_tx_done = 1U;
}

static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    (void)rssi;
    (void)snr;

    if (size > sizeof(rx_buffer))
    {
        size = sizeof(rx_buffer);
    }

    memcpy(rx_buffer, payload, size);
    rx_size = size;
    radio_rx_done = 1U;
}

static void OnTxTimeout(void)
{
    radio_rx_timeout = 1U;
}

static void OnRxTimeout(void)
{
    radio_rx_timeout = 1U;
}

static void OnRxError(void)
{
    radio_rx_error = 1U;
}

static void Uart_Log(const char *msg)
{
    if (msg == NULL)
    {
        return;
    }

    Uart_TransmitAll((const uint8_t *)msg, (uint16_t)strlen(msg));
}

static void Uart_LogText(const uint8_t *data, uint16_t size)
{
    uint16_t i;
    uint8_t c;

    for (i = 0; i < size; i++)
    {
        c = data[i];
        if ((c < 32U) || (c > 126U))
        {
            c = '.';
        }
        Uart_TransmitAll(&c, 1U);
    }
}

static void Uart_TransmitAll(const uint8_t *buf, uint16_t len)
{
    if ((buf == NULL) || (len == 0U))
    {
        return;
    }

    if (uart1_ready != 0U)
    {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 100U);
    }

    if (uart2_ready != 0U)
    {
        (void)HAL_UART_Transmit(&huart2, (uint8_t *)buf, len, 100U);
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
    if (HAL_UART_Init(&huart2) == HAL_OK)
    {
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
    if (HAL_UART_Init(&huart1) == HAL_OK)
    {
        uart1_ready = 1U;
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
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, LL_FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
