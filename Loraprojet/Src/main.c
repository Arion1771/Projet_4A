#include "main.h"
#include "radio.h"
#include "subghz.h"
#include "timer.h"
#include <string.h>
#include <stdio.h>

#define RF_FREQUENCY_HZ        868000000U
#define TX_OUTPUT_POWER_DBM    14
#define LORA_BANDWIDTH         0
#define LORA_SPREADING_FACTOR  7
#define LORA_CODINGRATE        1
#define LORA_PREAMBLE_LENGTH   8
#define LORA_SYMBOL_TIMEOUT    5
#define RX_TIMEOUT_MS          3000U
#define TX_TIMEOUT_MS          3000U
#define APP_TX_PERIOD_MS       2000U
#define FRAME_VERSION          1U
#define MSG_TEXT               1U
#define NODE_ID_E5             0xE501U
#define NODE_ID_WYRES          0x0201U
#define MAX_FRAME_PAYLOAD      64U
#define DEBUG_LED1_PORT        GPIOB
#define DEBUG_LED1_PIN         GPIO_PIN_5
#define DEBUG_LED2_PORT        GPIOA
#define DEBUG_LED2_PIN         GPIO_PIN_5

typedef struct
{
	uint8_t version;
	uint8_t type;
	uint16_t src_id;
	uint16_t dst_id;
	uint16_t seq;
	uint8_t ttl;
	uint8_t flags;
	uint8_t payload_len;
	uint8_t payload[MAX_FRAME_PAYLOAD];
} AppFrame_t;

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
static int16_t rx_rssi = 0;
static int8_t rx_snr = 0;
static uint16_t tx_seq = 1U;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void Radio_Init(void);

static void OnTxDone(void);
static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
static void OnTxTimeout(void);
static void OnRxTimeout(void);
static void OnRxError(void);

static void Uart_Log(const char *msg);
static void Uart_LogHex(const uint8_t *data, uint16_t size);
static void Uart_LogText(const uint8_t *data, uint16_t size);
static void Uart_TransmitAll(const uint8_t *buf, uint16_t len);
static void LedFlashPattern(uint8_t flashes);
static void DebugLedWrite(GPIO_PinState state);
static uint8_t BuildTextFrame(AppFrame_t *frame, const uint8_t *payload, uint8_t size);

