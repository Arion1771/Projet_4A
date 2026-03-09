#ifndef RADIO_CONF_H
#define RADIO_CONF_H

#include <string.h>

#include "main.h"
#include "radio_board_if.h"
#include "subghz.h"

#define SMPS_DRIVE_SETTING_DEFAULT  SMPS_DRV_40
#define SMPS_DRIVE_SETTING_MAX      SMPS_DRV_60
#define XTAL_FREQ                   32000000UL
#define XTAL_DEFAULT_CAP_VALUE      0x20UL
#define TCXO_CTRL_VOLTAGE           TCXO_CTRL_1_7V
#define RF_WAKEUP_TIME              1UL
#define DCDC_ENABLE                 1UL

#define RADIO_LR_FHSS_IS_ON 0
#define RFW_ENABLE 0
#define RFW_LONGPACKET_ENABLE 0

#define RADIO_INIT MX_SUBGHZ_Init
#define RADIO_DELAY_MS HAL_Delay

#define RADIO_MEMSET8(dest, value, size) memset((dest), (value), (size))
#define RADIO_MEMCPY8(dest, src, size) memcpy((dest), (src), (size))

#define CRITICAL_SECTION_BEGIN() __disable_irq()
#define CRITICAL_SECTION_END() __enable_irq()

#define DBG_GPIO_RADIO_RX(set_rst)
#define DBG_GPIO_RADIO_TX(set_rst)

#endif
