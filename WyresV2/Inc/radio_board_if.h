#ifndef RADIO_BOARD_IF_H
#define RADIO_BOARD_IF_H

#include "platform.h"

#define RBI_CONF_RFO_LP_HP 0
#define RBI_CONF_RFO_LP 1
#define RBI_CONF_RFO_HP 2

#define RBI_CONF_RFO RBI_CONF_RFO_HP

#define IS_TCXO_SUPPORTED 1U
#define IS_DCDC_SUPPORTED 1U

#define RF_CTRL1_PORT GPIOA
#define RF_CTRL1_PIN GPIO_PIN_4
#define RF_CTRL2_PORT GPIOA
#define RF_CTRL2_PIN GPIO_PIN_5

#define TCXO_PWR_PORT GPIOB
#define TCXO_PWR_PIN GPIO_PIN_0

typedef enum {
    RBI_SWITCH_OFF = 0,
    RBI_SWITCH_RX = 1,
    RBI_SWITCH_RFO_LP = 2,
    RBI_SWITCH_RFO_HP = 3,
} RBI_Switch_TypeDef;

typedef enum {
    RBI_RFO_LP_MAXPOWER = 0,
    RBI_RFO_HP_MAXPOWER = 1,
} RBI_RFOMaxPowerConfig_TypeDef;

int32_t RBI_Init(void);
int32_t RBI_DeInit(void);
int32_t RBI_ConfigRFSwitch(RBI_Switch_TypeDef Config);
int32_t RBI_GetTxConfig(void);
int32_t RBI_IsTCXO(void);
int32_t RBI_IsDCDC(void);
int32_t RBI_GetRFOMaxPowerConfig(RBI_RFOMaxPowerConfig_TypeDef Config);

#endif