int main(void)
{
	uint32_t last_tx_tick = 0;
	uint32_t last_alive_tick = 0;
	const uint8_t tx_payload[] = "PING";
	AppFrame_t tx_frame;
	uint8_t tx_len = 0;

	HAL_Init();
	SystemClock_Config();

	MX_GPIO_Init();
	MX_USART1_UART_Init();
	MX_SUBGHZ_Init();
	Uart_Log("BOOT LORAPROJET\r\n");

	Radio_Init();
	Radio.Rx(0);

	while (1)
	{
		TimerProcess();

		if (radio_rx_done != 0U)
		{
			AppFrame_t frame;
			radio_rx_done = 0U;

			if ((rx_size >= 11U) && (rx_buffer[0] == FRAME_VERSION))
			{
				memset(&frame, 0, sizeof(frame));
				if (rx_size > sizeof(frame))
				{
					rx_size = sizeof(frame);
				}
				memcpy(&frame, rx_buffer, rx_size);

				Uart_Log("RX FRAME TEXT: ");
				Uart_LogText(frame.payload, frame.payload_len);
				Uart_Log("\r\n");

				if ((frame.payload_len == 4U) && (memcmp(frame.payload, "PONG", 4U) == 0))
				{
					/* Link confirmation pattern when Wyres answers E5 ping. */
					LedFlashPattern(4U);
					Uart_Log("LINK OK (PONG)\r\n");
				}
			}
			else
			{
				Uart_Log("RX RAW: ");
				Uart_LogText(rx_buffer, rx_size);
				Uart_Log(" | HEX: ");
				Uart_LogHex(rx_buffer, rx_size);
				Uart_Log("\r\n");
			}

			LedFlashPattern(2U);
			Radio.Rx(0);
		}

		if (radio_tx_done != 0U)
		{
			radio_tx_done = 0U;
			LedFlashPattern(1U);
			Uart_Log("TX DONE\r\n");
			Radio.Rx(0);
		}

		if (radio_rx_timeout != 0U)
		{
			radio_rx_timeout = 0U;
			LedFlashPattern(3U);
			Uart_Log("RX TIMEOUT\r\n");
			Radio.Rx(0);
		}

		if (radio_rx_error != 0U)
		{
			radio_rx_error = 0U;
			LedFlashPattern(3U);
			Uart_Log("RX ERROR\r\n");
			Radio.Rx(0);
		}

		if ((HAL_GetTick() - last_tx_tick) >= APP_TX_PERIOD_MS)
		{
			RadioState_t state = Radio.GetStatus();
			if (state != RF_TX_RUNNING)
			{
				if (state == RF_RX_RUNNING)
				{
					Radio.Standby();
				}
				last_tx_tick = HAL_GetTick();
				tx_len = BuildTextFrame(&tx_frame, tx_payload, (uint8_t)strlen((const char *)tx_payload));
				Radio.Send((uint8_t *)&tx_frame, tx_len);
			}
		}

		if ((HAL_GetTick() - last_alive_tick) >= 2000U)
		{
			last_alive_tick = HAL_GetTick();
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
	if (size > sizeof(rx_buffer))
	{
		size = sizeof(rx_buffer);
	}

	memcpy(rx_buffer, payload, size);
	rx_size = size;
	rx_rssi = rssi;
	rx_snr = snr;
	radio_rx_done = 1U;
}

static void OnTxTimeout(void)
{
	radio_tx_done = 1U;
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

static void Uart_LogHex(const uint8_t *data, uint16_t size)
{
	static const char hex[] = "0123456789ABCDEF";
	char out[3];
	uint16_t i = 0;

	out[2] = '\0';
	for (i = 0; i < size; i++)
	{
		out[0] = hex[(data[i] >> 4) & 0x0F];
		out[1] = hex[data[i] & 0x0F];
		Uart_TransmitAll((const uint8_t *)out, 2U);
	}
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
		(void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 50U);
	}

	if (uart2_ready != 0U)
	{
		(void)HAL_UART_Transmit(&huart2, (uint8_t *)buf, len, 50U);
	}
}

static void LedFlashPattern(uint8_t flashes)
{
	uint8_t i;

	for (i = 0U; i < flashes; i++)
	{
		DebugLedWrite(GPIO_PIN_SET);
		HAL_Delay(60U);
		DebugLedWrite(GPIO_PIN_RESET);
		HAL_Delay(90U);
	}
}

static void DebugLedWrite(GPIO_PinState state)
{
	HAL_GPIO_WritePin(DEBUG_LED1_PORT, DEBUG_LED1_PIN, state);
	HAL_GPIO_WritePin(DEBUG_LED2_PORT, DEBUG_LED2_PIN, state);
}

static uint8_t BuildTextFrame(AppFrame_t *frame, const uint8_t *payload, uint8_t size)
{
	if (size > MAX_FRAME_PAYLOAD)
	{
		size = MAX_FRAME_PAYLOAD;
	}

	memset(frame, 0, sizeof(*frame));
	frame->version = FRAME_VERSION;
	frame->type = MSG_TEXT;
	frame->src_id = NODE_ID_E5;
	frame->dst_id = NODE_ID_WYRES;
	frame->seq = tx_seq++;
	frame->ttl = 8U;
	frame->flags = 0U;
	frame->payload_len = size;
	if (size > 0U)
	{
		memcpy(frame->payload, payload, size);
	}

	return (uint8_t)(11U + size);
}

static void MX_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	GPIO_InitStruct.Pin = DEBUG_LED1_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(DEBUG_LED1_PORT, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = DEBUG_LED2_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(DEBUG_LED2_PORT, &GPIO_InitStruct);

	DebugLedWrite(GPIO_PIN_RESET);
}

static void MX_USART1_UART_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* USART2 on PA2/PA3 (NUCLEO ST-LINK VCP common path). */
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

	/* USART1 on PB6/PB7 (alternative wiring path). */
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

	/* No UART available: keep firmware running and rely on LED/radio behavior. */
}

static void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	HAL_PWR_EnableBkUpAccess();
	__HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE | RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.LSEState = RCC_LSE_ON;
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;
	RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_11;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK3 | RCC_CLOCKTYPE_HCLK
															| RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1
															| RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.AHBCLK3Divider = RCC_SYSCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
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
