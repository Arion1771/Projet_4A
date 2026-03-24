#include "radio_board_if.h"
#include "main.h"

static void RBI_SetRfSwitch(uint8_t ctrl1, uint8_t ctrl2)
{
  HAL_GPIO_WritePin(RF_CTRL1_PORT, RF_CTRL1_PIN, ctrl1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(RF_CTRL2_PORT, RF_CTRL2_PIN, ctrl2 ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

int32_t RBI_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitStruct.Pin = RF_CTRL1_PIN | RF_CTRL2_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(RF_CTRL1_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = TCXO_PWR_PIN;
  HAL_GPIO_Init(TCXO_PWR_PORT, &GPIO_InitStruct);

  HAL_GPIO_WritePin(TCXO_PWR_PORT, TCXO_PWR_PIN, GPIO_PIN_SET);
  RBI_SetRfSwitch(0, 0);

  return 0;
}

int32_t RBI_DeInit(void)
{
  HAL_GPIO_WritePin(TCXO_PWR_PORT, TCXO_PWR_PIN, GPIO_PIN_RESET);
  RBI_SetRfSwitch(0, 0);
  return 0;
}

int32_t RBI_ConfigRFSwitch(RBI_Switch_TypeDef Config)
{
  switch (Config)
  {
    case RBI_SWITCH_RX:
#if (RBI_RF_SW_PROFILE == RBI_RF_SW_PROFILE_WYRES_REVC)
      RBI_SetRfSwitch(1, 1);
#else
      /* LoRa-E5 devkit profile */
      RBI_SetRfSwitch(1, 0);
#endif
      break;
    case RBI_SWITCH_RFO_LP:
    case RBI_SWITCH_RFO_HP:
#if (RBI_RF_SW_PROFILE == RBI_RF_SW_PROFILE_WYRES_REVC)
      RBI_SetRfSwitch(0, 1);
#else
      /* LoRa-E5 devkit profile */
      RBI_SetRfSwitch(0, 1);
#endif
      break;
    case RBI_SWITCH_OFF:
    default:
      RBI_SetRfSwitch(0, 0);
      break;
  }
  return 0;
}

int32_t RBI_GetTxConfig(void)
{
  return RBI_CONF_RFO;
}

int32_t RBI_IsTCXO(void)
{
  return IS_TCXO_SUPPORTED;
}

int32_t RBI_IsDCDC(void)
{
  return IS_DCDC_SUPPORTED;
}

int32_t RBI_GetRFOMaxPowerConfig(RBI_RFOMaxPowerConfig_TypeDef Config)
{
  if (Config == RBI_RFO_HP_MAXPOWER)
  {
    return 22;
  }
  return 15;
}
